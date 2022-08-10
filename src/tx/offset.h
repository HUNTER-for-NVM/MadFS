#pragma once

#include <atomic>
#include <cstdint>
#include <iosfwd>

#include "block/block.h"
#include "const.h"
#include "idx.h"
#include "tx/cursor.h"

namespace ulayfs::dram {

class File;
class TxMgr;
struct TxCursor;

class OffsetMgr {
  union TicketSlot {
    struct {
      std::atomic_uint64_t ticket;
      TxCursor cursor;
    } ticket_slot;
    char cl[CACHELINE_SIZE];

    TicketSlot() { ticket_slot.ticket.store(0, std::memory_order_relaxed); }
  };

  TxMgr* tx_mgr;
  uint64_t offset;
  uint64_t next_ticket;
  TicketSlot queues[NUM_OFFSET_QUEUE_SLOT];

 public:
  explicit OffsetMgr(TxMgr* tx_mgr)
      : tx_mgr(tx_mgr), offset(0), next_ticket(1), queues() {}

  // must have spinlock acquired
  // only call if seeking is the only serialization point
  // no boundary check
  uint64_t seek_absolute(uint64_t abs_offset) { return offset = abs_offset; }
  int64_t seek_relative(int64_t rel_offset) {
    int64_t new_offset = offset + rel_offset;
    if (new_offset < 0) return -1;
    return seek_absolute(new_offset);
  }

  /**
   * move the current offset and get the updated offset; not thread-safe so must
   * be called with spinlock held; must call release_offset after done
   *
   * @param[in,out] count movement applied to the offset, will be updated if
   * hit boundary and stop_at_boundary is set
   * @param[in] file_size the current file size for boundary check
   * @param[in] stop_at_boundary whether stop at the boundary
   * @param[out] ticket ticket for this acquire
   * @return old offset
   */
  uint64_t acquire_offset(uint64_t& count, uint64_t file_size,
                          bool stop_at_boundary, uint64_t& ticket) {
    auto old_offset = offset;
    offset += count;
    if (stop_at_boundary && offset > file_size) {
      offset = file_size;
      count = offset - old_offset;
    }
    ticket = next_ticket++;
    return old_offset;
  }

  /**
   * wait for the previous one to complete; return previous one's idx and block
   *
   * @param[in] ticket ticket from acquire_offset
   * @return address of slot; null if no need to validate
   */
  const OffsetMgr::TicketSlot* wait_offset(uint64_t ticket) {
    // if we don't want strict serialization on offset, always return
    // immediately
    if (!runtime_options.strict_offset_serial) return nullptr;
    uint64_t prev_ticket = ticket - 1;
    if (prev_ticket == 0) return nullptr;
    const TicketSlot* slot = &queues[prev_ticket % NUM_OFFSET_QUEUE_SLOT];
    while (slot->ticket_slot.ticket.load(std::memory_order_acquire) !=
           prev_ticket)
      _mm_pause();
    return slot;
  }

  /**
   * validate whether redo is necessary; the previous operation's serialization
   * point should be no larger than the current one's
   *
   * @param ticket ticket from acquire_offset
   * @param cursor the cursor seen by the current operation
   * @return whether the ordering is fine (prev <= curr)
   */
  bool validate_offset(uint64_t ticket, TxCursor cursor) {
    // if we don't want strict serialization on offset, always return
    // immediately
    if (!runtime_options.strict_offset_serial) return true;
    const TicketSlot* slot = wait_offset(ticket);
    // no previous operation to validate against
    if (!slot) return true;
    if (slot->ticket_slot.cursor < cursor) return true;
    return false;
  }

  /**
   * release the offset
   *
   * @param ticket ticket from acquire_offset
   * @param cursor the cursor seen by the current operation
   */
  void release_offset(uint64_t ticket, TxCursor cursor) {
    // if we don't want strict serialization on offset, always return
    // immediately
    if (!runtime_options.strict_offset_serial) return;
    TicketSlot* slot = &queues[ticket % NUM_OFFSET_QUEUE_SLOT];
    slot->ticket_slot.cursor = cursor;
    slot->ticket_slot.ticket.store(ticket, std::memory_order_release);
  }

  friend std::ostream& operator<<(std::ostream& out, const OffsetMgr& o) {
    out << "OffsetMgr: offset = " << o.offset << "\n";
    return out;
  }
};

};  // namespace ulayfs::dram
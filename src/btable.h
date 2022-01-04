#pragma once

#include <pthread.h>
#include <tbb/concurrent_vector.h>

#include <cstdint>
#include <ostream>

#include "block.h"
#include "idx.h"
#include "log.h"
#include "tx.h"
#include "utils.h"

namespace ulayfs::dram {

// read logs and update mapping from virtual blocks to logical blocks
class BlkTable {
  File* file;
  TxMgr* tx_mgr;

  tbb::concurrent_vector<LogicalBlockIdx> table;

  // keep track of the next TxEntry to apply
  TxEntryIdx tail_tx_idx;
  pmem::TxBlock* tail_tx_block;
  uint64_t file_size;

 public:
  explicit BlkTable(File* file, TxMgr* tx_mgr)
      : file(file),
        tx_mgr(tx_mgr),
        tail_tx_idx(),
        tail_tx_block(nullptr),
        file_size(0) {
    table.resize(16);
  }

  ~BlkTable() = default;

  /**
   * @return the logical block index corresponding the the virtual block index
   *  0 is returned if the virtual block index is not allocated yet
   */
  [[nodiscard]] LogicalBlockIdx get(VirtualBlockIdx virtual_block_idx) const {
    if (virtual_block_idx >= table.size()) return 0;
    return table[virtual_block_idx];
  }

  /**
   * Update the block table by applying the transactions; not thread-safe
   *
   * @param do_alloc whether we allow allocation when iterating the tx_idx
   * @param init_bitmap whether we need to initialize the bitmap
   */
  void update(bool do_alloc, bool init_bitmap = false);

  [[nodiscard]] TxEntryIdx get_tx_idx() const { return tail_tx_idx; }
  [[nodiscard]] pmem::TxBlock* get_tx_block() const { return tail_tx_block; }
  [[nodiscard]] uint64_t get_file_size() const { return file_size; }

 private:
  void resize_to_fit(VirtualBlockIdx idx);

  // TODO: handle writev requests
  /**
   * Apply a transaction to the block table
   *
   * @param tx_commit_entry the entry to be applied
   * @param log_mgr a thread-local log_mgr to be used
   * @param init_bitmap whether we need to initialize the bitmap object
   */
  void apply_tx(pmem::TxEntryIndirect tx_commit_entry, LogMgr* log_mgr,
                bool init_bitmap);

  void apply_tx(pmem::TxEntryInline tx_commit_inline_entry);

  friend std::ostream& operator<<(std::ostream& out, const BlkTable& b);
};

}  // namespace ulayfs::dram

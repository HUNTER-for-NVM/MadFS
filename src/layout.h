#pragma once

#include <atomic>
#include <bit>
#include <cstdint>

#include "config.h"

namespace ulayfs::pmem {

using BlockIdx = uint32_t;
using Bitmap = uint64_t;

class LogEntry;

class TxEntry {
 public:
  uint64_t entry;
};

class TxBeginEntry : public TxEntry {};

class TxCommitEntry : public TxEntry {};

enum LogOp : uint32_t {
  LOG_OVERWRITE,
};

class LogEntry {
  enum LogOp op;
  BlockIdx file_offset;
  BlockIdx block_offset;
  uint32_t size;
};

constexpr static int NUM_BITMAP = BLOCK_SIZE / sizeof(Bitmap);
constexpr static int NUM_TX_ENTRY =
    (BLOCK_SIZE - 2 * sizeof(BlockIdx)) / sizeof(TxEntry);
constexpr static int NUM_LOG_ENTRY = BLOCK_SIZE / sizeof(LogEntry);
constexpr static int NUM_INLINE_BITMAP = 24;
constexpr static int NUM_INLINE_TX_ENTRY = 480;

/*
 * Idx: 0          1          2
 * +----------+----------+----------+----------+----------+----------+----------
 * |   Meta   | Bitmap 1 | Bitmap 2 |   ...    |   ...    | Data/Log |   ...
 * +----------+----------+----------+----------+----------+----------+----------
 * Note: The first few blocks following the meta block is always bitmap blocks
 */

class MetaBlock {
  // file size in bytes
  uint64_t file_size;

  // address for futex to lock
  uint32_t meta_lock;

  // number of blocks following the meta block that are bitmap blocks
  uint32_t num_bitmap_blocks;

  // if inline_tx_entries is used up, this points to the next log block
  BlockIdx log_head;

  // hint to find log tail; not necessarily up-to-date
  BlockIdx log_tail;

  // padding avoid cache line contention
  char padding[40];

  // for the rest of 63 cache lines:
  // 3 cache lines for bitmaps (~1536 blocks)
  Bitmap inline_bitmaps[NUM_INLINE_BITMAP];

  // 60 cache lines for tx log (~480 txs)
  TxEntry inline_tx_entries[NUM_INLINE_TX_ENTRY];
};

class BitmapBlock {
  constexpr static uint64_t BITMAP_ALL_USED = 0xffffffffffffffff;

  Bitmap bitmaps[NUM_BITMAP];

  // allocate one block; return the index of allocated block
  // accept a hint for which bit to start searching
  // usually hint can just be the last idx return by this function
  int alloc(int hint = 0) {
    for (int idx = (hint >> 6); idx < NUM_BITMAP; ++idx) {
    retry:
      Bitmap b = __atomic_load_n(&bitmaps[idx], __ATOMIC_ACQUIRE);
      if (b == BITMAP_ALL_USED) continue;
      Bitmap allocated = ~b & (b + 1);
      if (!__atomic_compare_exchange_n(&bitmaps[idx], &b, b & allocated, true,
                                       __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE))
        goto retry;
      return (idx << 6) + std::countr_zero(b);
    }
    return -1;
  }

  // 64 blocks are considered one batch; return the index of the first block
  int alloc_batch(int hint = 0) {
    for (int idx = (hint >> 6); idx < NUM_BITMAP; ++idx) {
      uint64_t expected = 0;
      if (__atomic_load_n(&bitmaps[idx], __ATOMIC_ACQUIRE) != 0) continue;
      if (!__atomic_compare_exchange_n(&bitmaps[idx], &expected,
                                       BITMAP_ALL_USED, false, __ATOMIC_ACQ_REL,
                                       __ATOMIC_ACQUIRE))
        continue;
      return (idx << 6);
    }
    return -1;
  }
};

class TxLogBlock {
  BlockIdx prev;
  BlockIdx next;
  TxEntry tx_entries[NUM_TX_ENTRY];

 public:
  int try_commit(TxCommitEntry commit_entry, uint32_t hint_tail = 0) {
    for (auto idx = hint_tail; idx < NUM_LOG_ENTRY; ++idx) {
      uint64_t expected = 0;
      if (__atomic_load_n(&tx_entries[idx].entry, __ATOMIC_ACQUIRE)) continue;
      if (__atomic_compare_exchange_n(&tx_entries[idx].entry, &expected,
                                      commit_entry.entry, false,
                                      __ATOMIC_RELEASE, __ATOMIC_ACQUIRE))
        return idx;
    }
    return -1;
  }
};

class LogEntryBlock {
  LogEntry log_entries[NUM_LOG_ENTRY];
};

class DataBlock {
  char data[BLOCK_SIZE];
};

union Block {
  MetaBlock meta_block;
  BitmapBlock bitmap_block;
  TxLogBlock tx_log_block;
  LogEntryBlock log_entry_block;
  DataBlock data_block;
  char padding[BLOCK_SIZE];
};

static_assert(sizeof(Bitmap) == 8, "Bitmap must of 64 bits");
static_assert(sizeof(TxEntry) == 8, "TxEntry must be 64 bits");
static_assert(sizeof(LogEntry) == 16, "LogEntry must of size 16 bytes");
static_assert(sizeof(Block) == BLOCK_SIZE, "Block must be of size BLOCK_SIZE");

};  // namespace ulayfs::pmem

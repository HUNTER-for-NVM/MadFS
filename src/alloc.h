#pragma once

#include <stdexcept>
#include <vector>

#include "block.h"
#include "config.h"
#include "layout.h"
#include "mtable.h"
#include "posix.h"

namespace ulayfs::dram {

// per-thread data structure
class Allocator {
  pmem::MetaBlock* meta;
  MemTable* mem_table;
  int fd;

  // this local free_list maintains blocks allocated from the global free_list
  // and not used yet; pair: <size, idx>
  // sorted in the increasing order (the smallest size first)
  //
  // Note: we choose to use a vector instead of a balanced tree because we limit
  // the maximum number of blocks per allocation to be 64 blocks (256 KB), so
  // the fragmentation should be low, resulting in a small free_list
  std::vector<std::pair<uint32_t, LogicalBlockIdx>> free_list;

  // used as a hint for search; recent is defined to be "the next one to search"
  // keep id for idx translation
  BitmapBlockId recent_bitmap_block_id;
  // NOTE: this is the index within recent_bitmap_block
  BitmapLocalIdx recent_bitmap_local_idx;

 public:
  Allocator()
      : fd(-1),
        meta(nullptr),
        recent_bitmap_block_id(0),
        recent_bitmap_local_idx(0) {}

  void init(int fd, pmem::MetaBlock* meta, MemTable* mem_table) {
    this->fd = fd;
    this->meta = meta;
    this->free_list.reserve(64);
    this->mem_table = mem_table;
  }

  // allocate contiguous blocks (num_blocks must <= 64)
  // if large number of blocks required, please break it into multiple alloc
  // and use log entries to chain them together
  [[nodiscard]] LogicalBlockIdx alloc(uint32_t num_blocks);

  /**
   * Free the blocks in the range [block_idx, block_idx + num_blocks)
   */
  void free(LogicalBlockIdx block_idx, uint32_t num_blocks);
};

}  // namespace ulayfs::dram

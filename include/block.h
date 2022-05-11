#ifndef NAIVEFS_INCLUDE_BLOCK_H_
#define NAIVEFS_INCLUDE_BLOCK_H_

#include <bitset>
#include <map>
#include <vector>

#include "common.h"
#include "ext2/dentry.h"
#include "ext2/inode.h"
#include "ext2/super.h"
#include "state.h"
#include "utils/bitmap.h"
#include "utils/disk.h"
#include "utils/logging.h"

namespace naivefs {

#define GROUP_DESC_MIN_SIZE 0x20

#define BLOCKS2BYTES(__blks) (((uint64_t)(__blks)) * BLOCK_SIZE)
#define BYTES2BLOCKS(__bytes) \
  ((__bytes) / BLOCK_SIZE + ((__bytes) % BLOCK_SIZE ? 1 : 0))

#define MALLOC_BLOCKS(__blks) (malloc(BLOCKS2BYTES(__blks)))
#define ALIGN_TO_BLOCKSIZE(__n) (ALIGN_TO(__n, BLOCK_SIZE))

class Block {
 public:
  Block() : offset_(0), data_(nullptr) {}

  Block(off_t offset) : offset_(offset) {
    data_ = (uint8_t*)alloc_aligned(BLOCK_SIZE);
    int ret = disk_read(offset_, BLOCK_SIZE, data_);
    ASSERT(ret == 0);
  }

  ~Block() {
    flush();
    free(data_);
  }

  int flush() { return disk_write(offset_, BLOCK_SIZE, data_); }

  off_t offset() { return offset_; }

 protected:
  // index in the block group
  off_t offset_;
  // block data read from disk
  uint8_t* data_;
};

class SuperBlock : public Block {
 public:
  SuperBlock() : Block(0), super_((ext2_super_block*)data_) {
    init_super_block();
  }

  void init_super_block();

  inline ext2_super_block* get_super() { return super_; }

  inline ext2_group_desc* get_group_desc(int index) {
    if ((size_t)index >= desc_table_.size()) return nullptr;
    return desc_table_[index];
  }

  inline uint64_t block_group_size() {
    return BLOCKS2BYTES(super_->s_blocks_per_group);
  }

  inline uint32_t num_block_groups() {
    uint32_t n = (super_->s_blocks_count + super_->s_blocks_per_group - 1) /
                 super_->s_blocks_per_group;
    return n ? n : 1;
  }

  inline uint32_t block_size() {
    return ((uint64_t)1) << (super_->s_log_block_size + 10);
  }

  inline uint32_t blocks_per_group() { return super_->s_blocks_per_group; }

  inline uint32_t inodes_per_group() { return super_->s_inodes_per_group; }

  inline uint32_t inode_size() { return super_->s_inode_size; }

  inline off_t block_bitmap_offset(uint32_t block_index) {
    uint32_t n_group = block_index / blocks_per_group();
    ASSERT(n_group < num_block_groups());
    DEBUG("Block bitmap offset: 0x%x", desc_table_[n_group]->bg_block_bitmap);
    return BLOCKS2BYTES(desc_table_[n_group]->bg_block_bitmap);
  }

  inline off_t inode_bitmap_offset(uint32_t inode_index) {
    uint32_t n_group = inode_index / inodes_per_group();
    ASSERT(n_group < num_block_groups());
    DEBUG("Inode bitmap offset: 0x%x", desc_table_[n_group]->bg_inode_bitmap);
    return BLOCKS2BYTES(desc_table_[n_group]->bg_inode_bitmap);
  }

  inline off_t inode_table_offset(uint32_t inode_index) {
    uint32_t n_group = inode_index / inodes_per_group();
    ASSERT(n_group < num_block_groups());
    DEBUG("Inode table offset: 0x%x", desc_table_[n_group]->bg_inode_table);
    return BLOCKS2BYTES(desc_table_[n_group]->bg_inode_table);
  }

 private:
  ext2_super_block* super_;
  std::vector<ext2_group_desc*> desc_table_;
};

class BitmapBlock : public Block {
 public:
  BitmapBlock(off_t offset) : Block(offset), bitmap_(data_) {}

  int64_t alloc_new() {
    int64_t i = bitmap_.find(BLOCK_SIZE * sizeof(uint8_t));
    if (i < 0) {
      ERR("Failed to alloc new item");
      return i;
    }
    bitmap_.set(i);
    return i;
  }

  void set(int i) { bitmap_.set(i); }

  bool test(int i) { return bitmap_.test(i); }

  void clear(int i) { bitmap_.clear(i); }

 private:
  Bitmap bitmap_;
};

class InodeTableBlock : public Block {
 public:
  InodeTableBlock(off_t offset) : Block(offset) {
    ext2_inode* ptr = (ext2_inode*)data_;
    for (uint32_t i = 0; i < INODE_PER_BLOCK; ++i) {
      inodes_.push_back(ptr++);
    }
  }

  inline ext2_inode* get(uint32_t index) { return inodes_[index]; }

 private:
  std::vector<ext2_inode*> inodes_;
};

class BlockGroup {
 public:
  BlockGroup(ext2_group_desc* desc);

  ~BlockGroup();

  void flush();

  bool get_inode(uint32_t index, ext2_inode** inode);

  bool get_block(uint32_t index, Block** block);

  bool alloc_inode(ext2_inode** inode);

  bool alloc_block(Block** block);

 private:
  BitmapBlock* block_bitmap_;
  BitmapBlock* inode_bitmap_;
  std::map<uint32_t, InodeTableBlock*> inode_table_;

  off_t inode_block_offset(uint32_t inode_block_index);

  off_t data_block_offset(uint32_t data_block_index);
};

}  // namespace naivefs

#endif
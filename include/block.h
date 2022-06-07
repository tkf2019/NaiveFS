#ifndef NAIVEFS_INCLUDE_BLOCK_H_
#define NAIVEFS_INCLUDE_BLOCK_H_

#include <sys/stat.h>

#include <bitset>
#include <functional>
#include <map>
#include <vector>

#include "common.h"
#include "crypto.h"
#include "ext2/dentry.h"
#include "ext2/inode.h"
#include "ext2/super.h"
#include "utils/bitmap.h"
#include "utils/disk.h"
#include "utils/logging.h"
#include <algorithm>

namespace naivefs {

#define GROUP_DESC_MIN_SIZE 0x20

#define BLOCKS2BYTES(__blks) (((uint64_t)(__blks)) * BLOCK_SIZE)
#define BYTES2BLOCKS(__bytes) ((__bytes) / BLOCK_SIZE + ((__bytes) % BLOCK_SIZE ? 1 : 0))

#define MALLOC_BLOCKS(__blks) (malloc(BLOCKS2BYTES(__blks)))
#define ALIGN_TO_BLOCKSIZE(__n) (ALIGN_TO(__n, BLOCK_SIZE))

extern Auth* auth;

class Block {
 public:
  Block() : offset_(0), data_(nullptr) {}

  Block(off_t offset, bool alloc = false) : offset_(offset) {
    data_ = (uint8_t*)alloc_aligned(BLOCK_SIZE);
    if (!alloc) {
      int ret = disk_read(offset_, BLOCK_SIZE, data_);
      // we don't decrypt all of the superblock.
      ASSERT(ret == 0);
      if (offset)
        auth->read(data_, BLOCK_SIZE);
      else
        auth->read(reinterpret_cast<ext2_super_block*>(data_)->s_auth_string, 64);
    } else {
      memset(data_, 0, BLOCK_SIZE);
    }
  }

  ~Block() {
    // we need to flush the dirty block manually
    // flush();
    free(data_);
  }

  int flush() {
    if (auth->flag()) {
      INFO("flush auth");
      if (offset_) {
        auto ptr = auth->write(data_, BLOCK_SIZE);
        if (!ptr)
          disk_write(offset_, BLOCK_SIZE, data_);
        else {
          disk_write(offset_, BLOCK_SIZE, ptr);
          free(ptr);
        }
      } else {
        auto str = reinterpret_cast<ext2_super_block*>(data_)->s_auth_string;
        auto ptr = auth->write(str, 64);
        if (!ptr)
          disk_write(offset_, BLOCK_SIZE, data_);
        else {
          for (int i = 0; i < 64; ++i) std::swap(ptr[i], str[i]);
          disk_write(offset_, BLOCK_SIZE, data_);
          for (int i = 0; i < 64; ++i) std::swap(ptr[i], str[i]);
          free(ptr);
        }
      }
    } else {
      INFO("flush not auth");
      ASSERT(data_ != nullptr);
      return disk_write(offset_, BLOCK_SIZE, data_);
    }
    return 0;
  }

  off_t offset() { return offset_; }

  uint8_t* get() { return data_; }

 protected:
  // index in the block group
  off_t offset_;
  // block data read from disk
  uint8_t* data_;
};

class SuperBlock : public Block {
 public:
  SuperBlock() : Block(0), super_((ext2_super_block*)data_) { init_super_block(); }

  void init_super_block();

  inline ext2_super_block* get_super() { return super_; }

  inline ext2_group_desc* get_group_desc(int index) {
    if ((size_t)index >= desc_table_.size()) return nullptr;
    return desc_table_[index];
  }

  inline void put_group_desc(ext2_group_desc* desc) { desc_table_.push_back(desc); }

  inline uint64_t block_group_size() { return BLOCKS2BYTES(super_->s_blocks_per_group); }

  inline uint32_t num_block_groups() {
    DEBUG("[SuperBlock] BLOCKS COUNT: %u", super_->s_blocks_count);
    return super_->s_group;
    uint32_t block_n = (super_->s_blocks_count + super_->s_blocks_per_group - 1) / super_->s_blocks_per_group;
    uint32_t inode_n = (super_->s_inodes_count + super_->s_inodes_per_group - 1) / super_->s_inodes_per_group;
    uint32_t n = std::max(inode_n, block_n);
    return n ? n : 1;
  }

  inline uint32_t block_size() { return ((uint64_t)1) << (super_->s_log_block_size + 10); }

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

  inline uint32_t num_aligned_blocks(uint32_t iblocks) { return iblocks / (2 << super_->s_log_block_size); }

 private:
  ext2_super_block* super_;
  std::vector<ext2_group_desc*> desc_table_;
};

class BitmapBlock : public Block {
 public:
  BitmapBlock(off_t offset, bool alloc = false) : Block(offset, alloc), bitmap_(data_) {}

  int64_t alloc_new();

  inline void set(int i) { bitmap_.set(i); }

  inline bool test(int i) { return bitmap_.test(i); }

  inline void clear(int i) { bitmap_.clear(i); }

 private:
  Bitmap bitmap_;
};

class InodeTableBlock : public Block {
 public:
  InodeTableBlock(off_t offset, bool alloc = false) : Block(offset, alloc) {
    ext2_inode* ptr = (ext2_inode*)data_;
    for (uint32_t i = 0; i < INODES_PER_BLOCK; ++i) {
      inodes_.push_back(ptr++);
    }
  }

  inline ext2_inode* get(uint32_t index) { return inodes_[index]; }

 private:
  std::vector<ext2_inode*> inodes_;
};

class DentryBlock {
 public:
  /**
   * Construct a new Dentry Block object. We don't use a derived class
   * here. It's hard to handle memory space if two different objects refer to
   * the same position.
   *
   */
  DentryBlock(Block* block) : block_(block), size_(0) {
    uint8_t* data = block->get();
    ext2_dir_entry_2* dentry = (ext2_dir_entry_2*)data;
    while (true) {
      if (dentry->rec_len == 0) {
        // We ensure that directory entries are APPENDED to the data block.
        break;
      }
      dentries_.push_back(dentry);
      size_ += dentry->rec_len;
      if (size_ + sizeof(ext2_dir_entry_2) > BLOCK_SIZE) {
        // Avoid pointer reaching the undefined area
        break;
      }
      data += dentry->rec_len;
      dentry = (ext2_dir_entry_2*)data;
    }
  }

  std::vector<ext2_dir_entry_2*>* get() { return &dentries_; }

  ext2_dir_entry_2* alloc_dentry(const char* name, size_t name_len, uint32_t inode, mode_t mode) {
    ext2_dir_entry_2* dentry = (ext2_dir_entry_2*)(block_->get() + size_);
    dentry->inode = inode;
    dentry->name_len = name_len;
    dentry->rec_len = sizeof(ext2_dir_entry_2) + name_len;
    dentry->file_type = mode >> 12;
    strncpy(dentry->name, name, name_len);
    dentries_.push_back(dentry);
    return dentry;
  }

  size_t size() { return size_; }

 private:
  Block* block_;
  std::vector<ext2_dir_entry_2*> dentries_;
  size_t size_;
};

class BlockGroup {
 public:
  BlockGroup(ext2_group_desc* desc, uint32_t group_idx, bool alloc = false);

  ~BlockGroup();

  void flush();

  ext2_group_desc* get_desc() { return desc_; }

  bool get_inode(uint32_t index, ext2_inode** inode);

  bool get_block(uint32_t index, Block** block);

  bool alloc_inode(ext2_inode** inode, uint32_t* index, mode_t mode);

  bool alloc_block(Block** block, uint32_t* index);

  bool free_inode(uint32_t index);

  bool free_block(uint32_t index);

 private:
  uint32_t group_idx_;
  ext2_group_desc* desc_;
  BitmapBlock* block_bitmap_;
  BitmapBlock* inode_bitmap_;
  std::map<uint32_t, InodeTableBlock*> inode_table_;

  off_t inode_block_offset(uint32_t inode_block_index);

  off_t data_block_offset(uint32_t data_block_index);
};

typedef std::function<bool(uint32_t, Block*)> BlockVisitor;
}  // namespace naivefs

#endif

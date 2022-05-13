#ifndef NAIVEFS_INCLUDE_FUSEOP_H_
#define NAIVEFS_INCLUDE_FUSEOP_H_

#define FUSE_USE_VERSION 31
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <mutex>
#include <shared_mutex>

#include "ext2/inode.h"
#include "filesystem.h"
#include "utils/logging.h"
#include "utils/option.h"

namespace naivefs {

extern FileSystem* fs;

/**
 * @brief FileStatus: The status of the file handle
 *
 * Recall: i_block[0...11] is direct map https://ext4.wiki.kernel.org/index.php/Ext4_Disk_Layout
 * i_block[12] is indirect block, i.e. file blocks 12...BLOCK_SIZE / 4 + 11
 * i_block[13] is double-indirect block, i.e. file blocks BLOCK_SIZE / 4 + 12...(BLOCK_SIZE / 4) ^ 2 + (BLOCK_SIZE / 4) + 11
 * i_block[14] is trible-indirect block, i.e. file blocks (BLOCK_SIZE / 4) ^ 2 + (BLOCK_SIZE / 4) + 12...(BLOCK_SIZE / 4) ^ 3 + (BLOCK_SIZE / 4) ^ 2 +
 * (BLOCK_SIZE / 4) + 12
 *
 * inode is 4B.
 */

constexpr uint32_t IBLOCK_11 = 11;
constexpr uint32_t IBLOCK_12 = BLOCK_SIZE / 4 + 11;
constexpr uint32_t IBLOCK_13 = (BLOCK_SIZE / 4) * (BLOCK_SIZE / 4) + IBLOCK_12;
constexpr uint32_t IBLOCK_14 = (BLOCK_SIZE / 4) * (BLOCK_SIZE / 4) * (BLOCK_SIZE / 4) + IBLOCK_13;  // 1074791436

class InodeCache {
 public:
  uint32_t inode_id_;               // this inode
  ext2_inode* cache_;               // cache of the inode
  std::shared_mutex inode_rwlock_;  // if a file is opened by many processes, we use this to ensure atomicity.
  std::vector<FileStatus*> vec;     // when inode cache is changed in a critical section, other process must update their cache.
  void shared_lock() { inode_rwlock_->shared_lock(); }
  void shared_unlock() { inode_rwlock_->shared_unlock(); }
  void lock() { inode_rwlock_->lock(); }
  void unlock() { inode_rwlock_->unlock(); }
  void init() { inode_cache_ = new ext2_inode; }
  void copy() {
    ext2_inode* inode;
    if (!fs->get_inode(inode_id_, &inode)) return -EINVAL;
    memcpy(inode_cache_, inode, sizeof(ext2_inode));
  }
};

class FileStatus {
 public:
  class IndirectBlockPtr {
   public:
    uint32_t id_;
    IndirectBlockPtr(uint32_t indirect_block_id) : id_(indirect_block_id) {}
    bool seek(off_t off, uint32_t& block_id) {
      Block* blk;
      if (!fs->get_block(indirect_block_id_[0], &blk)) return false;
      block_id = (reinterpret_cast<uint32_t*>(blk->get()))[off];
    }
  };
  InodeCache* inode_cache_;
  bool cache_update_flag_;
  uint32_t block_id_;                   // current block
  uint32_t block_id_in_file_;           // i.e. current offset / BLOCK_SIZE
  IndirectBlockPtr indirect_block_[3];  // if indirect_blocks are using, we record each level
  std::shared_mutex rwlock;             // lock the FileStatus itself.
  // FileStatus(uint32_t inode_id, uint32_t block_id, uint32_t block_id_in_file)
  //     : inode_id_(inode_id), block_id_(block_id), block_id_in_file_(block_id_in_file) {}
  bool is_one_block(off_t off) { return off / BLOCK_SIZE == block_id_in_file_; }
  bool check_size(off_t off, size_t counts) { return counts + off > (size_t)inode_cache_->cache_->i_size; }
  size_t file_size() { return inode_cache_->cache_->i_size; }
  int next_block() {
    if (block_id_in_file_ <= IBLOCK_11) {
      // The first 12 blocks
      block_id_in_file_++;
      if (block_id_in_file_ <= IBLOCK_11) {
        block_id_ = inode_cache_->cache_->i_block[block_id_in_file_];
      } else {
        indirect_block_[0] = IndirectBlockPtr(inode_cache_->cache_->i_block[12]);
        if (!indirect_block_[0].seek(0, block_id_)) return -EINVAL;
      }
    } else if (block_id_in_file_ <= IBLOCK_12) {
      // The first indirect block
      block_id_in_file_++;
      if (block_id_in_file_ <= IBLOCK_12) {
        if (!indirect_block_[0].seek(block_id_in_file_ - IBLOCK_11 - 1, block_id_)) return -EINVAL;
      } else {
        indirect_block_[0] = IndirectBlockPtr(inode_cache_->cache_->i_block[13]);
        if (!indirect_block_[0].seek(0, indirect_block_[1].id_)) return -EINVAL;
        if (!indirect_block_[1].seek(0, block_id_)) return -EINVAL;
      }
    } else if (block_id_in_file_ <= IBLOCK_13) {
      // The double indirect block
      block_id_in_file_++;
      if (block_id_in_file_ <= IBLOCK_13) {
        uint32_t first_id = (block_id_in_file_ - IBLOCK_12 - 1) / (BLOCK_SIZE / 4);
        uint32_t second_id = (block_id_in_file_ - IBLOCK_12 - 1) % (BLOCK_SIZE / 4);
        if (second_id == 0) {
          if (!indirect_block_[0].seek(first_id, indirect_block_[1].id_)) return -EINVAL;
          if (!indirect_block_[1].seek(0, block_id_)) return -EINVAL;
        } else if (!indirect_block_[1].seek(second_id, block_id_))
          return -EINVAL;
      } else {
        indirect_block_[0] = IndirectBlockPtr(inode_cache_->cache_->i_block[14]);
        if (!indirect_block_[0].seek(0, indirect_block_[1].id_)) return -EINVAL;
        if (!indirect_block_[1].seek(0, indirect_block_[2].id_)) return -EINVAL;
        if (!indirect_block_[2].seek(0, block_id_)) return -EINVAL;
      }
    } else if (block_id_in_file_ <= IBLOCK_14) {
      // The triple indirect block
      block_id_in_file_++;
      uint32_t first_id = (block_id_in_file_ - IBLOCK_13 - 1) / ((BLOCK_SIZE / 4) * (BLOCK_SIZE / 4));
      uint32_t second_id = (block_id_in_file_ - IBLOCK_13 - 1) % ((BLOCK_SIZE / 4) * (BLOCK_SIZE / 4)) / (BLOCK_SIZE / 4);
      uint32_t third_id = (block_id_in_file_ - IBLOCK_13 - 1) % (BLOCK_SIZE / 4);
      if (third_id == 0) {
        if (second_id == 0) {
          if (!indirect_block_[0].seek(first_id, indirect_block_[1].id_)) return -EINVAL;
          if (!indirect_block_[1].seek(second_id, indirect_block_[2].id_)) return -EINVAL;
          if (!indirect_block_[2].seek(third_id, block_id_)) return -EINVAL;
        } else {
          if (!indirect_block_[1].seek(second_id, indirect_block_[2].id_)) return -EINVAL;
          if (!indirect_block_[2].seek(third_id, block_id_)) return -EINVAL;
        }
      } else if (!indirect_block_[2].seek(third_id, block_id_))
        return -EINVAL;
    } else
      return -EINVAL;
    return 0;
  }

  int seek(uint32_t new_block_id_in_file) {
    if (new_block_id_in_file <= IBLOCK_12) {
      bf_seek(new_block_id_in_file);
    } else if (new_block_id_in_file <= IBLOCK_13) {
      uint32_t old_first_id = (block_id_in_file_ - IBLOCK_12 - 1) / (BLOCK_SIZE / 4);
      uint32_t old_second_id = (block_id_in_file_ - IBLOCK_12 - 1) % (BLOCK_SIZE / 4);
      uint32_t new_first_id = (new_block_id_in_file - IBLOCK_12 - 1) / (BLOCK_SIZE / 4);
      uint32_t new_second_id = (new_block_id_in_file - IBLOCK_12 - 1) % (BLOCK_SIZE / 4);
      block_id_in_file_ = new_block_id_in_file;
      if (new_first_id == old_first_id && new_second_id == old_second_id) {
        return 0;
      } else if (new_first_id == old_first_id) {
        if (!indirect_block_[1].seek(new_second_id, block_id_)) return -EINVAL;
      } else
        return bf_seek(new_block_id_in_file);
    } else if (new_block_id_in_file <= IBLOCK_14) {
      uint32_t old_first_id = (block_id_in_file_ - IBLOCK_13 - 1) / ((BLOCK_SIZE / 4) * (BLOCK_SIZE / 4));
      uint32_t old_second_id = (block_id_in_file_ - IBLOCK_13 - 1) % ((BLOCK_SIZE / 4) * (BLOCK_SIZE / 4)) / (BLOCK_SIZE / 4);
      uint32_t old_third_id = (block_id_in_file_ - IBLOCK_13 - 1) % (BLOCK_SIZE / 4);
      uint32_t new_first_id = (new_block_id_in_file - IBLOCK_13 - 1) / ((BLOCK_SIZE / 4) * (BLOCK_SIZE / 4));
      uint32_t new_second_id = (new_block_id_in_file - IBLOCK_13 - 1) % ((BLOCK_SIZE / 4) * (BLOCK_SIZE / 4)) / (BLOCK_SIZE / 4);
      uint32_t new_third_id = (new_block_id_in_file - IBLOCK_13 - 1) % (BLOCK_SIZE / 4);
      block_id_in_file_ = new_block_id_in_file;
      if (new_first_id == old_first_id && new_second_id == old_second_id && new_third_id == old_third_id) {
        return 0;
      } else if (new_first_id == old_first_id && new_second_id == old_second_id) {
        if (!indirect_block_[2].seek(new_second_id, block_id_)) return -EINVAL;
      } else if (new_first_id == old_first_id) {
        if (!indirect_block_[1].seek(new_second_id, indirect_block_[2].id_)) return -EINVAL;
        if (!indirect_block_[2].seek(new_second_id, block_id_)) return -EINVAL;
      } else
        return bf_seek(new_block_id_in_file);
    }
    return 0;
  }

  int bf_seek(uint32_t new_block_id_in_file) {
    block_id_in_file_ = new_block_id_in_file;
    if (block_id_in_file_ <= IBLOCK_11) {
      // the 12 direct blocks
      block_id_ = inode_cache_->cache_->i_block[block_id_in_file_];
    } else if (block_id_in_file_ <= IBLOCK_12) {
      // The first indirect block
      uint32_t first_id = block_id_ - IBLOCK_11 - 1;
      indirect_block_[0] = IndirectBlockPtr(inode_cache_->cache_->i_block[12]);
      if (!indirect_block_[0].seek(first_id, block_id_)) return -EINVAL;
    } else if (block_id_in_file_ <= IBLOCK_13) {
      // The double indirect block
      uint32_t first_id = (block_id_ - IBLOCK_12 - 1) / (BLOCK_SIZE / 4);
      uint32_t second_id = (block_id_ - IBLOCK_12 - 1) % (BLOCK_SIZE / 4);
      indirect_block_[0] = IndirectBlockPtr(inode_cache_->cache_->i_block[13]);
      if (!indirect_block_[0].seek(first_id, indirect_block_[1].id_)) return -EINVAL;
      if (!indirect_block_[1].seek(second_id, block_id_)) return -EINVAL;
    } else if (block_id_in_file_ <= IBLOCK_14) {
      // The triple indirect block
      uint32_t first_id = (block_id_in_file_ - IBLOCK_13 - 1) / ((BLOCK_SIZE / 4) * (BLOCK_SIZE / 4));
      uint32_t second_id = (block_id_in_file_ - IBLOCK_13 - 1) % ((BLOCK_SIZE / 4) * (BLOCK_SIZE / 4)) / (BLOCK_SIZE / 4);
      uint32_t third_id = (block_id_in_file_ - IBLOCK_13 - 1) % (BLOCK_SIZE / 4);
      indirect_block_[0] = IndirectBlockPtr(inode_cache_->cache_->i_block[14]);
      if (!indirect_block_[0].seek(first_id, indirect_block_[1].id_)) return -EINVAL;
      if (!indirect_block_[1].seek(second_id, indirect_block_[2].id_)) return -EINVAL;
      if (!indirect_block_[2].seek(third_id, block_id_)) return -EINVAL;
    } else
      return -EINVAL;
    return 0;
  }

  int _upd_cache() {
    if (cache_update_flag_) {
      cache_update_flag_ = false;
      return bf_seek(block_id_in_file_);
    }
    return 0;
  }

  /**
   * @brief copy_to_buf copy the file to the buf. this function works under writer lock, because it changes file pointer. It works under reader lock
   * of inode_rwlock, because it only reads inode data.
   *
   * @param buf
   * @param offset
   * @param size
   * @return int the number of bytes
   *
   *
   */

  int copy_to_buf(char* buf, off_t offset, size_t size) {
    std::unique_lock<std::shared_mutex> lck(rwlock);
    std::shared_lock<std::shared_mutex> lck_inode(inode_cache_->inode_rwlock);
    size_t isize = file_size();
    if (offset >= isize) return 0;
    int _err_ret = _upd_cache();
    if (_err_ret) return _err_ret;

    _err_ret = seek(offset / BLOCK_SIZE);
    if (_err_ret) return _err_ret;
    Block* blk;
    if (!fs->get_block(block_id_, &blk)) return -EINVAL;
    size_t ret = 0;
    size_t csz = std::min(size, BLOCK_SIZE - offset % BLOCK_SIZE);
    memcpy(buf, blk->get() + offset % BLOCK_SIZE, csz);
    ret += csz, size -= csz, offset += csz;
    while (size) {
      if (offset >= isize) return ret;
      _err_ret = next_block();
      if (_err_ret) return _err_ret;
      if (!fs->get_block(block_id_, &blk)) return -EINVAL;
      csz = std::min(size, std::min((size_t)isize - (size_t)offset, (size_t)BLOCK_SIZE));
      memcpy(buf, blk->get(), csz);
      ret += csz, size -= csz, offset += csz;
    }
    return ret;
  }

  /**
   * @brief write buf to the file. the function works under rwlock because it changes file pointers, and we also lock inode_rwlock, because it changes
   * file metadata. We don't ensure that reading, writing a file by processes will occur in an expected order. We don't ensure that RW are not
   * performed at one data block at the same time. But we ensure that operations on inode metadata are serializable. We ensure that operations on
   * FileStatus itself are serializable.
   *
   * @param buf
   * @param offset
   * @param size
   * @return int
   */

  int write(char* buf, off_t offset, size_t size) {
    std::unique_lock<std::shared_mutex> lck(rwlock);
    inode_rwlock->lock_shared();
    size_t isize = file_size();
    if (offset >= isize) {
      inode_rwlock->unlock_shared();
      return 0;
    }
    int _err_ret = _upd_cache();
    if (_err_ret) {
      inode_rwlock->unlock_shared();
      return _err_ret;
    }
    _err_ret = seek(offset / BLOCK_SIZE);
    if (_err_ret) {
      inode_rwlock->unlock_shared();
      return _err_ret;
    }
    if (offset + size > isize) {
      // Now we need to modify the inode.
      inode_rwlock->unlock_shared();
      std::unique_lock<std::shared_mutex> inode_lck(inode_rwlock);
      isize = file_size();
      if (offset >= isize) return 0;
      _err_ret = seek(offset / BLOCK_SIZE);
      if (_err_ret) return _err_ret;
      Block* blk;
      if (!fs->get_block(block_id_, &blk)) return -EINVAL;
      size_t ret = 0;
      size_t csz = std::min(size, BLOCK_SIZE - offset % BLOCK_SIZE);
      memcpy(blk->get() + offset % BLOCK_SIZE, buf, csz);
      ret += csz, size -= csz, offset += csz, inode_cache_->cache_->i_size += csz;
      while(size) {
        if(offset >= isize) {
          if(fs->alloc_block(&blk)) return ret;
        } else {
          next_block();
          if(!fs->get_block(block_id_, &blk)) return -EINVAL;
        }
        csz = std::min(size, (size_t)BLOCK_SIZE);
        memcpy(blk->get(), buf, csz);
        ret += csz, size -= csz, offset += csz, inode_cache_->cache_->i_size += csz;
      }
      return ret;
      
    } else {
      Block* blk;
      if (!fs->get_block(block_id_, &blk)) return -EINVAL;
      size_t ret = 0;
      size_t csz = std::min(size, BLOCK_SIZE - offset % BLOCK_SIZE);
      memcpy(blk->get() + offset % BLOCK_SIZE, buf, csz);
      ret += csz, size -= csz, offset += csz;
      while (size) {
        if (offset >= isize) return ret;
        _err_ret = next_block();
        if (_err_ret) {
          inode_rwlock->unlock_shared();
          return _err_ret;
        }
        if (!fs->get_block(block_id_, &blk)) return -EINVAL;
        csz = std::min(size, (size_t)BLOCK_SIZE);
        memcpy(blk->get(), buf, csz);
        ret += csz, size -= csz, offset += csz;
      }
      return ret;
    }
    return 0;
  }
};

FileStatus* _fuse_trans_info(struct fuse_file_info* fi) { return reinterpret_cast<FileStatus*>(fi->fh); }

int fuse_getattr(const char* path, struct stat* stbuf, struct fuse_file_info* fi);

int fuse_readlink(const char* path, char* buf, size_t size);

int fuse_mkdir(const char* path, mode_t mode);

int fuse_readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi, enum fuse_readdir_flags flags);

void* fuse_init(fuse_conn_info* info, fuse_config* config);

int fuse_read(const char* path, char* buf, size_t size, off_t offset, fuse_file_info* fi);

int fuse_open(const char* path, fuse_file_info* fi);

}  // namespace naivefs

#endif
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

class FileStatus;
class InodeCache {
 public:
  uint32_t inode_id_;               // this inode
  ext2_inode* cache_;               // cache of the inode
  std::shared_mutex inode_rwlock_;  // if a file is opened by many processes, we use this to ensure atomicity.
  std::vector<FileStatus*> vec;     // when inode cache is changed in a critical section, other process must update their cache.
  void lock_shared() { inode_rwlock_.lock_shared(); }
  void unlock_shared() { inode_rwlock_.unlock_shared(); }
  void lock() { inode_rwlock_.lock(); }
  void unlock() { inode_rwlock_.unlock(); }
  void init() { cache_ = new ext2_inode; }
  int copy() {
    ext2_inode* inode;
    if (!fs->get_inode(inode_id_, &inode)) return -EINVAL;
    memcpy(cache_, inode, sizeof(ext2_inode));
  }
  void upd_all();
};

class FileStatus {
 public:
  class IndirectBlockPtr {
   public:
    uint32_t id_;
    IndirectBlockPtr() { id_ = 0; }
    IndirectBlockPtr(uint32_t indirect_block_id) : id_(indirect_block_id) {}
    bool seek(off_t off, uint32_t& block_id) {
      Block* blk;
      if (!fs->get_block(id_, &blk)) return false;
      block_id = (reinterpret_cast<uint32_t*>(blk->get()))[off];
      return true;
    }
    bool set(off_t off, uint32_t block_id) {
      Block* blk;
      if (!fs->get_block(id_, &blk)) return false;
      block_id = (reinterpret_cast<uint32_t*>(blk->get()))[off];
      return true;
    }
  };
  InodeCache* inode_cache_;
  bool cache_update_flag_;
  uint32_t block_id_;                   // current block
  uint32_t block_id_in_file_;           // i.e. current offset / BLOCK_SIZE
  IndirectBlockPtr indirect_block_[3];  // if indirect_blocks are using, we record each level
  std::shared_mutex rwlock;             // lock the FileStatus itself.
  bool is_one_block(off_t off) { return off / BLOCK_SIZE == block_id_in_file_; }
  bool check_size(off_t off, size_t counts) { return counts + off > (size_t)inode_cache_->cache_->i_size; }
  size_t file_size() { return inode_cache_->cache_->i_size; }

  /**
   * @brief next_block: get the next block of block_id_in_file_, and block_id_in_file_ += 1
   * @brief seek: given a new value of block_id_in_file_, seek block_id_ using old values indirect_block_
   * @brief bf_seek: seek, but don't use any old values
   * They are not atomic.
   *
   *
   * @return int, 0 if success, else a negative integer
   */

  int next_block();
  int seek(uint32_t new_block_id_in_file);
  int bf_seek(uint32_t new_block_id_in_file);

  /**
   * @brief update indirect_block_ by bf_seek
   *
   * @return int
   */

  int _upd_cache() {
    if (cache_update_flag_) {
      cache_update_flag_ = false;
      return bf_seek(block_id_in_file_);
    }
    return 0;
  }

  /**
   * @brief init the file pointers
   *
   */

  void init_seek() {
    std::shared_lock<std::shared_mutex> lck_inode(inode_cache_->inode_rwlock_);
    if (inode_cache_->cache_->i_size) bf_seek(0);
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
  int copy_to_buf(char* buf, size_t offset, size_t size);

  /**
   * @brief write buf to the file. the function works under rwlock because it changes file pointers, and we also lock inode_rwlock, because it changes
   * file metadata. We don't ensure that reading, writing a file by processes will occur in an expected order. We don't ensure that RW are not
   * performed at one data block at the same time. But we ensure that operations on inode metadata are serializable. We ensure that operations on
   * FileStatus itself are serializable.
   *
   * if append_flag is true then offset is set to the end of the file at beginning.
   *
   * @param buf
   * @param offset
   * @param size
   * @return int
   */
  int write(char* buf, size_t offset, size_t size, bool append_flag = false);

  int append(char* buf, size_t offset, size_t size) { return write(buf, offset, size, true); }
};

class OpManager {
 public:
  /**
   * @brief Get the cache object by inode_id
   *
   * @param inode_id
   * @return InodeCache*
   */
  InodeCache* get_cache(uint32_t inode_id) {
    std::unique_lock<std::shared_mutex> lck(m_);
    if (!st_.count(inode_id)) {
      auto ic = new InodeCache;
      ic->inode_id_ = inode_id;
      ic->init();
      st_[inode_id] = ic;
    }
    return st_[inode_id];
  }
  /**
   * @brief update the vector in the cache object
   *
   * @param fd
   * @param inode_id
   */
  void upd_cache(FileStatus* fd, uint32_t inode_id) {
    std::unique_lock<std::shared_mutex> lck(m_);
    if (!st_.count(inode_id)) return;
    st_[inode_id]->vec.push_back(fd);
  }

 private:
  std::map<uint32_t, InodeCache*> st_;
  std::shared_mutex m_;
};

extern OpManager* opm;

FileStatus* _fuse_trans_info(struct fuse_file_info* fi);

int fuse_getattr(const char* path, struct stat* stbuf, struct fuse_file_info* fi);

int fuse_readlink(const char* path, char* buf, size_t size);

int fuse_mkdir(const char* path, mode_t mode);

int fuse_readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi, enum fuse_readdir_flags flags);

void* fuse_init(fuse_conn_info* info, fuse_config* config);

int fuse_read(const char* path, char* buf, size_t size, off_t offset, fuse_file_info* fi);

int fuse_open(const char* path, fuse_file_info* fi);

int fuse_create(const char* path, mode_t mode, struct fuse_file_info* fi);

}  // namespace naivefs

#endif
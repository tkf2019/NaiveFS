#ifndef NAIVEFS_INCLUDE_FILESYSTEM_H_
#define NAIVEFS_INCLUDE_FILESYSTEM_H_

#include <sys/time.h>

#include <string>

#include "block.h"
#include "cache.h"

namespace naivefs {

class FileSystem {
 public:
  FileSystem();

  ~FileSystem() {
    delete root_inode_;
    delete super_block_;
    for (auto bg : block_groups_) {
      delete bg.second;
    }
  }

  inline ext2_super_block* super() { return super_block_->get_super(); }

  void init_root_inode();

 private:
  // Timestamp
  timeval time_;
  // Super block
  SuperBlock* super_block_;
  // Root inode
  ext2_inode* root_inode_;
  // Block Groups
  std::map<uint32_t, BlockGroup*> block_groups_;

  // path mapped to directory entry metadata
  LRUCache<std::string, ext2_dir_entry_2>* dentry_cache_;
};
}  // namespace naivefs
#endif
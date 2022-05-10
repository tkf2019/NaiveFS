#ifndef NAIVEFS_INCLUDE_FILESYSTEM_H_
#define NAIVEFS_INCLUDE_FILESYSTEM_H_

#include <string>

#include "block.h"
#include "cache.h"
#include "common.h"
#include "ext2/dentry.h"
#include "ext2/inode.h"
#include "ext2/super.h"
#include "state.h"

namespace naivefs {

class FileSystem {
 public:
  FileSystem() : super_block_(new SuperBlock()) {
    DEBUG("Initialize file system");
  }

  ~FileSystem() { delete super_block_; }

  inline ext2_super_block* super() { return super_block_->get(); }

 private:
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
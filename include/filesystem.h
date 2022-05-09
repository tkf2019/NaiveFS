#ifndef NAIVEFS_INCLUDE_FILESYSTEM_H_
#define NAIVEFS_INCLUDE_FILESYSTEM_H_

#include <string>

#include "block.h"
#include "cache.h"
#include "common.h"
#include "ext2/dentry.h"
#include "ext2/inode.h"
#include "ext2/super.h"

namespace naivefs {

class FileSystem {
 public:
  FileSystem() : super_block_(new SuperBlock()) {
    // Super block info
    INFO("BLOCK SIZE: %i", super_block_->block_size());
    INFO("BLOCK GROUP SIZE: %i", super_block_->block_group_size());
    INFO("N BlOCK GROUPS: %i", super_block_->num_block_groups());
    INFO("INODE SIZE: %i", super_block_->inode_size());
    INFO("INODES PER GROUP: %i", super_block_->inodes_per_group());
  }

  ext2_super_block* super() { return super_block_->get(); }

 private:
  // Super block
  SuperBlock* super_block_;
  // Root inode
  ext2_inode* root_inode_;

  // path mapped to directory entry metadata
  LRUCache<std::string, ext2_dir_entry_2>* dentry_cache_;
  
};
}  // namespace naivefs
#endif
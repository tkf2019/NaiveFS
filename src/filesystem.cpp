#include "filesystem.h"

namespace naivefs {
FileSystem::FileSystem() : super_block_(new SuperBlock()) {
  DEBUG("Initialize file system");
  gettimeofday(&time_, NULL);

  // init first block group
  block_groups_[0] = new BlockGroup(super_block_->get_group_desc(0));

  // init root inode
  if (!block_groups_[0]->get_inode(ROOT_INODE, &root_inode_)) {
    // alloc new root inode
    ASSERT(block_groups_[0]->alloc_inode(&root_inode_));
    root_inode_->i_ctime = time_.tv_sec;
  } else {
    INFO("INODE CREATE TIME: %i", root_inode_->i_ctime);
  }
  
  block_groups_[0]->flush();
}
}  // namespace naivefs
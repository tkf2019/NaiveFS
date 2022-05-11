#include "filesystem.h"

namespace naivefs {

/**
 * @brief Initialize a new inode
 *
 * @param inode inode points to the memory
 */
static void inode_init(ext2_inode* inode) {
  // update time values of root inode
  timeval time;
  gettimeofday(&time, NULL);
  inode->i_ctime = time.tv_sec;
  inode->i_mtime = time.tv_sec;
  inode->i_atime = time.tv_sec;
  // this value represents the total number of 512-byte blocks
  inode->i_blocks = 0;
  // initialized inode only has one link
  inode->i_links_count = 1;
  // for regular file size
  inode->i_size = 0;
  // regular file by default
  inode->i_mode = EXT2_S_IFREG;
  // currently unused fields
  inode->i_uid = 0;
  inode->i_gid = 0;
  inode->i_file_acl = 0;
  inode->i_dir_acl = 0;
  inode->i_faddr = 0;
  inode->i_dtime = 0;
}

/**
 * @brief Display inner data of the inode
 */
static void inode_display(ext2_inode* inode) {
  // timeval time;
  // gettimeofday(&time, NULL);
  // if (time.tv_sec >= inode->i_dtime) {
  //   WARNING("Inode has been DELETED!");
  //   return;
  // }
  INFO("INODE CREATE TIME: %i", inode->i_ctime);
  INFO("INODE MODIFIED TIME: %i", inode->i_mtime);
  INFO("INODE ACCESS TIME: %i", inode->i_atime);
  INFO("INODE NUM BLOCKS: %i", inode->i_blocks);
  if (S_ISDIR(inode->i_mode)) {
    INFO("INODE TYPE: directory");
  } else if (S_ISREG(inode->i_mode)) {
    INFO("INODE TYPE: regular file");
    INFO("INODE FILE SIZE: 0x%x", inode->i_size);
  } else {
    WARNING("Unimplemented inode type!");
  }
}

FileSystem::FileSystem()
    : super_block_(new SuperBlock()),
      block_cache_(new BlockCache(BLOCK_CACHE_SIZE)) {
  DEBUG("Initialize file system");

  // init first block group
  block_groups_[0] = new BlockGroup(super_block_->get_group_desc(0));

  // init root inode
  if (!block_groups_[0]->get_inode(ROOT_INODE, &root_inode_)) {
    // alloc new root inode
    ASSERT(block_groups_[0]->alloc_inode(&root_inode_));
    inode_init(root_inode_);
    // root directory: cannot be written or executed
    root_inode_->i_mode =
        EXT2_S_IFDIR | EXT2_S_IRUSR | EXT2_S_IRGRP | EXT2_S_IROTH;
  } else {
    inode_display(root_inode_);
  }

  block_groups_[0]->flush();
}

bool FileSystem::inode_lookup(const Path& path, ext2_inode** inode) {
  if (path.empty()) {
    *inode = root_inode_;
    return true;
  }
  return false;
}

void FileSystem::visit_inode_blocks(ext2_inode* inode,
                                    const BlockVisitor& visitor) {
  if (S_ISDIR(inode->i_mode) || S_ISREG(inode->i_mode)) {
    uint32_t num_blocks =
        inode->i_blocks / (2 << super_block_->get_super()->s_log_block_size);
    Block *block = nullptr, *indirect_block = nullptr;
    uint32_t curr_num = 0;

    for (int i = 0; i < EXT2_N_BLOCKS; ++i) {
      if (i < EXT2_NDIR_BLOCKS) {
        if (!get_block(inode->i_block[i], &block)) goto error_occured;
        if (visitor(block)) goto visit_finished;
        if (++curr_num == num_blocks) goto visit_finished;
      } else if (i == EXT2_IND_BLOCK) {
        if (!get_block(inode->i_block[i], &block)) goto error_occured;
        ASSERT(num_blocks - curr_num == EXT2_NDIR_BLOCKS);

        uint32_t* ptr = (uint32_t*)block->get();
        uint32_t* end = ptr + NUM_INDIRECT_BLOCKS;

        while (curr_num < num_blocks && ptr != end) {
          uint32_t indirect_index = *ptr;
          if (!get_block(indirect_index, &indirect_block)) goto error_occured;
          if (visitor(indirect_block)) goto visit_finished;
          ptr++;
          curr_num++;
        }

        if (curr_num == num_blocks) goto visit_finished;
      } else if (i == EXT2_DIND_BLOCK) {
        if (!get_block(inode->i_block[i], &block)) goto error_occured;
        ASSERT(num_blocks - curr_num == EXT2_NDIR_BLOCKS);

        uint32_t* ptr = (uint32_t*)block->get();
        uint32_t* end = ptr + NUM_INDIRECT_BLOCKS;

        while (curr_num < num_blocks && ptr != end) {
          Block* indirect_block;
          uint32_t indirect_index = *ptr;
          if (!get_block(indirect_index, &indirect_block)) goto error_occured;

          uint32_t* dptr = (uint32_t*)indirect_block->get();
          uint32_t* dend = dptr + NUM_INDIRECT_BLOCKS;
          while (curr_num < num_blocks && dptr != dend) {
            Block* double_indirect_block;
            uint32_t double_indirect_index = *dptr;
            if (!get_block(double_indirect_index, &double_indirect_block))
              goto error_occured;
            if (visitor(double_indirect_block)) goto visit_finished;
            dptr++;
            curr_num++;
          }

          ptr++;
        }
        if (curr_num == num_blocks) goto visit_finished;
      } else if (i == EXT2_TIND_BLOCK) {
        WARNING("Not supported: Too big file!");
        goto error_occured;
      }
    }
  visit_finished:
    DEBUG("Finished visiting inode() blocks: current index(%)");
    return;
  error_occured:
    ERR("Error occured while visiting inode blocks!");
    return;
  } else {
    WARNING("Unimplemented inode type!");
    return;
  }
}

bool FileSystem::get_inode(uint32_t index, ext2_inode** inode) {
  if (index >= super_block_->get_super()->s_inodes_count) {
    WARNING("Inode index exceeds inodes count");
    return false;
  }
  // lasy read
  uint32_t block_group_index = index / super_block_->inodes_per_group();
  if (block_groups_.find(block_group_index) == block_groups_.end()) {
    block_groups_[block_group_index] =
        new BlockGroup(super_block_->get_group_desc(block_group_index));
  }
  uint32_t inner_index = index % super_block_->inodes_per_group();
  if (!block_groups_[block_group_index]->get_inode(inner_index, inode)) {
    WARNING("Inode has not been allocated in the target block group");
    return false;
  }
  return true;
}

bool FileSystem::get_block(uint32_t index, Block** block) {
  if (index >= super_block_->get_super()->s_blocks_count) {
    WARNING("Block index exceeds blocks count");
    return false;
  }
  // find in block cache
  *block = block_cache_->get(index);
  if (*block == nullptr) {
    // lasy read
    uint32_t block_group_index = index / super_block_->blocks_per_group();
    if (block_groups_.find(block_group_index) == block_groups_.end()) {
      block_groups_[block_group_index] =
          new BlockGroup(super_block_->get_group_desc(block_group_index));
    }
    uint32_t inner_index = index % super_block_->blocks_per_group();
    if (!block_groups_[block_group_index]->get_block(inner_index, block)) {
      WARNING("Block has not been allocated in the target block group");
      return false;
    }
    // Update block cache
    block_cache_->insert(index, *block);
  }
  return true;
}

}  // namespace naivefs
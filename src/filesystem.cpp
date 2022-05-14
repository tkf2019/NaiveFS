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
 * Display inner data of the inode
 */
static void inode_display(ext2_inode* inode) {
  INFO("INODE CREATE TIME: %u", inode->i_ctime);
  INFO("INODE MODIFIED TIME: %u", inode->i_mtime);
  INFO("INODE ACCESS TIME: %u", inode->i_atime);
  INFO("INODE NUM BLOCKS: %u", inode->i_blocks);
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
      block_cache_(new BlockCache(BLOCK_CACHE_SIZE)),
      dentry_cache_(new DentryCache(BLOCK_CACHE_SIZE)) {
  DEBUG("Initialize file system");

  // init first block group
  block_groups_[0] = new BlockGroup(super_block_->get_group_desc(0));

  // init root inode
  if (!block_groups_[0]->get_inode(ROOT_INODE, &root_inode_)) {
    // alloc new root inode
    ASSERT(block_groups_[0]->alloc_inode(&root_inode_, nullptr, true));
    inode_init(root_inode_);
    // root directory: cannot be written or executed
    root_inode_->i_mode =
        EXT2_S_IFDIR | EXT2_S_IRUSR | EXT2_S_IRGRP | EXT2_S_IROTH;
  } else {
    inode_display(root_inode_);
  }

  block_groups_[0]->flush();
}

FileSystem::~FileSystem() {
  delete root_inode_;

  super_block_->flush();
  delete super_block_;

  for (auto bg : block_groups_) {
    bg.second->flush();
    delete bg.second;
  }

  delete block_cache_;
  delete dentry_cache_;
}

void FileSystem::flush() {
  super_block_->flush();

  for (auto bg : block_groups_) {
    bg.second->flush();
  }

  block_cache_->flush();
}

bool FileSystem::inode_create(const Path& path, ext2_inode** inode, bool dir) {
  ext2_inode* parent;
  if (!inode_lookup(Path(path, path.size() - 1), &parent)) {
    WARNING("Directory does not exist!");
    return false;
  }

  Block* last_block = nullptr;
  uint32_t last_block_index;
  auto last_item = path.get(path.size() - 1);

  // Check if name already exists
  uint32_t inode_index;
  bool name_exists = false;
  visit_inode_blocks(
      parent, [&inode_index, &name_exists, &last_block, &last_block_index,
               last_item](uint32_t index, Block* block) {
        DentryBlock dentry_block(block);
        for (auto dentry : *dentry_block.get()) {
          if (dentry->name_len != last_item.second) continue;
          if (memcmp(last_item.first, dentry->name, dentry->name_len)) continue;
          // find a matched directory entry
          name_exists = true;
          inode_index = dentry->inode;
        }

        last_block = block;
        last_block_index = index;
        return false;
      });
  if (name_exists) {
    WARNING("Create a duplicated inode: %u", inode_index);
    if (!get_inode(inode_index, inode)) return false;
    return true;
  }

  // allocate new inode
  if (!alloc_inode(inode, &inode_index, dir)) return false;

  // get parent last dentry block
  if (last_block == nullptr) {
    if (!alloc_block(&last_block, &last_block_index, parent)) {
      return false;
    }
  }
  DentryBlock* dentry_block = new DentryBlock(last_block);

  // update dentry block
  if (dentry_block->size() + sizeof(ext2_dir_entry_2) + last_item.second >
      BLOCK_SIZE) {
    if (!alloc_block(&last_block, &last_block_index, parent)) return false;
    delete dentry_block;
    dentry_block = new DentryBlock(last_block);
  }
  dentry_block->alloc_dentry(last_item.first, last_item.second, inode_index,
                             dir);
  block_cache_->modify(last_block_index);
  // We cannot put the new dentry into cache because we do not know the
  // parent. We add the dentry into cache after next lookup.
  delete dentry_block;
  return true;
}

bool FileSystem::inode_lookup(const Path& path, ext2_inode** inode,
                              uint32_t* inode_index) {
  *inode = root_inode_;
  if (path.empty()) {
    return true;
  }
  DentryCache::Node* link = nullptr;
  size_t curr_index = 0;
  bool cache_hit = false;
  int64_t result = -1;
  for (const auto& elem : path) {
    auto node = dentry_cache_->lookup(link, elem.first, elem.second);
    if (node == nullptr) {
      // get inode data if cache hits
      if (curr_index > 0 && cache_hit) {
        if (!get_inode(link->inode_, inode)) {
          WARNING("INODE should exist with a valid directory entry!");
          return false;
        }
      }
      visit_inode_blocks(
          *inode, [this, elem, &result, &inode](uint32_t index, Block* block) {
            DentryBlock dentry_block(block);
            for (auto dentry : *dentry_block.get()) {
              if (dentry->name_len != elem.second) continue;
              if (memcmp(elem.first, dentry->name, dentry->name_len)) continue;
              if (!this->get_inode(dentry->inode, inode)) {
                WARNING("INODE should exist with a valid directory entry!");
                result = -1;
                return true;
              }
              // find a matched directory entry
              result = dentry->inode;
              return true;
            }
            return false;
          });
      if (result == -1) {
        *inode = nullptr;
        return false;
      }
      // update dentry cache
      cache_hit = false;
      link = dentry_cache_->insert(link, elem.first, elem.second, result);
    } else {
      cache_hit = true;
      link = node;
      result = link->inode_;
      if (curr_index == path.size() - 1) {
        if (!get_inode(link->inode_, inode)) {
          WARNING("INODE should exist with a valid directory entry!");
          return false;
        }
      }
    }
    curr_index++;
  }
  if (inode_index != nullptr) *inode_index = result;
  return true;
}

void FileSystem::visit_inode_blocks(ext2_inode* inode,
                                    const BlockVisitor& visitor) {
  ASSERT(inode != nullptr &&
         (S_ISDIR(inode->i_mode) || S_ISREG(inode->i_mode)));
  uint32_t num_blocks = super_block_->num_aligned_blocks(inode->i_blocks);
  if (num_blocks == 0) return;
  Block *block = nullptr, *indirect_block = nullptr;
  uint32_t curr_num = 0;

  for (int i = 0; i < EXT2_N_BLOCKS; ++i) {
    if (i < EXT2_NDIR_BLOCKS) {
      if (!get_block(inode->i_block[i], &block)) goto error_occured;
      if (visitor(inode->i_block[i], block)) goto visit_finished;
      if (++curr_num == num_blocks) goto visit_finished;
    } else if (i == EXT2_IND_BLOCK) {
      if (!get_block(inode->i_block[i], &block)) goto error_occured;
      ASSERT(num_blocks - curr_num == MAX_DIR_BLOCKS);

      uint32_t* ptr = (uint32_t*)block->get();
      uint32_t* end = ptr + NUM_INDIRECT_BLOCKS;

      while (curr_num < num_blocks && ptr != end) {
        uint32_t indirect_index = *ptr;
        if (!get_block(indirect_index, &indirect_block)) goto error_occured;
        if (visitor(indirect_index, indirect_block)) goto visit_finished;
        ptr++;
        curr_num++;
      }

      if (curr_num == num_blocks) goto visit_finished;
    } else if (i == EXT2_DIND_BLOCK) {
      if (!get_block(inode->i_block[i], &block)) goto error_occured;
      ASSERT(num_blocks - curr_num == MAX_DIR_BLOCKS + MAX_IND_BLOCKS);

      uint32_t* ptr = (uint32_t*)block->get();
      uint32_t* end = ptr + NUM_INDIRECT_BLOCKS;

      while (curr_num < num_blocks && ptr != end) {
        uint32_t indirect_index = *ptr;
        if (!get_block(indirect_index, &indirect_block)) goto error_occured;

        uint32_t* dptr = (uint32_t*)indirect_block->get();
        uint32_t* dend = dptr + NUM_INDIRECT_BLOCKS;
        while (curr_num < num_blocks && dptr != dend) {
          Block* double_indirect_block;
          uint32_t double_indirect_index = *dptr;
          if (!get_block(double_indirect_index, &double_indirect_block))
            goto error_occured;
          if (visitor(double_indirect_index, double_indirect_block))
            goto visit_finished;
          dptr++;
          curr_num++;
        }
        if (curr_num == num_blocks) goto visit_finished;
        ptr++;
      }
      if (curr_num == num_blocks) goto visit_finished;
    } else if (i == EXT2_TIND_BLOCK) {
      WARNING("Not supported: Too big file!");
      goto error_occured;
    }
  }
visit_finished:
  DEBUG("Finished visiting inode blocks: current index %u", curr_num);
  return;
error_occured:
  WARNING("Error occured while visiting inode blocks!");
  return;
}

bool FileSystem::get_inode(uint32_t index, ext2_inode** inode) {
  if (index >= super_block_->get_super()->s_inodes_count) {
    WARNING("Inode index exceeds inodes count");
    return false;
  }
  // lazy read
  uint32_t block_group_index = index / super_block_->inodes_per_group();
  auto iter = block_groups_.find(block_group_index);
  if (iter == block_groups_.end()) {
    iter = block_groups_
               .insert({block_group_index,
                        new BlockGroup(
                            super_block_->get_group_desc(block_group_index))})
               .first;
  }
  uint32_t inner_index = index % super_block_->inodes_per_group();
  if (!iter->second->get_inode(inner_index, inode)) {
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
    auto iter = block_groups_.find(block_group_index);
    if (iter == block_groups_.end()) {
      iter = block_groups_
                 .insert({block_group_index,
                          new BlockGroup(
                              super_block_->get_group_desc(block_group_index))})
                 .first;
    }
    uint32_t inner_index = index % super_block_->blocks_per_group();
    if (!iter->second->get_block(inner_index, block)) {
      WARNING("Block has not been allocated in the target block group");
      return false;
    }
    // Update block cache
    block_cache_->insert(index, *block);
  }
  return true;
}

bool FileSystem::alloc_inode(ext2_inode** inode, uint32_t* index, bool dir) {
  // update super block
  super_block_->get_super()->s_free_inodes_count--;
  super_block_->get_super()->s_inodes_count++;

  uint32_t block_group_index;
  // allocated by block group
  for (auto bg : block_groups_) {
    if (bg.second->get_desc()->bg_free_inodes_count) {
      if (bg.second->alloc_inode(inode, index, dir)) {
        inode_init(*inode);
        block_group_index = bg.first;
        goto alloc_finished;
      }
    }
  }

  // create a new block group
  alloc_block_group(&block_group_index);
  if (block_groups_[block_group_index]->alloc_inode(inode, index, dir))
    goto alloc_finished;

  WARNING("Allocate inode in the new block group(%u) failed",
          block_group_index);
  return false;

alloc_finished:
  // must be converted to the index of the whole file system
  *index = block_group_index * super_block_->inodes_per_group() + *index;
  DEBUG("Allocate new inode %u in block group %u", *index, block_group_index);
  return true;
}

bool FileSystem::alloc_block(Block** block, uint32_t* index) {
  ASSERT(block != nullptr && index != nullptr);
  // update super block
  super_block_->get_super()->s_free_blocks_count--;
  super_block_->get_super()->s_blocks_count++;

  uint32_t block_group_index;
  // allocated by block group
  for (auto bg : block_groups_) {
    if (bg.second->get_desc()->bg_free_blocks_count) {
      if (bg.second->alloc_block(block, index)) {
        block_group_index = bg.first;
        goto alloc_finished;
      }
    }
  }

  // create a new block group
  alloc_block_group(&block_group_index);
  if (block_groups_[block_group_index]->alloc_block(block, index))
    goto alloc_finished;

  WARNING("Failed to allocate block in the new block group %u",
          block_group_index);
  return false;

alloc_finished:
  // must be converted to the index of the whole file system
  *index = block_group_index * super_block_->blocks_per_group() + *index;
  // add to block cache
  block_cache_->insert(*index, *block);
  DEBUG("Allocate new block %u in block group %u", *index, block_group_index);
  return true;
}

bool FileSystem::alloc_block(Block** block, uint32_t* index,
                             ext2_inode* inode) {
  ASSERT(inode != nullptr &&
         (S_ISDIR(inode->i_mode) || S_ISREG(inode->i_mode)));
  uint32_t block_index, indirect_block_index;
  uint32_t num_blocks = super_block_->num_aligned_blocks(inode->i_blocks);
  Block* indirect_block = nullptr;
  if (!alloc_block(block, &block_index)) goto error_occured;

  if (num_blocks < MAX_DIR_BLOCKS) {
    inode->i_blocks += 2 << super_block_->get_super()->s_log_block_size;
    inode->i_block[num_blocks] = block_index;
  } else if (num_blocks < MAX_DIR_BLOCKS + MAX_IND_BLOCKS) {
    if (num_blocks == MAX_DIR_BLOCKS) {
      if (!(alloc_block(&indirect_block, &indirect_block_index)))
        goto error_occured;
      inode->i_block[EXT2_IND_BLOCK] = indirect_block_index;
    } else if (!get_block(inode->i_block[EXT2_IND_BLOCK], &indirect_block))
      goto error_occured;
    uint32_t* ptr = (uint32_t*)indirect_block->get();
    *(ptr + (num_blocks - MAX_DIR_BLOCKS)) = block_index;
    inode->i_blocks += 2 << super_block_->get_super()->s_log_block_size;
    // update block cache
    block_cache_->modify(inode->i_block[EXT2_IND_BLOCK]);
  } else if (num_blocks < MAX_DIR_BLOCKS + MAX_IND_BLOCKS + MAX_DIND_BLOCKS) {
    if (num_blocks == MAX_DIR_BLOCKS + MAX_IND_BLOCKS) {
      if (!(alloc_block(&indirect_block, &indirect_block_index)))
        goto error_occured;
      inode->i_block[EXT2_DIND_BLOCK] = indirect_block_index;
    } else if (!get_block(inode->i_block[EXT2_DIND_BLOCK], &indirect_block))
      goto error_occured;

    uint32_t num_indirects =
        (num_blocks - MAX_DIR_BLOCKS - MAX_IND_BLOCKS) / NUM_INDIRECT_BLOCKS;
    ASSERT(num_indirects <= NUM_INDIRECT_BLOCKS);
    uint32_t inner_index =
        (num_blocks - MAX_DIR_BLOCKS - MAX_IND_BLOCKS) % NUM_INDIRECT_BLOCKS;
    uint32_t double_indirect_index;
    Block* double_indirect_block;
    if (inner_index == 0) {
      // double indirect blocks has been full
      if (!alloc_block(&double_indirect_block, &double_indirect_index))
        goto error_occured;
      uint32_t* ptr = (uint32_t*)double_indirect_block->get();
      *ptr = block_index;
      inode->i_blocks += 2 << super_block_->get_super()->s_log_block_size;
      // update new double indirect block
      block_cache_->modify(double_indirect_index);
    } else {
      uint32_t* ptr = (uint32_t*)indirect_block->get();
      double_indirect_index = *(ptr + num_indirects - 1);
      if (!get_block(double_indirect_index, &double_indirect_block))
        goto error_occured;
      uint32_t* double_ptr = (uint32_t*)double_indirect_block->get();
      *(double_ptr + inner_index) = block_index;
      inode->i_blocks += 2 << super_block_->get_super()->s_log_block_size;
      // update new double indirect block
      block_cache_->modify(double_indirect_index);
    }
  } else {
    WARNING("Not supported: Too big file!");
    goto error_occured;
  }
  *index = block_index;
  return true;

error_occured:
  WARNING("Error occured while allocating inode blocks!");
  return false;
}

bool FileSystem::alloc_block_group(uint32_t* index) {
  *index = super_block_->num_block_groups();
  // We assume disk space will not drain out
  ASSERT(sizeof(ext2_super_block) + *index * sizeof(ext2_group_desc) <=
         BLOCK_SIZE);
  ext2_group_desc* desc =
      (ext2_group_desc*)(super_block_->get() + sizeof(ext2_super_block) +
                         *index * sizeof(ext2_group_desc));
  desc->bg_inode_bitmap = BLOCK_SIZE;
  desc->bg_block_bitmap = desc->bg_inode_bitmap + BLOCK_SIZE;
  desc->bg_inode_table = desc->bg_block_bitmap + BLOCK_SIZE;
  desc->bg_free_blocks_count = BLOCKS_PER_GROUP;
  desc->bg_free_inodes_count = INODES_PER_GROUP;
  desc->bg_used_dirs_count = 0;
  super_block_->put_group_desc(desc);
  block_groups_[*index] = new BlockGroup(desc);
  DEBUG("Allocate new block group: %u", *index);
  return true;
}

bool FileSystem::seek_last_block(ext2_inode* inode, Block** block,
                                 uint32_t* index) {
  if (inode == nullptr) return false;
  Block* last_block = nullptr;
  uint32_t last_index;
  // TODO: it can be faster (skip blocks)
  visit_inode_blocks(inode,
                     [&last_block, &last_index](uint32_t index, Block* block) {
                       last_block = block;
                       last_index = index;
                       return false;
                     });
  *block = last_block;
  *index = last_index;
  return true;
}

}  // namespace naivefs
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
    ASSERT(block_groups_[0]->alloc_inode(&root_inode_, nullptr, S_IFDIR));
    inode_init(root_inode_);
    // root directory: cannot be written or executed
    root_inode_->i_mode =
        EXT2_S_IFDIR | EXT2_S_IRUSR | EXT2_S_IRGRP | EXT2_S_IROTH;
  } else {
    inode_display(root_inode_);
  }

  block_groups_[0]->flush();
  DEBUG("File system has been initialized");
}

FileSystem::~FileSystem() {
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

RetCode FileSystem::inode_create(const Path& path, ext2_inode** inode,
                                 uint32_t* inode_index_result, mode_t mode) {
  if (!path.valid()) return FS_INVALID;
  // cannot create root inode
  if (path.empty()) return FS_DUP_ERR;

  ext2_inode* parent;
  if (!inode_index_result) return FS_NULL_ERR;
  RetCode lookup_ret = inode_lookup(Path(path, path.size() - 1), &parent);
  if (lookup_ret) return lookup_ret;
  if (!S_ISDIR(parent->i_mode)) return FS_NDIR_ERR;

  // Check if name already exists
  Block* last_block = nullptr;
  uint32_t last_block_index;
  auto last_item = path.back();
  uint32_t inode_index;
  bool name_exists = false;
  lookup_ret =
      inode_lookup(parent, last_item.first, last_item.second, &name_exists,
                   &inode_index, &last_block, &last_block_index);
  if (lookup_ret) return lookup_ret;
  if (name_exists) {
    WARNING("Create a duplicated inode: %u", inode_index);
    *inode_index_result = inode_index;
    if (!get_inode(inode_index, inode)) return FS_NOT_FOUND;
    return FS_DUP_ERR;
  }

  // allocate new inode
  if (!alloc_inode(inode, &inode_index, mode)) return FS_ALLOC_ERR;

  RetCode dentry_ret =
      dentry_create(last_block, last_block_index, parent, last_item.first,
                    last_item.second, inode_index, mode);
  if (dentry_ret) return dentry_ret;

  DEBUG("Create inode: %i,%s", inode_index,
        std::string(path.back().first).c_str());
  *inode_index_result = inode_index;
  return FS_SUCCESS;
}

RetCode FileSystem::dentry_create(Block* last_block, uint32_t last_block_index,
                                  ext2_inode* parent, const char* name,
                                  size_t name_len, uint32_t inode_index,
                                  mode_t mode) {
  if (last_block == nullptr) {
    if (!alloc_block(&last_block, &last_block_index, parent)) {
      return FS_ALLOC_ERR;
    }
  }
  DentryBlock* dentry_block = new DentryBlock(last_block);

  // update dentry block
  if (dentry_block->size() + sizeof(ext2_dir_entry_2) + name_len > BLOCK_SIZE) {
    if (!alloc_block(&last_block, &last_block_index, parent))
      return FS_ALLOC_ERR;
    delete dentry_block;
    dentry_block = new DentryBlock(last_block);
  }
  dentry_block->alloc_dentry(name, name_len, inode_index, mode);
  block_cache_->modify(last_block_index);
  // We cannot put the new dentry into cache because we do not know the
  // parent. We add the dentry into cache after next lookup.
  delete dentry_block;

  return FS_SUCCESS;
}

RetCode FileSystem::inode_lookup(const Path& path, ext2_inode** inode,
                                 uint32_t* inode_index,
                                 DentryCache::Node** cache_ptr) {
  if (!path.valid()) return FS_INVALID;
  *inode = root_inode_;
  if (path.empty()) {
    if (inode_index != nullptr) *inode_index = ROOT_INODE;
    if (cache_ptr != nullptr) *cache_ptr = nullptr;
    return FS_SUCCESS;
  }
  DentryCache::Node* link = nullptr;
  DentryCache::Node* node = nullptr;
  DentryCache::Node* parent = nullptr;
  size_t curr_index = 0;
  bool cache_hit = false;
  int64_t result = -1;
  for (const auto& elem : path) {
    node = dentry_cache_->lookup(link, elem.first, elem.second);
    if (node == nullptr) {
      // get inode data if cache hits
      if (curr_index > 0 && cache_hit) {
        if (!get_inode(link->inode_, inode)) {
          WARNING("INODE should exist with a valid directory entry!");
          return FS_NOT_FOUND;
        }
      }
      // final item can be a file or a directory
      if (curr_index < path.size() - 1 && !S_ISDIR((*inode)->i_mode))
        return FS_NDIR_ERR;

      result = -1;
      visit_inode_blocks(
          *inode, [this, elem, &result, &inode](
                      __attribute__((unused)) uint32_t index, Block* block) {
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
        return FS_NOT_FOUND;
      }
      // update dentry cache
      cache_hit = false;
      parent = link;
      link = dentry_cache_->insert(link, elem.first, elem.second, result);
    } else {
      cache_hit = true;
      parent = link;
      link = node;
      result = link->inode_;
      if (curr_index == path.size() - 1) {
        if (!get_inode(link->inode_, inode)) {
          WARNING("INODE should exist with a valid directory entry!");
          return FS_NOT_FOUND;
        }
      }
    }
    curr_index++;
  }
  if (inode_index != nullptr) *inode_index = result;
  if (cache_ptr != nullptr) *cache_ptr = parent;
  return FS_SUCCESS;
}

RetCode FileSystem::inode_lookup(ext2_inode* parent, const char* name,
                                 size_t name_len, bool* name_exists,
                                 uint32_t* inode_index, Block** last_block,
                                 uint32_t* last_block_index) {
  auto visitor = [name, name_len, &inode_index, &name_exists, &last_block,
                  &last_block_index](uint32_t index, Block* block) {
    DentryBlock dentry_block(block);
    for (auto dentry : *dentry_block.get()) {
      if (dentry->name_len != name_len) continue;
      if (memcmp(name, dentry->name, dentry->name_len)) continue;
      // find a matched directory entry
      if (name_exists != nullptr) *name_exists = true;
      if (inode_index != nullptr) *inode_index = dentry->inode;
    }

    if (last_block != nullptr) *last_block = block;
    if (last_block_index != nullptr) *last_block_index = index;
    return false;
  };
  visit_inode_blocks(parent, visitor);
  return FS_SUCCESS;
}

RetCode FileSystem::inode_delete(uint32_t index) {
  if (index == ROOT_INODE) return FS_INVALID;
  ext2_inode* inode;
  if (!get_inode(index, &inode)) return FS_NOT_FOUND;
  if (!S_ISREG(inode->i_mode) && !S_ISDIR(inode->i_mode)) return FS_NOT_FOUND;

  if (S_ISDIR(inode->i_mode)) {
    visit_inode_blocks(
        inode, [this](__attribute__((unused)) uint32_t index, Block* block) {
          DentryBlock dentry_block(block);
          for (auto dentry : *dentry_block.get()) {
            // delete an existing inode
            if (dentry->name_len != 0) {
              inode_delete(dentry->inode);
            }
          }
          return false;
        });
  } else {
    visit_inode_blocks(inode, [this](__attribute__((unused)) uint32_t index,
                                     __attribute__((unused)) Block* block) {
      free_block(index);
      return false;
    });
  }

  free_inode(index);
  return FS_SUCCESS;
}

RetCode FileSystem::inode_unlink(const Path& path) {
  if (!path.valid()) return FS_INVALID;
  // cannot delete root inode
  if (path.empty()) return FS_INVALID;

  DentryCache::Node* parent_dentry;
  ext2_inode* parent;
  Path dir_path = Path(path, path.size() - 1);
  RetCode lookup_ret = inode_lookup(dir_path, &parent, nullptr, &parent_dentry);
  if (lookup_ret) return lookup_ret;
  if (!S_ISDIR(parent->i_mode)) return FS_NDIR_ERR;

  auto last_item = path.back();
  uint32_t dentry_block_index;
  bool name_exists = false;
  uint32_t matched_index;

  visit_inode_blocks(
      parent, [last_item, &dentry_block_index, &name_exists, &matched_index](
                  __attribute__((unused)) uint32_t index, Block* block) {
        DentryBlock dentry_block(block);
        for (auto dentry : *dentry_block.get()) {
          if (dentry->name_len != last_item.second) continue;
          if (memcmp(last_item.first, dentry->name, dentry->name_len)) continue;
          // delete directory entry by setting the name length to 0
          dentry->name_len = 0;
          dentry_block_index = index;
          name_exists = true;
          matched_index = dentry->inode;
          return true;
        }
        return false;
      });
  if (!name_exists) return FS_NOT_FOUND;

  // update block cache
  block_cache_->modify(dentry_block_index);

  // release dentry cache node
  auto parent_item = dir_path.back();
  auto cache_ptr = dentry_cache_->lookup(parent_dentry, parent_item.first,
                                         parent_item.second);
  dentry_cache_->remove(cache_ptr, last_item.first, last_item.second);

  // release inode
  ext2_inode* inode;
  if (!get_inode(matched_index, &inode)) return FS_NOT_FOUND;
  inode->i_links_count--;
  if (inode->i_links_count == 0) {
    RetCode delete_ret = inode_delete(matched_index);
    if (delete_ret) return delete_ret;
    DEBUG("Delete inode: %i,%s", matched_index,
          std::string(last_item.first, last_item.second).c_str());
  }

  DEBUG("Unlink: %s", path.path());
  return FS_SUCCESS;
}

RetCode FileSystem::inode_link(const Path& src, const Path& dst) {
  if (!src.valid() || !dst.valid()) return FS_INVALID;
  // cannot create link from or to root inode
  if (src.empty() || dst.empty()) return FS_DUP_ERR;

  uint32_t inode_index;
  ext2_inode* src_inode;
  // look up source inode
  RetCode lookup_ret = inode_lookup(src, &src_inode, &inode_index);
  if (lookup_ret) return lookup_ret;
  if (S_ISDIR(src_inode->i_mode)) return FS_DIR_ERR;

  ext2_inode* parent;
  lookup_ret = inode_lookup(Path(dst, dst.size() - 1), &parent);
  if (lookup_ret) return lookup_ret;
  if (!S_ISDIR(parent->i_mode)) return FS_NDIR_ERR;

  // Check if name already exists
  Block* last_block = nullptr;
  uint32_t last_block_index;
  auto last_item = dst.back();
  bool name_exists = false;
  lookup_ret =
      inode_lookup(parent, last_item.first, last_item.second, &name_exists,
                   &inode_index, &last_block, &last_block_index);
  if (lookup_ret) return lookup_ret;
  if (name_exists) {
    WARNING("Cannot link with a duplicated dentry");
    return FS_DUP_ERR;
  }

  // Inode index will bot be changed after looking up source inode if name to
  // create is not in duplicate with existing dentries.
  RetCode dentry_ret =
      dentry_create(last_block, last_block_index, parent, last_item.first,
                    last_item.second, inode_index, src_inode->i_mode);
  if (dentry_ret) return dentry_ret;

  // update source inode
  src_inode->i_links_count++;

  DEBUG("Create link: %s => %s", dst.path(), src.path());
  return FS_SUCCESS;
}

bool FileSystem::visit_indirect_blocks(Block* indirect_block, uint32_t num,
                                       const BlockVisitor& visitor) {
  ASSERT(indirect_block != nullptr);
  uint32_t* ptr = (uint32_t*)indirect_block->get();
  uint32_t* end = ptr + NUM_INDIRECT_BLOCKS;
  uint32_t curr_num = 0;
  Block* block = nullptr;

  while (curr_num < num && ptr != end) {
    uint32_t indirect_index = *ptr;
    if (!get_block(indirect_index, &block)) return false;
    if (visitor(indirect_index, block)) return true;
    ptr++;
    curr_num++;
  }

  return true;
}

void FileSystem::visit_inode_blocks(ext2_inode* inode,
                                    const BlockVisitor& visitor) {
  ASSERT(inode != nullptr &&
         (S_ISDIR(inode->i_mode) || S_ISREG(inode->i_mode)));
  uint32_t num_blocks = super_block_->num_aligned_blocks(inode->i_blocks);
  if (num_blocks == 0) return;
  Block* block = nullptr;
  Block* indirect_block = nullptr;
  uint32_t curr_num = 0;

  auto indirect_visitor = [this, visitor, num_blocks, &curr_num](
                              __attribute__((unused)) uint32_t index,
                              Block* block) {
    visit_indirect_blocks(block, num_blocks - curr_num, visitor);
    curr_num += std::min(num_blocks - curr_num, (uint32_t)NUM_INDIRECT_BLOCKS);
    return false;
  };

  auto double_indirect_visitor =
      [this, indirect_visitor, num_blocks, &curr_num](
          __attribute__((unused)) uint32_t index, Block* block) {
        visit_indirect_blocks(block, num_blocks - curr_num, indirect_visitor);
        return false;
      };

  for (int i = 0; i < EXT2_N_BLOCKS; ++i) {
    if (i < EXT2_NDIR_BLOCKS) {
      if (!get_block(inode->i_block[i], &block)) goto error_occured;
      if (visitor(inode->i_block[i], block)) goto visit_finished;
      if (++curr_num == num_blocks) goto visit_finished;
    } else if (i == EXT2_IND_BLOCK) {
      ASSERT(curr_num == MAX_DIR_BLOCKS);
      if (!get_block(inode->i_block[i], &indirect_block)) goto error_occured;
      if (!visit_indirect_blocks(indirect_block, num_blocks - curr_num,
                                 visitor))
        goto error_occured;
      curr_num += std::min(num_blocks - curr_num, (uint32_t)MAX_IND_BLOCKS);
      if (curr_num == num_blocks) goto visit_finished;
    } else if (i == EXT2_DIND_BLOCK) {
      ASSERT(curr_num == MAX_DIR_BLOCKS + MAX_IND_BLOCKS);
      if (!get_block(inode->i_block[i], &indirect_block)) goto error_occured;
      uint32_t num_indirects =
          ((num_blocks - curr_num) + NUM_INDIRECT_BLOCKS - 1) /
          NUM_INDIRECT_BLOCKS;
      if (!visit_indirect_blocks(indirect_block, num_indirects,
                                 indirect_visitor))
        goto error_occured;
      if (curr_num == num_blocks) goto visit_finished;
    } else if (i == EXT2_TIND_BLOCK) {
      ASSERT(curr_num == MAX_DIR_BLOCKS + MAX_IND_BLOCKS + MAX_DIND_BLOCKS);
      if (!get_block(inode->i_block[i], &indirect_block)) goto error_occured;
      uint32_t num_indirects =
          ((num_blocks - curr_num) + MAX_DIND_BLOCKS - 1) / MAX_DIND_BLOCKS;
      if (!visit_indirect_blocks(indirect_block, num_indirects,
                                 double_indirect_visitor))
        goto error_occured;
      if (curr_num == num_blocks) goto visit_finished;
    }
  }
visit_finished:
  DEBUG("Finished visiting %u inode blocks", curr_num);
  return;
error_occured:
  WARNING("Error occured while visiting inode blocks!");
  return;
}

bool FileSystem::get_inode(uint32_t index, ext2_inode** inode) {
  // INFO("get inode: %d", index);
  if (index == -1) {
    *inode = root_inode_;
    return true;
  }
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
  // INFO("get inode: %d, return true", index);
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

bool FileSystem::alloc_inode(ext2_inode** inode, uint32_t* index, mode_t mode) {
  // update super block
  super_block_->get_super()->s_free_inodes_count--;
  super_block_->get_super()->s_inodes_count++;

  uint32_t block_group_index;
  // allocated by block group
  for (auto bg : block_groups_) {
    if (bg.second->get_desc()->bg_free_inodes_count) {
      if (bg.second->alloc_inode(inode, index, mode)) {
        inode_init(*inode);
        block_group_index = bg.first;
        goto alloc_finished;
      }
    }
  }

  // create a new block group
  alloc_block_group(&block_group_index);
  if (block_groups_[block_group_index]->alloc_inode(inode, index, mode))
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
    inode->i_block[num_blocks] = block_index;
  } else if (num_blocks < MAX_DIR_BLOCKS + MAX_IND_BLOCKS) {
    if (num_blocks == MAX_DIR_BLOCKS) {
      if (!alloc_block(&indirect_block, &indirect_block_index))
        goto error_occured;
      inode->i_block[EXT2_IND_BLOCK] = indirect_block_index;
    } else if (!get_block(inode->i_block[EXT2_IND_BLOCK], &indirect_block))
      goto error_occured;

    uint32_t* ptr = (uint32_t*)indirect_block->get();
    *(ptr + (num_blocks - MAX_DIR_BLOCKS)) = block_index;
    // update block cache
    block_cache_->modify(inode->i_block[EXT2_IND_BLOCK]);
  } else if (num_blocks < MAX_DIR_BLOCKS + MAX_IND_BLOCKS + MAX_DIND_BLOCKS) {
    if (num_blocks == MAX_DIR_BLOCKS + MAX_IND_BLOCKS) {
      if (!alloc_block(&indirect_block, &indirect_block_index))
        goto error_occured;
      inode->i_block[EXT2_DIND_BLOCK] = indirect_block_index;
    } else if (!get_block(inode->i_block[EXT2_DIND_BLOCK], &indirect_block))
      goto error_occured;

    uint32_t num_indirects =
        (num_blocks - MAX_DIR_BLOCKS - 1) / NUM_INDIRECT_BLOCKS;
    ASSERT(num_indirects <= NUM_INDIRECT_BLOCKS);
    uint32_t inner_index =
        (num_blocks - MAX_DIR_BLOCKS - MAX_IND_BLOCKS) % NUM_INDIRECT_BLOCKS;
    uint32_t double_indirect_index;
    Block* double_indirect_block;

    if (inner_index == 0) {  // double indirect blocks has been full
      // allocate new double indirect block
      if (!alloc_block(&double_indirect_block, &double_indirect_index))
        goto error_occured;

      // update indirect block
      uint32_t* ptr = (uint32_t*)indirect_block->get();
      *(ptr + num_indirects) = double_indirect_index;
      block_cache_->modify(inode->i_block[EXT2_DIND_BLOCK]);

      // update new double indirect block
      ptr = (uint32_t*)double_indirect_block->get();
      *ptr = block_index;
      block_cache_->modify(double_indirect_index);

    } else {
      uint32_t* ptr = (uint32_t*)indirect_block->get();
      double_indirect_index = *(ptr + num_indirects - 1);
      if (!get_block(double_indirect_index, &double_indirect_block))
        goto error_occured;
      // update double indirect block
      ptr = (uint32_t*)double_indirect_block->get();
      *(ptr + inner_index) = block_index;
      block_cache_->modify(double_indirect_index);
    }
  } else {
    if (num_blocks == MAX_DIR_BLOCKS + MAX_IND_BLOCKS + MAX_DIND_BLOCKS) {
      if (!alloc_block(&indirect_block, &indirect_block_index))
        goto error_occured;
      inode->i_block[EXT2_TIND_BLOCK] = indirect_block_index;
    } else if (!get_block(inode->i_block[EXT2_TIND_BLOCK], &indirect_block))
      goto error_occured;

    uint32_t num_indirects =
        (num_blocks - MAX_DIR_BLOCKS - MAX_IND_BLOCKS - 1) / MAX_DIND_BLOCKS;
    ASSERT(num_indirects <= NUM_INDIRECT_BLOCKS);
    uint32_t inner_index =
        (num_blocks - MAX_DIR_BLOCKS - MAX_IND_BLOCKS - MAX_DIND_BLOCKS) %
        MAX_DIND_BLOCKS;

    uint32_t double_indirect_index, triple_indirect_index;
    Block *double_indirect_block, *triple_indirect_block;

    if (inner_index == 0) {  // double indirect blocks has been full
      // allocate new double indirect block
      if (!alloc_block(&double_indirect_block, &double_indirect_index))
        goto error_occured;
      // allocate new triple indirect block
      if (!alloc_block(&triple_indirect_block, &triple_indirect_index))
        goto error_occured;

      // update indirect block
      uint32_t* ptr = (uint32_t*)indirect_block->get();
      *(ptr + num_indirects) = double_indirect_index;
      block_cache_->modify(inode->i_block[EXT2_DIND_BLOCK]);

      // update new double indirect block
      ptr = (uint32_t*)double_indirect_block->get();
      *ptr = triple_indirect_index;
      block_cache_->modify(double_indirect_index);

      // update new triple indirect block
      ptr = (uint32_t*)triple_indirect_block->get();
      *ptr = block_index;
      block_cache_->modify(triple_indirect_index);

    } else {
      uint32_t* ptr = (uint32_t*)indirect_block->get();
      double_indirect_index = *(ptr + num_indirects - 1);
      if (!get_block(double_indirect_index, &double_indirect_block))
        goto error_occured;
      uint32_t num_double_indirects =
          (inner_index + MAX_IND_BLOCKS - 1) / MAX_IND_BLOCKS;
      uint32_t double_inner_index = inner_index % MAX_IND_BLOCKS;
      if (double_inner_index == 0) {  // triple indirect blocks has been full
        // allocate new triple indirect block
        if (!alloc_block(&triple_indirect_block, &triple_indirect_index))
          goto error_occured;

        // update double indirect block
        ptr = (uint32_t*)double_indirect_block->get();
        *(ptr + num_double_indirects) = triple_indirect_index;
        block_cache_->modify(double_indirect_index);

        // update new triple indirect block
        ptr = (uint32_t*)triple_indirect_block->get();
        *ptr = block_index;
        block_cache_->modify(triple_indirect_index);
      } else {
        ptr = (uint32_t*)double_indirect_block->get();
        triple_indirect_index = *(ptr + num_double_indirects - 1);
        if (!get_block(triple_indirect_index, &triple_indirect_block))
          goto error_occured;
        // update triple indirect block
        ptr = (uint32_t*)triple_indirect_block->get();
        *(ptr + double_inner_index) = block_index;
        block_cache_->modify(triple_indirect_index);
      }
    }
  }
  // update inode
  inode->i_blocks += 2 << super_block_->get_super()->s_log_block_size;
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
  desc->bg_inode_bitmap = *index * MAX_BLOCK_GROUP_SIZE + BLOCK_SIZE;
  desc->bg_block_bitmap = desc->bg_inode_bitmap + BLOCK_SIZE;
  desc->bg_inode_table = desc->bg_block_bitmap + BLOCK_SIZE;
  desc->bg_free_blocks_count = BLOCKS_PER_GROUP;
  desc->bg_free_inodes_count = INODES_PER_GROUP;
  desc->bg_used_dirs_count = 0;
  super_block_->put_group_desc(desc);
  block_groups_[*index] = new BlockGroup(desc, true);
  DEBUG("Allocate new block group: %u", *index);
  return true;
}

bool FileSystem::free_inode(uint32_t index) {
  // update super block
  super_block_->get_super()->s_free_inodes_count++;
  super_block_->get_super()->s_inodes_count--;

  uint32_t block_group_index = index / super_block_->inodes_per_group();
  uint32_t inner_index = index % super_block_->inodes_per_group();
  auto iter = block_groups_.find(block_group_index);
  if (iter == block_groups_.end()) {
    iter = block_groups_
               .insert({block_group_index,
                        new BlockGroup(
                            super_block_->get_group_desc(block_group_index))})
               .first;
  }
  DEBUG("Free END");
  if (!iter->second->free_inode(inner_index)) {
    WARNING("Attempting to free nonexistent inode!");
    return false;
  }
  return true;
}

bool FileSystem::free_block(uint32_t index) {
  // update super block
  super_block_->get_super()->s_free_blocks_count++;
  super_block_->get_super()->s_blocks_count--;

  uint32_t block_group_index = index / super_block_->blocks_per_group();
  uint32_t inner_index = index % super_block_->blocks_per_group();
  auto iter = block_groups_.find(block_group_index);
  if (iter == block_groups_.end()) {
    iter = block_groups_
               .insert({block_group_index,
                        new BlockGroup(
                            super_block_->get_group_desc(block_group_index))})
               .first;
  }
  if (!iter->second->free_block(inner_index)) {
    WARNING("Attempting to free nonexistent block!");
    return false;
  }
  // free block in block cache
  block_cache_->remove(index);
  return true;
}

}  // namespace naivefs
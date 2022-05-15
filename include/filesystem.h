#ifndef NAIVEFS_INCLUDE_FILESYSTEM_H_
#define NAIVEFS_INCLUDE_FILESYSTEM_H_

#include <sys/time.h>

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "block.h"
#include "cache.h"
#include "utils/path.h"

namespace naivefs {

class FileSystem {
 public:
  FileSystem();

  ~FileSystem();

  inline ext2_super_block* super() { return super_block_->get_super(); }

  void flush();

  /**
   * Create a new inode
   */
  RetCode inode_create(const Path& path, ext2_inode** inode,
                       uint32_t* inode_index_result, bool dir);

  RetCode inode_create(const Path& path, ext2_inode** inode, bool dir) {
    uint32_t _;
    return inode_create(path, inode, &_, dir);
  }

  /**
   * Lookup inode by given path
   */
  RetCode inode_lookup(const Path& path, ext2_inode** inode,
                       uint32_t* inode_index = nullptr,
                       DentryCache::Node** cache_ptr = nullptr);

  /**
   * @brief Delete an existing inode. We need to release the data held by dentry
   * cache. And we set dentry.name_len to 0 to delete the inode in the dentry
   * block of parent inode.
   */
  RetCode inode_delete(const Path& path, uint32_t* inode_index = nullptr);

  /**
   * @brief Delete an existing inode by recursion
   */
  RetCode inode_delete(uint32_t inode_index);

  /**
   * @brief Visit inode blocks
   *
   * @param visitor visiting loop will be terminated by return value of
   * visitor
   */
  void visit_inode_blocks(ext2_inode* inode, const BlockVisitor& visitor);

  /**
   * @brief Visit indirect blocks
   *
   * @param block basic indirect block
   * @param num the number of blocks
   * @param visitor visiting loop will be terminated by return value of visitor
   */
  bool visit_indirect_blocks(Block* block, uint32_t num,
                             const BlockVisitor& visitor);

  /**
   * @brief Get the inode from target block group
   *
   * @return true if inode exists
   */
  bool get_inode(uint32_t index, ext2_inode** inode);

  /**
   * @brief Get the block from target block group
   *
   * @return true if block exists
   */
  bool get_block(uint32_t index, Block** block);

  /**
   * @brief Allocate a new inode in the file system. This operation changes: 1.
   * super block; 2. group descriptors (maybe a new block group); 3. inode
   * bitmap; 4. inode table (maybe a new inode table block)
   *
   * @param inode new inode allocated
   * @param index returns inode index
   * @param dir if the inode to allocate is a directory inode
   * @return @return true always true (we assume disk space will not ne used up)
   */
  bool alloc_inode(ext2_inode** inode, uint32_t* index, bool dir);

  /**
   * @brief Allocate a new block in the file system. This operation changes: 1.
   * super block; 2. group descriptors (maybe a new block group); 3. block
   * bitmap
   *
   * @return always true (we assume disk space will not be used up)
   */
  bool alloc_block(Block** block, uint32_t* index);

  /**
   * @brief Allocate a new block for the inode
   *
   * @return always true (we assume disk space will not be used up)
   */
  bool alloc_block(Block** block, uint32_t* index, ext2_inode* inode);

  /**
   * @brief Allocatea a new block group
   *
   * @param index newly allocated index
   * @return always true (we assume disk space will not be used up)
   */
  bool alloc_block_group(uint32_t* index);

  /**
   * @brief Free inode with inode index
   *
   */
  bool free_inode(uint32_t index);

  /**
   * @brief Free block with block index
   */
  bool free_block(uint32_t index);

 private:
  // Timestamp
  timeval time_;
  // Super block
  SuperBlock* super_block_;
  // Root inode
  ext2_inode* root_inode_;
  // Block Groups
  std::map<uint32_t, BlockGroup*> block_groups_;
  // block index mapped to block allocated in memory
  BlockCache* block_cache_;
  // name mapped to directory entry metadata
  DentryCache* dentry_cache_;
};
}  // namespace naivefs
#endif
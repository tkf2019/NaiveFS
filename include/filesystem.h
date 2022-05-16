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
                       uint32_t* inode_index_result, mode_t mode);

  RetCode inode_create(const Path& path, ext2_inode** inode, mode_t mode) {
    uint32_t _;
    return inode_create(path, inode, &_, mode);
  }

  /**
   * @brief Create a new dentry, appending the dentry data to the last block of
   * parent inode. If the last block is full, we will allocate a new block for
   * the parent.
   */
  RetCode dentry_create(Block* last_block, uint32_t last_block_index,
                        ext2_inode* parent, const char* name, size_t name_len,
                        uint32_t inode_index, mode_t mode);

  /**
   * Lookup inode by given path.
   * If we find a symbolic inode, lookup the target path to finally return the
   * real inode.
   */
  RetCode inode_lookup(const Path& path, ext2_inode** inode,
                       uint32_t* inode_index = nullptr,
                       DentryCache::Node** cache_ptr = nullptr);

  /**
   * @brief  Lookup inode by given parent. Parent should not be nullptr.
   */
  RetCode inode_lookup(ext2_inode* parent, const char* name, size_t name_len,
                       bool* name_exists = nullptr,
                       uint32_t* inode_index = nullptr,
                       Block** last_block = nullptr,
                       uint32_t* last_block_index = nullptr);

  /**
   * @brief Delete an existing inode by recursion
   */
  RetCode inode_delete(uint32_t inode_index);

  /**
   * @brief Unlink an existing inode (delete the inode when i_links equal to 0)
   */
  RetCode inode_unlink(const Path& path);

  /**
   * @brief Create a new dentry by destination path and link the dentry to the
   * inode pointed by source dentry. Cannot create dentry when the destination
   * file already exists or the source file does not exist.
   */
  RetCode inode_link(const Path& src, const Path& dst);

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
   * @return @return true always true (we assume disk space will not ne used up)
   */
  bool alloc_inode(ext2_inode** inode, uint32_t* index, mode_t mode);

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

  /**
   * @brief Write data to block
   */
  inline void write_block(Block* block, uint32_t index, const char* buf,
                          size_t size) {
    memcpy(block->get(), buf, size);
    block_cache_->modify(index);
  }

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
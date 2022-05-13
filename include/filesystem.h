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

  bool inode_create(const Path& path, ext2_inode** inode, bool dir);

  /**
   * @brief Lookup inode by given path
   *
   * @return true if inode exists
   * @return false if inode does not exist or directory in the path has been
   * deleted
   */
  bool inode_lookup(const Path& path, ext2_inode** inode);

  /**
   * @brief Lookup inode dentry by given name
   *
   * @return true if dentry exists
   * @return false
   */
  bool dentry_lookup(ext2_inode* inode, char* name, ext2_dir_entry_2** dentry);

  /**
   * @brief Visit inode blocks
   *
   * @param visitor visiting loop will be terminated by return value of visitor
   */
  void visit_inode_blocks(uint32_t inode_index, const BlockVisitor& visitor);

  /**
   * @brief Visit inode blocks
   *
   * @param visitor visiting loop will be terminated by return value of visitor
   */
  void visit_inode_blocks(ext2_inode* inode, const BlockVisitor& visitor);

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
   * @param index
   * @return always true (we assume disk space will not be used up)
   */
  bool alloc_block_group(uint32_t* index);

  /**
   * Seek to the last block of the inode. Returns NULL if no block has been
   * allocated.
   */
  bool seek_last_block(ext2_inode* inode, Block** block, uint32_t* index);

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
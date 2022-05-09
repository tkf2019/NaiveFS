#ifndef NAIVEFS_INCLUDE_SUPER_H_
#define NAIVEFS_INCLUDE_SUPER_H_

#include "common.h"
#include "disk.h"

namespace naivefs {
struct super_t {
  uint32_t magic;           // magic number
  uint32_t inode_cnt;       // inodes count
  uint32_t block_cnt;       // blocks count
  uint32_t free_inode_cnt;  // the number of free inodes in the file system
  uint32_t free_block_cnt;  // the number of free blocks in the file system
  uint32_t block_size;      // block size
  uint32_t first_inode;     // first inode
  uint32_t block_group;     // the block group number that holds the copy of the
                            // superblock
};
}  // namespace naivefs

#endif
#ifndef NAIVEFS_INCLUDE_COMMON_H_
#define NAIVEFS_INCLUDE_COMMON_H_

#include <stdint.h>

namespace naivefs {
// disk
#define DISK_ALIGN 512
#define DISK_NAME "/tmp/disk"

#define ALIGN_TO(__n, __align)                        \
  ({                                                  \
    typeof(__n) __ret;                                \
    if ((__n) % (__align)) {                          \
      __ret = ((__n) & (~((__align)-1))) + (__align); \
    } else                                            \
      __ret = (__n);                                  \
    __ret;                                            \
  })

// File System Configs
#define BLOCK_SIZE 4096   // 4KB block
#define LOG_BLOCK_SIZE 4  // 4 * 2^10 block

#define INODE_PER_BLOCK BLOCK_SIZE / sizeof(ext2_inode)
#define NUM_INODE_TABLE_BLOCKS BLOCK_SIZE * 8 / INODE_PER_BLOCK

// 1 inode bitmap, 1 block bitmap, 1024 inode table blocks, 4096 * 8 data blocks
#define BLOCKS_PER_GROUP BLOCK_SIZE * 8
#define INODES_PER_GROUP BLOCK_SIZE * 8
#define TOTAL_BLOCKS_PER_GROUP BLOCKS_PER_GROUP + NUM_INODE_TABLE_BLOCKS + 2

#define ROOT_INODE 0
}  // namespace naivefs

#endif
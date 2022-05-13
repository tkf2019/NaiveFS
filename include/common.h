#ifndef NAIVEFS_INCLUDE_COMMON_H_
#define NAIVEFS_INCLUDE_COMMON_H_

#include <stdint.h>
#include <sys/time.h>

#include <algorithm>

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

// File system state (super_block.s_state)
enum FSState { UNINIT = 0x0, NORMAL = 0x1 };

constexpr static const uint32_t NUM_INDIRECT_BLOCKS =
    BLOCK_SIZE / sizeof(uint32_t);
enum IndirectLevel { SINGLE = 1, DOUBLE = 2, TRIPPLE = 3 };

#define UPDATE_TIME(__val)      \
  ({                            \
    timeval time;               \
    gettimeofdata(&time, NULL); \
    __val = time.tv_sec;        \
  })

#define ACCESS_INODE(__i) UPDATE_TIME(__i->i_atime)
#define MODIFY_INODE(__i) UPDATE_TIME(__i->i_mtime)

#define BLOCK_CACHE_SIZE 1024  // TODO: maybe larger ?

// dentry types

#define DENTRY_DIR 0x4
#define DENTRY_REG 0x8

#define DENTRY_ISDIR(__t) (__t == DENTRY_DIR)
#define DENTRY_ISREG(__t) (__t == DENTRY_REG)

}  // namespace naivefs

#endif
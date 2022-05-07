#ifndef NAIVEFS_INCLUDE_INODE_H_
#define NAIVEFS_INCLUDE_INODE_H_

#include "common.h"

namespace naivefs {
struct inode_t {
  uint16_t mode;             // rwx mode
  uint16_t uid;              // owner id
  uint32_t size;             // file size in bytes
  uint32_t atime;            // time last accessed
  uint32_t ctime;            // time created
  uint32_t mtime;            // time last modified
  uint32_t dtime;            // time deleted
  uint16_t gid;              // group id
  uint16_t nlinks;           // number of hard links to this file
  uint32_t nblocks;          // number of blocks allocated to this file
  uint32_t flags;            // how should ext2 use this inode?
  uint8_t osd1[4];           // OS dependent field
  uint32_t block[N_BLOCKS];  // disk pointers
  uint32_t generation;       // file version (used by NFS)
  uint32_t file_acl;         // a new permissions model beyond mode bits
  uint32_t dir_acl;          // called access control lists
  uint32_t faddr;            // an unsupported field
  uint8_t osd2[12];          // another OS dependent field
};
}  // namespace naivefs

#endif
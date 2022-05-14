#include <vector>

#include "operation.h"

namespace naivefs {

FileSystem* fs;
OpManager* opm;

void test_filesystem() {
  ext2_inode* home_inode;
  ASSERT(fs->inode_create("/home", &home_inode, true));
  ext2_inode* test_inode;
  ASSERT(fs->inode_create("/home/test.txt", &test_inode, false));
  ext2_inode* test2_inode;
  ASSERT(fs->inode_create("/home/test2.txt", &test2_inode, false));
  ext2_inode* home2_inode;
  ASSERT(fs->inode_create("/home/tmp", &home2_inode, true));
  ext2_inode* test3_inode;
  ASSERT(fs->inode_create("/home/tmp/test.txt", &test3_inode, false));
}

void* fuse_init(struct fuse_conn_info* info, fuse_config* config) {
  INFO("Using FUSE protocol %d.%d", info->proto_major, info->proto_minor);
  (void)config;
  disk_open();
  fs = new FileSystem();
  opm = new OpManager();

  test_filesystem();

  return NULL;
}
}  // namespace naivefs
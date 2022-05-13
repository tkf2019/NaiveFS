#include "operation.h"

namespace naivefs {
// options global_options;

int fuse_open(const char *path, struct fuse_file_info *fi) {
  INFO("OPEN: %s", path);

  ext2_inode* inode;
  uint32_t inode_id;
  if(!fs->inode_lookup(path, &inode, &inode_id)) return -EINVAL;

  auto fd = new FileStatus;
  fd->cache_update_flag_ = false;
  fd->inode_cache_ = opm->get_cache(inode_id);
  opm->upd_cache(fd, inode_id);
  fd->init_seek();

  fi->fh = reinterpret_cast<decltype(fi->fh)>(fd);

  return 0;
}
}
#include "operation.h"

namespace naivefs {
// options global_options;

int fuse_open(const char *path, struct fuse_file_info *fi) {
  static std::mutex m_;
  std::unique_lock<std::mutex> lck(m_);
  INFO("OPEN: %s", path);

  ext2_inode* inode;
  uint32_t inode_id;
  if(!fs->inode_lookup(path, &inode, &inode_id)) {
    if(!(fi->flags & O_CREAT)) return -EINVAL; 
    if(!fs->inode_create(path, &inode, false)) return -EINVAL;
  }

  auto fd = new FileStatus;
  fd->cache_update_flag_ = false;
  fd->inode_cache_ = opm->get_cache(inode_id);
  opm->upd_cache(fd, inode_id);
  fd->init_seek();

  fi->fh = reinterpret_cast<decltype(fi->fh)>(fd);

  return 0;
}
}
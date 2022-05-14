#include "operation.h"

namespace naivefs {
int fuse_create(const char* path, mode_t mode, struct fuse_file_info* fi) {
  INFO("CREATE: %s, mode %d", path, mode);
  // if O_CREAT is specified without mode specified, mode will be something in the stack.

  ext2_inode* inode;
  uint32_t inode_id;
  if (fs->inode_lookup(path, &inode, &inode_id)) return -EEXIST;
  if (!fs->inode_create(path, &inode, &inode_id, false)) return -EIO;
  INFO("Begin create: %s, inode_id: %d", path, inode_id);
  auto fd = new FileStatus;
  fd->cache_update_flag_ = false;
  auto ic = opm->get_cache(inode_id);
  if (!ic) return -EIO;
  fd->inode_cache_ = ic;
  opm->upd_cache(fd, inode_id);
  fd->init_seek();

  uint32_t nw_time = time(0);

  ic->lock();
  inode = ic->cache_;
  inode->i_mode = mode;
  inode->i_size = 0;
  inode->i_atime = nw_time;
  inode->i_ctime = nw_time;
  inode->i_mtime = nw_time;
  inode->i_flags = 0;  // now we don't care this
  inode->i_gid = 0;
  inode->i_uid = 0;
  ic->unlock();

  fi->fh = reinterpret_cast<decltype(fi->fh)>(fd);

  return 0;
}
int fuse_open(const char *path, struct fuse_file_info *fi) {
  static std::mutex m_;
  std::unique_lock<std::mutex> lck(m_);
  INFO("OPEN: %s", path);

  ext2_inode* inode;
  uint32_t inode_id;
  if(!fs->inode_lookup(path, &inode, &inode_id)) {
    return -ENOENT;
  }

  auto fd = new FileStatus;
  fd->cache_update_flag_ = false;
  fd->inode_cache_ = opm->get_cache(inode_id);
  opm->upd_cache(fd, inode_id);
  fd->init_seek();

  fi->fh = reinterpret_cast<decltype(fi->fh)>(fd);

  return 0;
}

int fuse_rename(const char * oldname, const char * newname, unsigned int flags) {
  return 0;
}

int fuse_truncate(const char * path, off_t offset, struct fuse_file_info* fi) {
  return 0;
}

}  // namespace naivefs
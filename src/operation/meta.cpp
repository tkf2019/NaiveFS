#include "operation.h"

namespace naivefs {
int fuse_create(const char* path, mode_t mode, struct fuse_file_info* fi) {
  INFO("CREATE %s, mode %d", path, mode);
  // if O_CREAT is specified without mode specified, mode will be something in
  // the stack.

  ext2_inode* inode;
  uint32_t inode_id;
  RetCode ret = fs->inode_create(path, &inode, &inode_id, mode);
  if (ret) return Code2Errno(ret);

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
int fuse_open(const char* path, struct fuse_file_info* fi) {
  static std::mutex m_;
  std::unique_lock<std::mutex> lck(m_);
  INFO("OPEN %s", path);

  ext2_inode* inode;
  uint32_t inode_id;
  RetCode ret = fs->inode_lookup(path, &inode, &inode_id);
  if (ret) return Code2Errno(ret);

  auto fd = new FileStatus;
  fd->cache_update_flag_ = false;
  fd->inode_cache_ = opm->get_cache(inode_id);
  opm->upd_cache(fd, inode_id);
  fd->init_seek();

  fi->fh = reinterpret_cast<decltype(fi->fh)>(fd);

  return 0;
}

int fuse_rename(const char* oldname, const char* newname, unsigned int flags) {
  return 0;
}

int fuse_truncate(const char* path, off_t offset, struct fuse_file_info* fi) {
  return 0;
}

int fuse_link(const char* src, const char* dst) {
  INFO("LINK %s,%s", src, dst);

  RetCode link_ret = fs->inode_link(src, dst);
  if (link_ret) return Code2Errno(link_ret);

  return 0;
}

int fuse_unlink(const char* path) {
  INFO("UNLINK %s", path);

  ext2_inode* inode;
  uint32_t inode_id;
  RetCode ret = fs->inode_lookup(path, &inode, &inode_id);
  if (ret) return Code2Errno(ret);

  auto ic = opm->get_cache(inode_id);
  ic->lock();
  if(ic->cnts_ >= 2) {
    ic->unlock();
    opm->rel_cache(inode_id);
    return -EACCES;
  }
  RetCode ret = fs->inode_delete(path);
  ic->unlock();
  if (ret) return Code2Errno(ret);
  return 0;
}

int fuse_access(const char* path, int) {
  INFO("ACCESS %s", path);

  return 0;
}

int fuse_utimens(const char* path, const struct timespec tv[2],
                 struct fuse_file_info* fi) {
  INFO("UTIMENS %s", path);

  return 0;
}

int fuse_release(const char * path, struct fuse_file_info * fi) {
  (void) path;
  if (fs == nullptr || fi == nullptr) return -EINVAL;
  auto fd = _fuse_trans_info(fi);

  // File handle is not valid.
  if (!fd) return -EBADF;
  if ((fi->flags & O_ACCMODE) == O_RDONLY) return -EACCES;

  auto id = fd->inode_cache_->inode_id_;
  delete fd;
  opm->rel_cache(id);
  return 0;
}

int fuse_fsync(const char * path, int datasync, struct fuse_file_info * fi) {
  if (fs == nullptr || fi == nullptr || !path) return -EINVAL;
  auto fd = _fuse_trans_info(fi);
  if (!fd) return -EBADF;

  if(datasync) {
    fs->flush();
  } else {
    fd->inode_cache_->lock_shared();
    fd->inode_cache_->commit();
    fd->inode_cache_->unlock_shared();
    fs->flush();
  }
  return 0;
}



}  // namespace naivefs
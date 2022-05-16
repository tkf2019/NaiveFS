#include "operation.h"

// from https://elixir.bootlin.com/linux/v4.9.33/source/include/uapi/linux/fs.h#L41
#define RENAME_NOREPLACE (1 << 0) /* Don't overwrite target */
#define RENAME_EXCHANGE (1 << 1)  /* Exchange source and dest */
#define RENAME_WHITEOUT (1 << 2)  /* Whiteout source */

namespace naivefs {
extern std::shared_mutex _big_lock;
int fuse_create(const char* path, mode_t mode, struct fuse_file_info* fi) {
  std::unique_lock<std::shared_mutex> __lck(_big_lock);
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
  auto current_user = fuse_get_context();

  ic->lock();
  inode = ic->cache_;
  inode->i_mode = mode;
  inode->i_size = 0;
  inode->i_atime = nw_time;
  inode->i_ctime = nw_time;
  inode->i_mtime = nw_time;
  inode->i_flags = 0;  // now we don't care this
  inode->i_gid = current_user->gid;
  inode->i_uid = current_user->uid;
  ic->unlock();

  fi->fh = reinterpret_cast<decltype(fi->fh)>(fd);

  return 0;
}
int fuse_open(const char* path, struct fuse_file_info* fi) {
  std::unique_lock<std::shared_mutex> __lck(_big_lock);
  INFO("OPEN %s", path);

  ext2_inode* inode;
  uint32_t inode_id;
  RetCode ret = fs->inode_lookup(path, &inode, &inode_id);
  if (ret) return Code2Errno(ret);
  if (!_check_permission(inode->i_mode, (fi->flags & (O_RDONLY | O_RDWR)), (fi->flags & (O_WRONLY | O_RDWR)), 0, inode->i_gid, inode->i_uid))
    return -EACCES;

  auto fd = new FileStatus;
  fd->cache_update_flag_ = false;
  auto ic = fd->inode_cache_ = opm->get_cache(inode_id);
  opm->upd_cache(fd, inode_id);
  fd->init_seek();

  uint32_t nw_time = time(0);

  ic->lock();
  inode = ic->cache_;
  inode->i_atime = nw_time;
  inode->i_ctime = nw_time;
  if ((fi->flags & O_ACCMODE) != O_RDONLY) inode->i_mtime = nw_time;
  ic->unlock();

  fi->fh = reinterpret_cast<decltype(fi->fh)>(fd);

  return 0;
}

int fuse_rename(const char* oldname, const char* newname, unsigned int flags) {
  std::unique_lock<std::shared_mutex> __lck(_big_lock);
  INFO("RENAME %s, %s", oldname, newname);

  ext2_inode *_old, *_new;
  if (flags == RENAME_EXCHANGE) {
    // atomicitily exchange two existing files
    if (fs->inode_lookup(newname, &_old)) return -ENOENT;
    if (!_check_permission(_old->i_mode, 0, 1, 0, _old->i_gid, _old->i_uid)) return -EACCES;
    if (fs->inode_lookup(oldname, &_new)) return -ENOENT;
    if (!_check_permission(_new->i_mode, 0, 1, 0, _new->i_gid, _new->i_uid)) return -EACCES;
    RetCode ret = fs->inode_link(oldname, "/<a");
    if (ret) return Code2Errno(ret);
    ret = fs->inode_unlink(oldname);
    if (ret) return Code2Errno(ret);
    ret = fs->inode_link(newname, oldname);
    if (ret) return Code2Errno(ret);
    ret = fs->inode_link(oldname, newname);
    if (ret) return Code2Errno(ret);
    ret = fs->inode_unlink("/<a");
    if (ret) return Code2Errno(ret);
  } else if (flags == RENAME_NOREPLACE) {
    // don't replace an existing file
    ext2_inode* _;
    if (!fs->inode_lookup(newname, &_old)) return -EEXIST;
    if (fs->inode_lookup(oldname, &_new)) return -ENOENT;
    if (!_check_permission(_new->i_mode, 0, 1, 0, _new->i_gid, _new->i_uid)) return -EACCES;
    RetCode ret = fs->inode_link(oldname, newname);
    if (ret) return Code2Errno(ret);
    ret = fs->inode_unlink(oldname);
    if (ret) return Code2Errno(ret);
  } else if (flags == 0) {
    // replace existing file

    ext2_inode* _;
    if (fs->inode_lookup(oldname, &_old)) return -ENOENT;
    if (!_check_permission(_old->i_mode, 0, 1, 0, _old->i_gid, _old->i_uid)) return -EACCES;
    if (!fs->inode_lookup(newname, &_new)) {
      if (!_check_permission(_new->i_mode, 0, 1, 0, _new->i_gid, _new->i_uid)) return -EACCES;
      RetCode ret = fs->inode_unlink(newname);
      if (ret) return Code2Errno(ret);
    }
    RetCode ret = fs->inode_link(oldname, newname);
    if (ret) return Code2Errno(ret);
    ret = fs->inode_unlink(oldname);
    if (ret) return Code2Errno(ret);

  } else {
    WARNING("fuse rename: invalid flags, %d", flags);
    return -EINVAL;
  }
  return 0;
}

int fuse_truncate(const char* path, off_t offset, struct fuse_file_info* fi) {
  std::unique_lock<std::shared_mutex> __lck(_big_lock);
  INFO("TRUNATE %s", path);
  // char buf[1];
  // fuse_write(path, buf, 0, offset, fi);
  return 0;
}

int fuse_link(const char* src, const char* dst) {
  std::unique_lock<std::shared_mutex> __lck(_big_lock);
  INFO("LINK %s,%s", src, dst);

  RetCode ret = fs->inode_link(src, dst);
  if (ret) return Code2Errno(ret);

  return 0;
}

int fuse_unlink(const char* path) {
  std::unique_lock<std::shared_mutex> __lck(_big_lock);
  INFO("UNLINK %s", path);

  ext2_inode* inode;
  uint32_t inode_id;
  RetCode ret = fs->inode_lookup(path, &inode, &inode_id);
  if (ret) return Code2Errno(ret);

  auto ic = opm->get_cache(inode_id);
  ic->lock();
  if (ic->cnts_ >= 2 || !_check_permission(ic->cache_->i_mode, 0, 1, 0, ic->cache_->i_gid, ic->cache_->i_uid)) {
    ic->unlock();
    opm->rel_cache(inode_id);
    return -EACCES;
  }
  ic->unlock();
  opm->rel_cache(inode_id);
  // TODO: remove, open concurrently
  ret = fs->inode_unlink(path);
  if (ret) return Code2Errno(ret);
  return 0;
}

int fuse_access(const char* path, int mode) {
  std::unique_lock<std::shared_mutex> __lck(_big_lock);
  INFO("ACCESS %s", path);
  ext2_inode* inode;
  int32_t inode_id;
  RetCode ret = fs->inode_lookup(path, &inode, &inode_id);
  if(ret) return Code2Errno(ret);
  if(mode == F_OK) return 0;
  return _check_permission(inode->i_mode, mode & R_OK, mode & W_OK, mode & X_OK, inode->i_gid, inode->i_uid);
}

int fuse_utimens(const char* path, const struct timespec tv[2], struct fuse_file_info* fi) {
  std::unique_lock<std::shared_mutex> __lck(_big_lock);
  INFO("UTIMENS %s", path);

  // see i_atime and i_atime_extra in https://ext4.wiki.kernel.org/index.php/Ext4_Disk_Layout
  // since we don't have i_atime_extra, the precision is second.

  // struct timespec {
  //     time_t tv_sec;        /* seconds */
  //     long   tv_nsec;       /* nanoseconds */
  // };

  // For both calls, the new file timestamps are specified in the
  // array times: times[0] specifies the new "last access time"
  // (atime); times[1] specifies the new "last modification time"
  // (mtime).  Each of the elements of times specifies a time as the
  // number of seconds and nanoseconds since the Epoch, 1970-01-01
  // 00:00:00 +0000 (UTC).  This information is conveyed in a
  // structure of the following form

  ext2_inode* inode;
  uint32_t inode_id;
  RetCode ret = fs->inode_lookup(path, &inode, &inode_id);
  if (ret) return Code2Errno(ret);

  // https://www.daemon-systems.org/man/utimens.2.html
  // If times is NULL, the access and modification times are set to the
  //  current time.  The caller must be the owner of the file, have permission
  //  to write the file, or be the super-user.

  //  If times is non-NULL, it is assumed to point to an array of two timeval
  //  structures.  The access time is set to the value of the first element,
  //  and the modification time is set to the value of the second element.  For
  //  file systems that support file birth (creation) times (such as UFS2), the
  //  birth time will be set to the value of the second element if the second
  //  element is older than the currently set birth time.  To set both a birth
  //  time and a modification time, two calls are required; the first to set
  //  the birth time and the second to set the (presumably newer) modification
  //  time.  Ideally a new system call will be added that allows the setting of
  //  all three times at once.  The caller must be the owner of the file or be
  //  the super-user.

  if (!_check_user(inode->i_mode, inode->i_uid, 0, 1, 0)) return -EACCES;

  auto ic = opm->get_cache(inode_id);

  time_t a_time, m_time;
  if(!tv || !tv[0].tv_sec) m_time = a_time = time(0);
  else a_time = tv[0].tv_sec, m_time = tv[1].tv_sec;

  ic->lock();
  inode = ic->cache_;
  inode->i_atime = a_time;
  inode->i_mtime = m_time;
  ic->commit();
  ic->unlock();

  // INFO("time: %llu, %llu", now_time, tv[0].tv_nsec);
  // INFO("time: %d, %d, %d", tv ? tv[0].tv_sec : now_time, tv ? tv[0].tv_nsec : 0, now_time);

  opm->rel_cache(inode_id);

  return 0;
}

int fuse_release(const char* path, struct fuse_file_info* fi) {
  INFO("RELEASE %s", path);
  if (fs == nullptr || fi == nullptr) return -EINVAL;
  auto fd = _fuse_trans_info(fi);

  // File handle is not valid.
  if (!fd) return -EBADF;

  auto id = fd->inode_cache_->inode_id_;
  delete fd;
  opm->rel_cache(id);
  return 0;
}

int fuse_fsync(const char* path, int datasync, struct fuse_file_info* fi) {
  DEBUG("FSYNC %s", path);
  if (fs == nullptr || fi == nullptr || !path) return -EINVAL;
  auto fd = _fuse_trans_info(fi);
  if (!fd) return -EBADF;

  if (datasync) {
    fs->flush();
  } else {
    fd->inode_cache_->lock_shared();
    fd->inode_cache_->commit();
    fd->inode_cache_->unlock_shared();
    fs->flush();
  }
  return 0;
}

int fuse_statfs(const char* path, struct statvfs* stf) {
  INFO("STATFS %s", path);

  return 0;
}

int fuse_flush(const char* path, struct fuse_file_info*) {
  INFO("FLUSH %s", path);
  // ignore, since flush doesn't sync data
  return 0;
}

int fuse_chown(const char* path, uid_t user, gid_t group, struct fuse_file_info* fi) {
  std::unique_lock<std::shared_mutex> __lck(_big_lock);
  INFO("CHOWN %s", path);
  ext2_inode* inode;
  uint32_t inode_id;
  RetCode ret = fs->inode_lookup(path, &inode, &inode_id);
  if (ret) return Code2Errno(ret);

  auto ic = opm->get_cache(inode_id);

  ic->lock();
  inode = ic->cache_;
  inode->i_uid = user;
  inode->i_gid = group;
  ic->commit();
  ic->unlock();
  opm->rel_cache(inode_id);

  return 0;
}

}  // namespace naivefs
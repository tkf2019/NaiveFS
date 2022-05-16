#include "operation.h"

namespace naivefs {

extern std::shared_mutex _big_lock;

int fuse_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
  std::unique_lock<std::shared_mutex> __lck(_big_lock);
  (void)fi;
  INFO("GETATTR: %s", path);
  if (!stbuf) return -EINVAL;
  ext2_inode *inode;
  uint32_t inode_id;

  // we assume inode_lookup is thread-safe
  auto ret = fs->inode_lookup(path, &inode, &inode_id);
  if (ret) return Code2Errno(ret);
  auto ic = opm->get_cache(inode_id);

  // can't get inode
  if (!ic) return -EIO;


  memset(stbuf, 0, sizeof(struct stat));
  ic->lock_shared();
  inode = ic->cache_;
  stbuf->st_dev = 0;                       // ID of device
  stbuf->st_ino = inode_id;                // inode number
  stbuf->st_mode = inode->i_mode;          // protection
  stbuf->st_nlink = inode->i_links_count;  // number of hard links
  stbuf->st_uid = inode->i_uid;            // user ID of owner
  stbuf->st_gid = inode->i_gid;            // group ID of owner
  stbuf->st_rdev = 0;                      // ID of device (special file)
  stbuf->st_size = inode->i_size;          // size in bytes
  stbuf->st_blksize = BLOCK_SIZE;
  stbuf->st_blocks = inode->i_blocks;  // number of 512 bytes
  stbuf->st_atime = inode->i_atime;    // access time
  stbuf->st_mtime = inode->i_mtime;    // modify time
  stbuf->st_ctime = inode->i_ctime;    // change time
  ic->unlock_shared();
  opm->rel_cache(inode_id);
  INFO("GETATTR END");

  return 0;
}

int fuse_chmod(const char *path, mode_t mode, struct fuse_file_info *fi) {
  std::unique_lock<std::shared_mutex> __lck(_big_lock);
  (void)fi;
  INFO("CHMOD: %s", path);
  ext2_inode *inode;
  uint32_t inode_id;

  // we assume inode_lookup is thread-safe
  auto ret = fs->inode_lookup(path, &inode, &inode_id);
  if (ret) return Code2Errno(ret);
  if (!_check_permission(inode->i_mode, 1, 1, 0, inode->i_gid, inode->i_uid)) return -EACCES;
  auto ic = opm->get_cache(inode_id);

  // can't get inode
  if (!ic) return -EIO;

  ic->lock();
  inode = ic->cache_;
  inode->i_mode = mode;
  ic->commit();
  ic->unlock();
  opm->rel_cache(inode_id);
  INFO("CHMOD END");

  return 0;
}

}  // namespace naivefs
#include "operation.h"

namespace naivefs {
// options global_options;

extern std::shared_mutex _big_lock;
int fuse_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                 off_t offset, struct fuse_file_info *fi,
                 enum fuse_readdir_flags flags) {
  std::unique_lock<std::shared_mutex> __lck(_big_lock);
  // TODO: readdir can be thread-safe
  (void)offset;
  (void)fi;
  (void)flags;
  INFO("READDIR: %s", path);

  ext2_inode *parent;
  uint32_t inode_id;
  if (fs->inode_lookup(path, &parent, &inode_id)) return -ENOENT;

  if (!S_ISDIR(parent->i_mode)) return -ENOTDIR;
  if (!_check_permission(parent->i_mode, 1, 0, 0, parent->i_gid, parent->i_uid))
    return -EACCES;

  filler(buf, ".", NULL, 0, FUSE_FILL_DIR_PLUS);
  filler(buf, "..", NULL, 0, FUSE_FILL_DIR_PLUS);
  fs->visit_inode_blocks(parent, [&buf, &filler](__attribute__((unused))
                                                 uint32_t index,
                                                 Block *block) {
    DentryBlock dentry_block(block);
    for (const auto &dentry : *dentry_block.get()) {
      INFO("readdir entry: (%d, inode_id %d) %s", dentry->name_len,
           dentry->inode, std::string(dentry->name, dentry->name_len).c_str());
      if (dentry->name_len)
        filler(buf, std::string(dentry->name, dentry->name_len).c_str(), NULL,
               0, FUSE_FILL_DIR_PLUS);
    }
    return false;
  });

  return 0;
}

int fuse_mkdir(const char *path, mode_t mode) {
  std::unique_lock<std::shared_mutex> __lck(_big_lock);
  INFO("MKDIR: %s", path);
  mode |= S_IFDIR;

  ext2_inode *inode;
  uint32_t id;
  auto ret = fs->inode_create(path, &inode, &id, mode);
  if (ret) return Code2Errno(ret);
  if (!_check_permission(inode->i_mode, 1, 1, 0, inode->i_gid, inode->i_uid))
    return -EACCES;

  uint32_t nw_time = time(0);

  auto ic = opm->get_cache(id);
  if (!ic) return -EIO;
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
  ic->commit();
  ic->unlock();
  opm->rel_cache(id);

  INFO("MKDIR END");

  return 0;
}

int fuse_rmdir(const char *path) {
  std::unique_lock<std::shared_mutex> __lck(_big_lock);
  DEBUG("RMDIR %s", path);
  ext2_inode *parent;
  uint32_t inode_id;
  if (fs->inode_lookup(path, &parent, &inode_id)) return -ENOENT;
  if (!S_ISDIR(parent->i_mode)) return -ENOTDIR;
  if (!_check_permission(parent->i_mode, 1, 1, 0, parent->i_gid, parent->i_uid))
    return -EACCES;
  bool result = true;
  fs->visit_inode_blocks(parent, [&result](uint32_t index, Block *block) {
    DentryBlock dentry_block(block);
    for (const auto &dentry : *dentry_block.get()) {
      if (dentry->name_len) {
        result = false;
        return true;
      }
    }
    return false;
  });
  if (!result) return -ENOTEMPTY;
  auto ret = fs->inode_unlink(path);
  if (ret) {
    return Code2Errno(ret);
  }
  return 0;
}

int fuse_opendir(const char *path, struct fuse_file_info *fi) { return 0; }

}  // namespace naivefs
#include "operation.h"

namespace naivefs {
// options global_options;

int fuse_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                 off_t offset, struct fuse_file_info *fi,
                 enum fuse_readdir_flags flags) {
  (void)offset;
  (void)fi;
  (void)flags;
  INFO("READDIR: %s", path);

  ext2_inode *parent;
  uint32_t inode_id;
  if (fs->inode_lookup(path, &parent, &inode_id)) return -ENOENT;

  if (!S_ISDIR(parent->i_mode)) return -ENOTDIR;

  filler(buf, ".", NULL, 0, FUSE_FILL_DIR_PLUS);
  filler(buf, "..", NULL, 0, FUSE_FILL_DIR_PLUS);
  fs->visit_inode_blocks(parent, [&buf, &filler](__attribute__((unused))
                                                 uint32_t index,
                                                 Block *block) {
    DentryBlock dentry_block(block);
    for (const auto &dentry : *dentry_block.get()) {
      INFO("readdir entry: (%d) %s", dentry->name_len,
           std::string(dentry->name, dentry->name_len).c_str());
      filler(buf, std::string(dentry->name, dentry->name_len).c_str(), NULL, 0,
             FUSE_FILL_DIR_PLUS);
    }
    return false;
  });

  return 0;
}

int fuse_mkdir(const char *path, mode_t mode) {
  INFO("MKDIR: %s", path);
  mode |= S_IFDIR;

  ext2_inode *inode;
  uint32_t id;
  auto ret = fs->inode_create(path, &inode, &id, mode);
  if (ret) return Code2Errno(ret);
  
  uint32_t nw_time = time(0);

  auto ic = opm->get_cache(id);
  if (!ic) return -EIO;
  ic->lock();
  inode->i_mode = mode;
  inode->i_size = 0;
  inode->i_atime = nw_time;
  inode->i_ctime = nw_time;
  inode->i_mtime = nw_time;
  inode->i_flags = 0;  // now we don't care this
  inode->i_gid = 0;
  inode->i_uid = 0;
  ic->unlock();
  opm->rel_cache(id);

  INFO("MKDIR END");

  return 0;
}

int fuse_rmdir(const char *path) {
  DEBUG("RMDIR %s", path);

  return 0;
}

}  // namespace naivefs
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
  fs->visit_inode_blocks(parent, [&buf, &filler](uint32_t index, Block *block) {
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
  ext2_inode *_;
  if (fs->inode_create(path, &_, mode)) return -EINVAL;
  return 0;
}

int fuse_rmdir(const char *path) {
  DEBUG("RMDIR %s", path);

  return 0;
}

}  // namespace naivefs
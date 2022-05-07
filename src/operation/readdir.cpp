#include "operation.h"

namespace naivefs {
// options global_options;

int readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
            struct fuse_file_info *fi, enum fuse_readdir_flags flags) {
  (void)offset;
  (void)fi;
  (void)flags;

  if (strcmp(path, "/") != 0) return -ENOENT;

  filler(buf, ".", NULL, 0, FUSE_FILL_DIR_PLUS);
  filler(buf, "..", NULL, 0, FUSE_FILL_DIR_PLUS);
  filler(buf, "hello", NULL, 0, FUSE_FILL_DIR_PLUS);

  return 0;
}
}
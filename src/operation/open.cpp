#include "operation.h"

namespace naivefs {
// options global_options;

int open(const char *path, struct fuse_file_info *fi) {
  INFO("OPEN!");
  if (strcmp(path + 1, "hello") != 0) return -ENOENT;

  if ((fi->flags & O_ACCMODE) != O_RDONLY) return -EACCES;

  return 0;
}
}
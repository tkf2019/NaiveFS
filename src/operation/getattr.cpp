#include "operation.h"

namespace naivefs {

int getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
  (void)fi;
  int res = 0;

  memset(stbuf, 0, sizeof(struct stat));
  if (strcmp(path, "/") == 0) {
    stbuf->st_mode = S_IFDIR | 0755;
    stbuf->st_nlink = 2;
  } else if (strcmp(path + 1, "hello") == 0) {
    stbuf->st_mode = S_IFREG | 0444;
    stbuf->st_nlink = 1;
    stbuf->st_size = strlen("Hello World!");
  } else
    res = -ENOENT;

  return res;
}
}  // namespace naivefs
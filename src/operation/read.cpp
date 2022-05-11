#include "operation.h"

namespace naivefs {

// options global_options;

int fuse_read(const char *path, char *buf, size_t size, off_t offset,
         struct fuse_file_info *fi) {
  WARNING("READ!");
  size_t len;
  (void)fi;
  if (strcmp(path + 1, "hello") != 0) return -ENOENT;

  len = strlen("Hello World!");
  if ((size_t)offset < len) {
    if (offset + size > len) size = len - offset;
    memcpy(buf, "Hello World!" + offset, size);
  } else
    size = 0;

  return size;
}
}  // namespace naivefs
#include "operation.h"

namespace naivefs {

int fuse_symlink(const char *f1, const char *f2) {
  INFO("SYMLINK %s,%s", f1, f2);

  return 0;
}

int fuse_readlink(const char *path, char *, size_t) {
  INFO("READLINK %s", path);

  return 0;
}
}  // namespace naivefs
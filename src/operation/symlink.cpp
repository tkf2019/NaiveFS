#include "operation.h"

namespace naivefs {

extern std::shared_mutex _big_lock;
int fuse_symlink(const char *f1, const char *f2) {
  std::unique_lock<std::shared_mutex> __lck(_big_lock);
  INFO("SYMLINK %s,%s", f1, f2);

  return 0;
}

int fuse_readlink(const char *path, char *, size_t) {
  std::unique_lock<std::shared_mutex> __lck(_big_lock);
  INFO("READLINK %s", path);

  return 0;
}
}  // namespace naivefs
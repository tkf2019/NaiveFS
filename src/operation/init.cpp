#include <vector>

#include "operation.h"

namespace naivefs {
void* fuse_init(struct fuse_conn_info* info, fuse_config* config) {
  INFO("INIT!");
  std::vector<int> a{1, 2, 3, 4};
  for (auto i : a) {
    DEBUG("%d ", i);
  }
  (void)info;

  return NULL;
}
}  // namespace naivefs
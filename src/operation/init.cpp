#include <iostream>

#include "operation.h"
namespace naivefs {
void* init(struct fuse_conn_info* info, fuse_config* config) {
  (void)info;
  config->kernel_cache = 1;
  return NULL;
}
}  // namespace naivefs
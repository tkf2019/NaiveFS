#include <vector>

#include "operation.h"

namespace naivefs {

FileSystem* fs;

void* fuse_init(struct fuse_conn_info* info, fuse_config* config) {
  INFO("Using FUSE protocol %d.%d", info->proto_major, info->proto_minor);
  disk_open();
  fs = new FileSystem();
  return NULL;
}
}  // namespace naivefs
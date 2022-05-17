#include <vector>

#include "operation.h"

namespace naivefs {

FileSystem* fs;
OpManager* opm;
std::shared_mutex _big_lock;

void* fuse_init(struct fuse_conn_info* info, fuse_config* config) {
  INFO("INIT");
  INFO("Using FUSE protocol %d.%d", info->proto_major, info->proto_minor);
  (void)config;

  disk_open();
  fs = new FileSystem();
  opm = new OpManager();

  // enable writeback cache
  // info->want |= FUSE_CAP_WRITEBACK_CACHE;

  return NULL;
}

void fuse_destroy(void* private_data) {
  INFO("DESTROY")

  delete opm;
  delete fs;
  disk_close();

  return;
}

}  // namespace naivefs
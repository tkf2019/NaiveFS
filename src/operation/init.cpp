#include <vector>

#include "operation.h"

namespace naivefs {

FileSystem* fs;
OpManager* opm;



void* fuse_init(struct fuse_conn_info* info, fuse_config* config) {
  INFO("Using FUSE protocol %d.%d", info->proto_major, info->proto_minor);
  (void)config;
  disk_open();
  fs = new FileSystem();
  opm = new OpManager();

  // enable writeback cache 
  // info->want |= FUSE_CAP_WRITEBACK_CACHE;

  return NULL;
}
}  // namespace naivefs
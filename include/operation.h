#ifndef NAIVEFS_INCLUDE_FUSEOP_H_
#define NAIVEFS_INCLUDE_FUSEOP_H_

#define FUSE_USE_VERSION 31
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "inode.h"
#include "utils/logging.h"
#include "utils/option.h"

namespace naivefs {
int getattr(const char* path, struct stat* stbuf, struct fuse_file_info* fi);

int readlink(const char* path, char* buf, size_t size);

int mkdir(const char* path, mode_t mode);

int readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset,
            struct fuse_file_info* fi, enum fuse_readdir_flags flags);

void* init(fuse_conn_info* info, fuse_config* config);

int read(const char* path, char* buf, size_t size, off_t offset,
         fuse_file_info* fi);

int open(const char* path, fuse_file_info* fi);

}  // namespace naivefs

#endif
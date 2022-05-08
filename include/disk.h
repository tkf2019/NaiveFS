#ifndef NAIVEFS_INCLUDE_DISK_H_
#define NAIVEFS_INCLUDE_DISK_H_

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "common.h"
#include "utils/logging.h"

namespace naivefs {

void* alloc_aligned(size_t size);
int disk_open();
int disk_close();

#define disk_read(__where, __s, __p) \
  __disk_read(__where, __s, __p, __func__, __LINE__)
#define disk_read_type(__where, __t)                                     \
  ({                                                                     \
    __t ret;                                                             \
    ASSERT(__disk_read(__where, sizeof(__t), &ret, __func__, __LINE__)); \
    ret;                                                                 \
  })
#define disk_write(__where, __s, __p) \
  __disk_write(__where, __s, __p, __func__, __LINE__)

int __disk_read(off_t where, size_t size, void* buf, const char* func,
                int line);
int __disk_write(off_t where, size_t size, void* buf, const char* func,
                 int line);
}  // namespace naivefs
#endif
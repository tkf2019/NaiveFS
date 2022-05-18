#include "utils/disk.h"

#include <sys/types.h>

namespace naivefs {

static int disk_fd = -1;

void* alloc_aligned(size_t size) {
  void* buf = nullptr;
  int ret = posix_memalign(&buf, DISK_ALIGN, size);
  if (ret != 0) {
    ERR("Failed to alloc aligned buffer");
    return nullptr;
  }
  // since memset has been done by callers
  // memset(buf, 0, size);
  return buf;
}

int disk_open() {
  disk_fd = open(DISK_NAME, O_DIRECT | O_NOATIME | O_RDWR);
  if (disk_fd < 0) {
    ERR("Failed to open %s: %s", DISK_NAME, strerror(errno));
    return errno;
  }
  return 0;
}

int disk_close() {
  int ret = close(disk_fd);
  if (ret < 0) {
    ERR("Failed to close %s: %s", DISK_NAME, strerror(errno));
    return errno;
  }
  return 0;
}

// TODO: multi-thread
int __disk_write(off_t where, size_t size, void* buf, const char* func,
                 int line) {
  DEBUG("Disk Write: 0x%jx +0x%zx [%s:%d]", where, size, func, line);
  // Fist seek to the disk (seek path in real disk)
  int ret = lseek(disk_fd, where, SEEK_SET);
  if (ret < 0) {
    ERR("Failed to seek to 0x%jx: %s", where, strerror(errno));
    return -errno;
  }
  ret = write(disk_fd, buf, size);
  if (ret < 0) {
    ERR("Failed to write %s: %s", DISK_NAME, strerror(errno));
    return -errno;
  }
  return 0;
}

// TODO: multi-thread
int __disk_read(off_t where, size_t size, void* buf, const char* func,
                int line) {
  DEBUG("Disk Read: 0x%jx +0x%zx [%s:%d]", where, size, func, line);
  int ret = pread(disk_fd, buf, size, where);
  if (ret < 0) {
    ERR("Failed to read %s: %s", DISK_NAME, strerror(errno));
    return -errno;
  }
  ASSERT((size_t)ret == size);
  return 0;
}

}  // namespace naivefs
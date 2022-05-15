#include "operation.h"

namespace naivefs {

// options global_options;
// it returns the number of bytes it read if success.
int fuse_read(const char *path, char *buf, size_t size, off_t offset,
              struct fuse_file_info *fi) {
  INFO("READ %s", path);
  // TODO: locking, poll events
  if (fs == nullptr || fi == nullptr) return -EINVAL;
  auto fd = _fuse_trans_info(fi);

  // File handle is not valid.
  if (!fd) return -EINVAL;
  if ((fi->flags & O_ACCMODE) == O_WRONLY) return -EINVAL;

  return fd->copy_to_buf(buf, offset, size);
}

int fuse_write(const char *path, const char *buf, size_t size, off_t offset,
               struct fuse_file_info *fi) {
  INFO("WRITE %s", path);

  if (fs == nullptr || fi == nullptr) return -EINVAL;
  auto fd = _fuse_trans_info(fi);

  // File handle is not valid.
  if (!fd) return -EINVAL;
  if ((fi->flags & O_ACCMODE) == O_RDONLY) return -EINVAL;
  if (!size) return 0;

  return (fi->flags & O_APPEND) ? fd->append(buf, offset, size)
                                : fd->write(buf, offset, size);
}

}  // namespace naivefs
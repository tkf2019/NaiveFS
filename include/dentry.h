#ifndef NAIVEFS_INCLUDE_DENTRY_H_
#define NAIVEFS_INCLUDE_DENTRY_H_

#include "common.h"

namespace naivefs {
struct dentry_t {
  uint32_t inode;              // inode number
  uint16_t rec_len;            // directory entry length
  uint8_t name_len;            // filename length
  uint8_t file_type;           // file type
  uint8_t name[MAX_NAME_LEN];  // filename
};
}  // namespace naivefs

#endif
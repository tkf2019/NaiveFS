#ifndef NAIVEFS_INCLUDE_COMMON_H_
#define NAIVEFS_INCLUDE_COMMON_H_

#include <stdint.h>

namespace naivefs {

constexpr static const int N_BLOCKS = 15;
constexpr static const int MAX_NAME_LEN = 255;

// disk
constexpr static const int DISK_ALIGN = 512;
#define DISK_NAME "/tmp/disk"

}  // namespace naivefs

#endif
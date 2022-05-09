#ifndef NAIVEFS_INCLUDE_COMMON_H_
#define NAIVEFS_INCLUDE_COMMON_H_

#include <stdint.h>

namespace naivefs {
// disk
#define DISK_ALIGN 512
#define DISK_NAME "/tmp/disk"

#define ALIGN_TO(__n, __align)                        \
  ({                                                  \
    typeof(__n) __ret;                                \
    if ((__n) % (__align)) {                          \
      __ret = ((__n) & (~((__align)-1))) + (__align); \
    } else                                            \
      __ret = (__n);                                  \
    __ret;                                            \
  })

}  // namespace naivefs

#endif
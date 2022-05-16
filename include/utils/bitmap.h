#ifndef NAIVEFS_INCLUDE_BITMAP_H_
#define NAIVEFS_INCLUDE_BITMAP_H_

#include <math.h>

#include "common.h"
#include "logging.h"

namespace naivefs {

#define BIT_SHIFT 5
#define BIT_MASK 0x1f
#define BIT_MAX (~0u)
#define BIT_GET(__i) (1 << (__i & BIT_MASK))

class Bitmap {
 public:
  Bitmap(void* buf) : data_((uint32_t*)buf) {}

  void set(int i);

  bool test(int i);

  void clear(int i);

  /**
   * @brief find the first unset bit in the bitmap
   *
   * @param size find the bit in range [0, size)
   * @return int -1 if out of range or no unset bit
   */
  int64_t find(int size);

 private:
  uint32_t* data_;
};

}  // namespace naivefs

#endif
#include "utils/bitmap.h"

namespace naivefs::bitmap {

uint32_t* create(void* buf) { return (uint32_t*)buf; }

void set(uint32_t* bitmap, int i) {
  DEBUG("SET %d", i);
  bitmap[i >> BIT_SHIFT] |= BIT_GET(i);
}

int test(uint32_t* bitmap, int i) {
  return bitmap[i >> BIT_SHIFT] & BIT_GET(i);
}

int clear(uint32_t* bitmap, int i) {
  return bitmap[i >> BIT_SHIFT] & ~BIT_GET(i);
}

int find(uint32_t* bitmap, int size) {
  int max_index = (size >> BIT_SHIFT) + ((size & BIT_MASK) != 0);
  for (int i = 0; i < max_index; ++i) {
    if (bitmap[i] != BIT_MAX) {
      int cur_bits = bitmap[i];
      int k = 0;
      while (cur_bits & 1) {
        cur_bits >>= 1;
        k++;
      }
      int ret = (i << BIT_SHIFT) + k;
      return ret >= size ? -1 : ret;
    }
  }
  return -1;
}

}  // namespace naivefs::bitmap

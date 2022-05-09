#include "utils/bitmap.h"

namespace naivefs {

void Bitmap::set(int i) { data_[i >> BIT_SHIFT] |= BIT_GET(i); }

int Bitmap::test(int i) { return data_[i >> BIT_SHIFT] & BIT_GET(i); }

int Bitmap::clear(int i) { return data_[i >> BIT_SHIFT] & ~BIT_GET(i); }

int64_t Bitmap::find(int size) {
  int max_index = (size >> BIT_SHIFT) + ((size & BIT_MASK) != 0);
  for (int i = 0; i < max_index; ++i) {
    if (data_[i] != BIT_MAX) {
      int cur_bits = data_[i];
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

}  // namespace naivefs

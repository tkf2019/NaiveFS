#ifndef NAIVEFS_INCLUDE_BITMAP_H_
#define NAIVEFS_INCLUDE_BITMAP_H_

#include <math.h>

#include "common.h"
#include "logging.h"

namespace naivefs::bitmap {

#define BIT_SHIFT 5
#define BIT_MASK 0x1f
#define BIT_MAX ((1UL << 32) - 1)
#define BIT_GET(__i) (1 << (__i & BIT_MASK))

uint32_t* create(void* buf);

void set(uint32_t* bitmap, int i);

int test(uint32_t* bitmap, int i);

int clear(uint32_t* bitmap, int i);

/**
 * @brief find the first unset bit in the bitmap
 *
 * @param size find the bit in range [0, size)
 * @return int -1 if out of range or no unset bit
 */
int find(uint32_t* bitmap, int size);

}  // namespace naivefs::bitmap

#endif
#ifndef H_COMMON
#define H_COMMON

#include "array.h"
#include "assert.h"
#include "typedefs.h"

arr_define(u32);
slice_define(u32);
arr_define(i16);
slice_define(i16);
arr_define(f32);
slice_define(f32);
arr_define(u8);
slice_define(u8);
arr_define(char);
slice_define(char);

force_inline bool32 is_power_of_two(uintptr x) { return (x & (x - 1)) == 0; }

force_inline uintptr align_forward(uintptr ptr, size_t align) {
  uintptr p, a, modulo;

  assert(is_power_of_two(align));

  p = ptr;
  a = cast(uintptr) align;
  // Same as (p % a) but faster as 'a' is a power of two
  modulo = p & (a - 1);

  if (modulo != 0) {
    // If 'p' address is not aligned, push the address to the
    // next value which is aligned
    p += a - modulo;
  }
  return p;
}

// Number to string conversion functions
size_t i32_to_str(int32 n, char *str);
size_t u32_to_str(uint32 n, char *str);
size_t hex32_to_str(uint32 n, char *str);
size_t f32_to_str(float32 n, char *str);
size_t double_to_str(double n, char *str);

#endif

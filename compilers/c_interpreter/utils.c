#ifndef H_UTILS
#define H_UTILS

#include "macros.h"
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
  int kind;
  char *msg;
} Error;

int parse_int(const char *str, int len, int *out_num) {
  if (str == NULL || out_num == NULL || len <= 0) {
    return -1; // Invalid input
  }

  int num = 0;
  int sign = 1;
  int i = 0;

  // Check for a leading sign
  if (str[0] == '-') {
    sign = -1;
    i++;
  } else if (str[0] == '+') {
    i++;
  }

  for (; i < len; i++) {
    char ch = str[i];

    if (ch < '0' || ch > '9') {
      return -1; // Invalid character, not a number
    }

    // Shift the current number by one decimal place to the left and add the new
    // digit
    num = num * 10 + (ch - '0');
  }

  *out_num = num * sign;
  return 0; // Success
}

size_t calculate_int_string_size(int n) {
  size_t size = 0;

  if (n == 0) {
    return 2;
  }

  if (n < 0) {
    size++;
    if (n == -2147483648) {
      // Return size for "-2147483648" (11 characters plus null terminator)
      return 12;
    }
    n = -n;
  }

  // Count the digits
  while (n != 0) {
    size++;
    n /= 10;
  }

  return size + 1;
}

bool is_power_of_two(uintptr_t x) { return (x & (x - 1)) == 0; }

uintptr_t align_forward(uintptr_t ptr, size_t align) {
  uintptr_t p, a, modulo;

  assert(is_power_of_two(align));

  p = ptr;
  a = (uintptr_t)align;
  // Same as (p % a) but faster as 'a' is a power of two
  modulo = p & (a - 1);

  if (modulo != 0) {
    // If 'p' address is not aligned, push the address to the
    // next value which is aligned
    p += a - modulo;
  }
  return p;
}

#define MEMCPY_ARRAY(type, dest, src, count) memcpy(dest, src, sizeof(type) * count)

#endif

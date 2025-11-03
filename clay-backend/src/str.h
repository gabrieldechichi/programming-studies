#ifndef STRING_H
#define STRING_H

#include "typedefs.h"
#include <stddef.h>

#if defined(__wasm__) && defined(__clang__)

static inline u32 strlen(const char *s) {
  const char *start = s;
  while (*s) {
    s++;
  }
  return (u32)(s - start);
}

// strcmp - lexicographic string comparison
static inline int strcmp(const char *s1, const char *s2) {
  while (*s1 && (*s1 == *s2)) {
    s1++;
    s2++;
  }
  return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

// strncmp - compare at most n characters
static inline int strncmp(const char *s1, const char *s2, size_t n) {
  while (n && *s1 && (*s1 == *s2)) {
    s1++;
    s2++;
    n--;
  }
  if (n == 0)
    return 0;
  return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

// strcpy - copy string (unsafe - no bounds checking)
static inline char *strcpy(char *dest, const char *src) {
  char *d = dest;
  while ((*d++ = *src++))
    ;
  return dest;
}

// strncpy - copy at most n characters
static inline char *strncpy(char *dest, const char *src, size_t n) {
  size_t i;
  for (i = 0; i < n && src[i] != '\0'; i++) {
    dest[i] = src[i];
  }
  for (; i < n; i++) {
    dest[i] = '\0';
  }
  return dest;
}

// strchr - find first occurrence of character
static inline char *strchr(const char *s, int c) {
  while (*s != (char)c) {
    if (!*s++)
      return NULL;
  }
  return (char *)s;
}

// strrchr - find last occurrence of character
static inline char *strrchr(const char *s, int c) {
  const char *last = NULL;
  do {
    if (*s == (char)c) {
      last = s;
    }
  } while (*s++);
  return (char *)last;
}

// i32_to_str - convert signed 32-bit integer to string
// Returns number of characters written (excluding null terminator)
static inline int i32_to_str(int32_t value, char *str) {
  char *start = str;
  uint32_t uvalue;

  // Handle negative numbers
  if (value < 0) {
    *str++ = '-';
    uvalue = (uint32_t)(-value);
  } else {
    uvalue = (uint32_t)value;
  }

  // Handle zero
  if (uvalue == 0) {
    *str++ = '0';
    *str = '\0';
    return (int)(str - start);
  }

  // Convert digits (reversed)
  char *digits_start = str;
  while (uvalue > 0) {
    *str++ = '0' + (uvalue % 10);
    uvalue /= 10;
  }

  // Reverse the digits
  char *left = digits_start;
  char *right = str - 1;
  while (left < right) {
    char tmp = *left;
    *left = *right;
    *right = tmp;
    left++;
    right--;
  }

  *str = '\0';
  return (int)(str - start);
}

// f32_to_str - convert 32-bit float to string with precision
// Returns number of characters written (excluding null terminator)
// precision: number of decimal places (default 6 if negative)
static inline int f32_to_str(float value, char *str, int precision) {
  char *start = str;

  // Default precision
  if (precision < 0)
    precision = 6;

  // Handle special cases
  union {
    float f;
    uint32_t i;
  } u = {value};
  uint32_t exp = (u.i >> 23) & 0xFF;

  // NaN
  if (exp == 0xFF && (u.i & 0x7FFFFF)) {
    *str++ = 'N';
    *str++ = 'a';
    *str++ = 'N';
    *str = '\0';
    return 3;
  }

  // Infinity
  if (exp == 0xFF) {
    if (u.i >> 31)
      *str++ = '-';
    *str++ = 'i';
    *str++ = 'n';
    *str++ = 'f';
    *str = '\0';
    return (int)(str - start);
  }

  // Handle sign
  if (value < 0.0f) {
    *str++ = '-';
    value = -value;
  }

  // Extract integer part
  int32_t int_part = (int32_t)value;
  float frac_part = value - (float)int_part;

  // Convert integer part
  str += i32_to_str(int_part, str);

  // Add decimal point
  if (precision > 0) {
    *str++ = '.';

    // Convert fractional part
    for (int i = 0; i < precision; i++) {
      frac_part *= 10.0f;
      int digit = (int)frac_part;
      *str++ = '0' + digit;
      frac_part -= (float)digit;
    }
  }

  *str = '\0';
  return (int)(str - start);
}

#else
#define strlen(s) __error("strlen not implemented for platform")
#define strcmp(s1, s2) __error("strcmp not implemented for platform")
#define strncmp(s1, s2, n) __error("strncmp not implemented for platform")
#define strcpy(dest, src) __error("strcpy not implemented for platform")
#define strncpy(dest, src, n) __error("strncpy not implemented for platform")
#define strchr(s, c) __error("strchr not implemented for platform")
#define strrchr(s, c) __error("strrchr not implemented for platform")
#define i32_to_str(value, str)                                                 \
  __error("i32_to_str not implemented for platform")
#define f32_to_str(value, str, precision)                                      \
  __error("f32_to_str not implemented for platform")
#endif

static inline u32 str_len(const char *s) { strlen(s); }

static inline b32 char_is_space(char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' ||
         c == '\v';
}

static inline b32 char_is_line_break(char c) { return c == '\n'; }

static inline b32 char_is_digit(char c) { return c >= '0' && c <= '9'; }

static inline double str_to_double(const char *str) {
  if (!str)
    return 0.0;

  double result = 0.0;
  double sign = 1.0;
  const char *ptr = str;

  // Skip whitespace
  while (char_is_space(*ptr))
    ptr++;

  // Handle sign
  if (*ptr == '-') {
    sign = -1.0;
    ptr++;
  } else if (*ptr == '+') {
    ptr++;
  }

  // Parse integer part
  while (char_is_digit(*ptr)) {
    result = result * 10.0 + (*ptr - '0');
    ptr++;
  }

  // Parse fractional part
  if (*ptr == '.') {
    ptr++;
    double fraction = 0.0;
    double divisor = 1.0;

    while (char_is_digit(*ptr)) {
      fraction = fraction * 10.0 + (*ptr - '0');
      divisor *= 10.0;
      ptr++;
    }

    result += fraction / divisor;
  }

  // Parse exponent (basic support for e/E notation)
  if (*ptr == 'e' || *ptr == 'E') {
    ptr++;
    double exp_sign = 1.0;
    double exponent = 0.0;

    if (*ptr == '-') {
      exp_sign = -1.0;
      ptr++;
    } else if (*ptr == '+') {
      ptr++;
    }

    while (char_is_digit(*ptr)) {
      exponent = exponent * 10.0 + (*ptr - '0');
      ptr++;
    }

    // Apply exponent (simple power calculation)
    double multiplier = 1.0;
    for (int i = 0; i < (int)exponent; i++) {
      if (exp_sign > 0) {
        multiplier *= 10.0;
      } else {
        multiplier /= 10.0;
      }
    }
    result *= multiplier;
  }

  return result * sign;
}

#endif // STRING_H

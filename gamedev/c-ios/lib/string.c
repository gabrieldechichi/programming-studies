#include "string.h"
#include <string.h>

String str_from_cstr_alloc(const char *cstr, u32 len, Allocator *allocator) {
  char *s = ALLOC_ARRAY(allocator, char, len + 1);
  memcpy(s, cstr, len);
  s[len] = '\0';
  return (String){.value = s, .len = len};
}

String32Bytes fixedstr32_from_cstr(char *cstr) {
  String32Bytes result;
  size_t len = str_len(cstr);
  if (len >= 32) {
    len = 31;
  }
  str_copy(result.value, cstr, len);
  result.value[len] = 0; // null-terminate
  result.len = len;
  return result;
}

String64Bytes fixedstr64_from_cstr(char *cstr) {
  String64Bytes result;
  size_t len = str_len(cstr);
  if (len >= 64) {
    len = 64;
  }
  str_copy(result.value, cstr, len);
  result.value[len] = 0; // null-terminate
  result.len = len;
  return result;
}

u32 str_len(const char *s) {
  const char *start = s;
  while (*s) {
    s++;
  }
  return (u32)(s - start);
}

u32 str_copy(char *to, char *from, u32 len) {
  u32 i;
  for (i = 0; i < len && from[i] != '\0'; i++) {
    to[i] = from[i];
  }
  to[i] = '\0'; // Null-terminate the destination string
  return i;     // Return the number of characters copied
}

b32 str_equal(const char *a, const char *b) {
  while (*a && *b) {
    if (*a != *b) {
      return 0;
    }
    a++;
    b++;
  }
  return *a == *b;
}

b32 str_equal_len(const char *a, u32 len_a, const char *b, u32 len_b) {
  if (len_a != len_b) {
    return false;
  }

  for (u32 i = 0; i < len_a; i++) {
    if (a[i] != b[i]) {
      return false;
    }
  }

  return true;
}

b32 str_contains(const char *a, const char *b) {
  if (!a || !b) {
    return false;
  }

  for (const char *p = a; *p; ++p) {
    const char *start = p;
    const char *q = b;

    while (*start && *q && *start == *q) {
      ++start;
      ++q;
    }

    if (!*q) {
      return 1;
    }
  }

  return 0;
}

double str_to_double(const char *str) {
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

String str_trim(String str, Allocator *allocator) {
  if (str.len == 0 || !str.value) {
    return (String){0};
  }

  // Find start of non-whitespace
  u32 start = 0;
  while (start < str.len && char_is_space(str.value[start])) {
    start++;
  }

  // If all characters are whitespace
  if (start >= str.len) {
    return (String){0};
  }

  // Find end of non-whitespace (work backwards)
  u32 end = str.len - 1;
  while (end > start && char_is_space(str.value[end])) {
    end--;
  }

  // Calculate trimmed length
  u32 trimmed_len = end - start + 1;

  // Create new trimmed string
  char *trimmed_str = ALLOC_ARRAY(allocator, char, trimmed_len + 1);
  memcpy(trimmed_str, str.value + start, trimmed_len);
  trimmed_str[trimmed_len] = '\0';

  return (String){.value = trimmed_str, .len = trimmed_len};
}

static b32 char_in_trim_list(char c, const char *trim_chars) {
  for (const char *p = trim_chars; *p; p++) {
    if (c == *p) {
      return true;
    }
  }
  return false;
}

String str_trim_chars(String str, const char *trim_chars,
                      Allocator *allocator) {
  if (str.len == 0 || !str.value || !trim_chars) {
    return (String){0};
  }

  // Find start of non-trim characters
  u32 start = 0;
  while (start < str.len && char_in_trim_list(str.value[start], trim_chars)) {
    start++;
  }

  // If all characters are in trim list
  if (start >= str.len) {
    return (String){0};
  }

  // Find end of non-trim characters (work backwards)
  u32 end = str.len - 1;
  while (end > start && char_in_trim_list(str.value[end], trim_chars)) {
    end--;
  }

  // Calculate trimmed length
  u32 trimmed_len = end - start + 1;

  // Create new trimmed string
  char *trimmed_str = ALLOC_ARRAY(allocator, char, trimmed_len + 1);
  memcpy(trimmed_str, str.value + start, trimmed_len);
  trimmed_str[trimmed_len] = '\0';

  return (String){.value = trimmed_str, .len = trimmed_len};
}

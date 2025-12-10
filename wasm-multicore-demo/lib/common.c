#include "common.h"
#include "fmt.h"
#include "lib/string.h"

size_t i64_to_str(i64 n, char *str) {
  b32 is_negative = false;
  i32 i = 0;

  if (n < 0) {
    is_negative = true;
    n = -n;
  }

  do {
    str[i++] = (n % 10) + '0';
    n /= 10;
  } while (n);

  if (is_negative) {
    str[i++] = '-';
  }

  str[i] = '\0';

  for (i64 j = 0; j < i / 2; j++) {
    char temp = str[j];
    str[j] = str[i - j - 1];
    str[i - j - 1] = temp;
  }
  return (size_t)i;
}

size_t u64_to_str(u64 n, char *str) {
  int32 i = 0;

  do {
    str[i++] = (n % 10) + '0';
    n /= 10;
  } while (n);

  str[i] = '\0';

  for (int32 j = 0; j < i / 2; j++) {
    char temp = str[j];
    str[j] = str[i - j - 1];
    str[i - j - 1] = temp;
  }
  return (size_t)i;
}

size_t hex64_to_str(u64 n, char *str) {
  int32 i = 0;

  if (n == 0) {
    str[0] = '0';
    str[1] = '\0';
    return 1;
  }

  while (n > 0) {
    uint32 digit = n % 16;
    if (digit < 10) {
      str[i++] = (char)digit + '0';
    } else {
      str[i++] = (char)(digit - 10) + 'a';
    }
    n /= 16;
  }

  str[i] = '\0';

  for (int32 j = 0; j < i / 2; j++) {
    char temp = str[j];
    str[j] = str[i - j - 1];
    str[i - j - 1] = temp;
  }
  return (size_t)i;
}

size_t f64_to_str(f64 n, char *str) {
  char *ptr = str;
  int32 whole_part = (int32)n;
  f64 fractional_part = n - (f64)whole_part;
  int32 sign = (n < 0) ? -1 : 1;

  if (sign == -1) {
    *ptr++ = '-';
    whole_part = -whole_part;
    fractional_part = -fractional_part;
  }

  // convert whole part to str
  char buffer[12];
  char *buf_ptr = buffer;
  do {
    *buf_ptr++ = '0' + (whole_part % 10);
    whole_part /= 10;
  } while (whole_part > 0);

  while (buf_ptr != buffer) {
    *ptr++ = *--buf_ptr;
  }

  *ptr++ = '.';

  // convert fractional part to str
  for (int i = 0; i < 2; ++i) {
    fractional_part *= 10;
    int32 digit = (int32)fractional_part;
    *ptr++ = '0' + (char)digit;
    fractional_part -= digit;
  }

  *ptr = '\0';

  return (size_t)(ptr - str);
}

static double get_positive_infinity(void) {
  union {
    double d;
    uint64 bits;
  } u;
  u.bits = 0x7FF0000000000000ULL;
  return u.d;
}

static double get_negative_infinity(void) {
  union {
    double d;
    uint64 bits;
  } u;
  u.bits = 0xFFF0000000000000ULL;
  return u.d;
}

size_t double_to_str(double n, char *str) {
  char *ptr = str;

  if (n != n) {
    *ptr++ = 'n';
    *ptr++ = 'a';
    *ptr++ = 'n';
    *ptr = '\0';
    return 3;
  }

  if (n == get_positive_infinity()) {
    *ptr++ = 'i';
    *ptr++ = 'n';
    *ptr++ = 'f';
    *ptr = '\0';
    return 3;
  }

  if (n == get_negative_infinity()) {
    *ptr++ = '-';
    *ptr++ = 'i';
    *ptr++ = 'n';
    *ptr++ = 'f';
    *ptr = '\0';
    return 4;
  }

  // Handle sign
  int32 sign = (n < 0) ? -1 : 1;
  if (sign == -1) {
    *ptr++ = '-';
    n = -n;
  }

  // Check if it's effectively an integer (no fractional part worth showing)
  double whole_part = (double)(int64)n;
  if (n == whole_part && n >= -9007199254740992.0 && n <= 9007199254740992.0) {
    // Write as integer
    int64 int_val = (int64)n;
    if (int_val == 0) {
      *ptr++ = '0';
    } else {
      // Convert to string in reverse
      char digits[32];
      int digit_count = 0;
      while (int_val > 0) {
        digits[digit_count++] = '0' + (int_val % 10);
        int_val /= 10;
      }

      // Write digits in correct order
      for (int i = digit_count - 1; i >= 0; i--) {
        *ptr++ = digits[i];
      }
    }
  } else {
    // Write as floating point with epsilon-based rounding
    int64 int_part = (int64)n;
    double frac_part = n - (double)int_part;

    // Write integer part
    if (int_part == 0) {
      *ptr++ = '0';
    } else {
      char digits[32];
      int digit_count = 0;
      int64 temp = int_part;
      while (temp > 0) {
        digits[digit_count++] = '0' + (temp % 10);
        temp /= 10;
      }

      for (int i = digit_count - 1; i >= 0; i--) {
        *ptr++ = digits[i];
      }
    }

    // Write fractional part if significant
    if (frac_part > 0.0000001) { // Only show decimals if meaningful
      *ptr++ = '.';

      // Determine precision limit based on magnitude
      int max_decimal_places;
      if (int_part > 1000000000000LL) { // > 1 trillion
        max_decimal_places = 3;
      } else if (int_part > 1000000000LL) { // > 1 billion
        max_decimal_places = 6;
      } else if (int_part > 1000000LL) { // > 1 million
        max_decimal_places = 9;
      } else {
        max_decimal_places = 12;
      }

      // Extract decimal digits with epsilon-based rounding
      char frac_str[20];
      int frac_len = 0;
      double epsilon = 0.0000001; // Threshold for rounding decisions

      for (int i = 0; i < max_decimal_places && frac_part > epsilon; i++) {
        frac_part *= 10.0;
        int32 digit = (int32)frac_part;

        // Check if we're very close to the next digit (rounding error)
        double remainder = frac_part - (double)digit;
        if (remainder > (1.0 - epsilon)) { // Very close to next digit
          digit++;
          if (digit >= 10) {
            // Handle carry-over case
            if (frac_len > 0) {
              // Try to carry to previous digit
              int carry_pos = frac_len - 1;
              while (carry_pos >= 0 && frac_str[carry_pos] == '9') {
                frac_str[carry_pos] = '0';
                carry_pos--;
              }
              if (carry_pos >= 0) {
                frac_str[carry_pos]++;
              } else {
                // Carry propagated all the way, need to increment integer part
                // This is rare but possible - just truncate the fraction
                frac_len = 0;
                break;
              }
            }
            break; // Stop processing more digits
          }
          remainder = 0.0;
        }

        frac_str[frac_len++] = '0' + (char)digit;
        frac_part = remainder;
      }

      // Remove trailing zeros, but keep at least one decimal if we started with
      // any
      while (frac_len > 1 && frac_str[frac_len - 1] == '0') {
        frac_len--;
      }

      // Copy fraction digits
      for (int i = 0; i < frac_len; i++) {
        *ptr++ = frac_str[i];
      }
    }
  }

  *ptr = '\0';
  return (size_t)(ptr - str);
}

size_t fmt_string(char *buffer, size_t buffer_size, const char *fmt,
                  const FmtArgs *args) {
  size_t buffer_idx = 0;
  size_t fmt_idx = 0;

  int8 arg_idx = 0;

  while (fmt[fmt_idx] != '\0' && buffer_idx < buffer_size - 1) {
    if (fmt[fmt_idx] != '%') {
      buffer[buffer_idx++] = fmt[fmt_idx++];
      continue;
    }

    // if we hit this the user has passed more args then the format
    debug_assert(arg_idx < args->args_size);
    if (arg_idx < args->args_size) {
      size_t arg_size = 0;
      FmtArg arg = args->args[arg_idx++];
      switch (arg.type) {
      case FMTARG_FLOAT:
        arg_size = f64_to_str(arg.value_f32, &buffer[buffer_idx]);
        break;
      case FMTARG_INT:
        arg_size = i64_to_str(arg.value_i32, &buffer[buffer_idx]);
        break;
      case FMTARG_UINT:
        arg_size = u64_to_str(arg.value_u32, &buffer[buffer_idx]);
        break;
      case FMTARG_CHAR:
        arg_size = 1;
        buffer[buffer_idx] = arg.value_char;
        break;
      case FMTARG_STR:
        arg_size = str_len(arg.value_str);
        for (uint32 i = 0; i < arg_size; i++) {
          if (buffer_idx + i < buffer_size) {
            buffer[buffer_idx + i] = arg.value_str[i];
          }
        }
        break;
      case FMTARG_STR_VIEW:
        arg_size = arg.value_str_view.len;
        for (uint32 i = 0; i < arg_size; i++) {
          if (buffer_idx + i < buffer_size) {
            buffer[buffer_idx + i] = arg.value_str_view.chars[i];
          }
        }
        break;
      case FMTARG_HEX:
        arg_size = hex64_to_str(arg.value_hex, &buffer[buffer_idx]);
        break;
      default:
        break;
      }

      buffer_idx += arg_size;
    }

    fmt_idx++;
  }

  buffer[buffer_idx] = '\0';
  return buffer_idx;
}

#include "string_builder.h"
#include "assert.h"
#include "lib/string.h"
#include "lib/fmt.h"

void sb_init(StringBuilder *sb, char *buffer, size_t size) {
  sb->buffer = buffer;
  sb->size = size;
  sb->len = 0;
  if (size > 0) {
    buffer[0] = '\0';
  }
}

void sb_clear(StringBuilder *sb) {
  sb->len = 0;
  if (sb->size > 0) {
    sb->buffer[0] = '\0';
  }
}

i32 sb_append_len(StringBuilder *sb, const char *str, u32 len) {
  if (!str)
    return 0;

  size_t available = sb->size - sb->len - 1; // -1 for null terminator

  if (len > available) {
    // Would overflow, append what we can
    if (available > 0) {
      memcpy(sb->buffer + sb->len, str, available);
      sb->len += available;
      sb->buffer[sb->len] = '\0';
    }
    debug_assert_msg(false, "Failed to append to string buffer. Required: % bytes, Available: % bytes, Total: % kb", FMT_UINT(len), FMT_UINT(available), FMT_UINT(BYTES_TO_KB(sb->size)));
    return -1; // Indicate truncation
  }

  memcpy(sb->buffer + sb->len, str, len);
  sb->len += len;
  sb->buffer[sb->len] = '\0';
  return 0;
}

i32 sb_append(StringBuilder *sb, const char *str) {
  return sb_append_len(sb, str, str_len(str));
}

i32 sb_append_char(StringBuilder *sb, char c) {
  if (!c)
    return 0;

  sb->buffer[sb->len] = c;
  sb->len += 1;
  sb->buffer[sb->len] = '\0';
  return 0;
};

i32 sb_append_space(StringBuilder *sb) { return sb_append_char(sb, ' '); }
i32 sb_append_line(StringBuilder *sb, const char *str) {
  sb_append(sb, str);
  return sb_append_char(sb, '\n');
}

char *sb_get(StringBuilder *sb) { return sb->buffer; }

size_t sb_length(StringBuilder *sb) { return sb->len; }

size_t sb_remaining(StringBuilder *sb) {
  return sb->size - sb->len - 1; // -1 for null terminator
}

// todo: use conversion function here from common.h or something
void sb_append_f32(StringBuilder *sb, f64 value, u32 decimal_places) {
  i32 int_part = (i32)value;
  f64 frac_part = value - int_part;

  char int_str[32];
  u32 int_len = 0;
  if (int_part == 0) {
    int_str[0] = '0';
    int_len = 1;
  } else {
    i32 temp = int_part;
    while (temp > 0) {
      int_str[int_len++] = '0' + (temp % 10);
      temp /= 10;
    }
    for (u32 i = 0; i < int_len / 2; i++) {
      char t = int_str[i];
      int_str[i] = int_str[int_len - 1 - i];
      int_str[int_len - 1 - i] = t;
    }
  }
  int_str[int_len] = '\0';
  // todo: handle error
  sb_append(sb, int_str);

  if (decimal_places > 0) {
    sb_append(sb, ".");
    for (u32 i = 0; i < decimal_places; i++) {
      frac_part *= 10;
      u32 digit = (u32)frac_part;
      char digit_char[2] = {'0' + (char)digit, '\0'};
      sb_append(sb, digit_char);
      frac_part -= digit;
    }
  }
}

void sb_append_u32(StringBuilder *sb, u32 value) {
  if (value == 0) {
    sb_append(sb, "0");
    return;
  }

  char buffer[16];
  u32 len = 0;
  while (value > 0) {
    buffer[len++] = '0' + (value % 10);
    value /= 10;
  }

  for (u32 i = len; i > 0; i--) {
    char digit[2] = {buffer[i - 1], '\0'};
    // todo: handle error
    sb_append(sb, digit);
  }
}

i32 _sb_append_format(StringBuilder *sb, const char *fmt, const FmtArgs *args) {
  char temp_buffer[KB(16)];
  fmt_string(temp_buffer, sizeof(temp_buffer), fmt, args);
  return sb_append(sb, temp_buffer);
}

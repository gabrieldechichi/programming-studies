#ifndef H_STRING
#define H_STRING
// use stb_ds. keeps null terminator
#include "macros.h"
#include "vendor/stb/stb_ds.h"
#include <string.h>

typedef struct {
  const char *value;
  size_t len;
} StringSlice;

StringSlice strslice_new(const char *str, int start, int end) {
  StringSlice s = {0};

  if (str == NULL) {
    return s;
  }

  int len = strlen(str);
  DEBUG_ASSERT(start <= end);
  DEBUG_ASSERT(start < len);
  DEBUG_ASSERT(end < len);

  if (start > end || start >= len || end >= len) {
    return s;
  }

  s.value = &str[start];
  s.len = (end - start) + 1;

  return s;
}

StringSlice strslice_new_len(const char *value, size_t len) {
  StringSlice s;
  s.value = value;
  s.len = len;
  return s;
}

StringSlice strslice_from_char_str(const char *value) {
  return strslice_new_len(value, strlen(value));
}

bool strslice_eq(StringSlice a, StringSlice b) {
  if (a.len != b.len) {
    return FALSE;
  }
  for (int i = 0; i < a.len; ++i) {
    if (a.value[i] != b.value[i]) {
      return FALSE;
    }
  }
  return TRUE;
}

bool strslice_eq_s(StringSlice a, const char *b) {
  int len_b = strlen(b);
  if (a.len != len_b) {
    return FALSE;
  }
  for (int i = 0; i < a.len; ++i) {
    if (a.value[i] != b[i]) {
      return FALSE;
    }
  }
  return TRUE;
}

typedef struct {
  char *value;
} String;

String string_new(const char *str) {
  String s = {0};
  if (str != NULL) {
    int len = strlen(str);
    arrsetlen(s.value, len + 1);
    for (int i = 0; i < len; i++) {
      s.value[i] = str[i];
    }
    s.value[len] = '\0';
  }
  return s;
}

String string_from_slice(const char *str, int start, int end) {
  String s = {0};

  if (str == NULL) {
    return s;
  }

  int len = strlen(str);
  DEBUG_ASSERT(start <= end);
  DEBUG_ASSERT(start < len);
  DEBUG_ASSERT(end < len);

  if (start > end || start >= len || end >= len) {
    return s;
  }

  size_t slice_len = (end - start) + 1;
  arrsetlen(s.value, slice_len + 1);
  for (int i = 0; i < slice_len; i++) {
    s.value[i] = str[i + start];
  }
  s.value[slice_len] = '\0';

  return s;
}

int str_len(const String s) { return arrlen(s.value) - 1; }

bool str_eq_c(const char *a, const char *b) { return strcmp(a, b) == 0; }
bool str_eq_s(const String a, const char *b) { return str_eq_c(a.value, b); }
bool str_eq(const String a, const String b) {
  if (str_len(a) != str_len(b)) {
    return FALSE;
  }
  return strcmp(a.value, b.value) == 0;
}

#endif

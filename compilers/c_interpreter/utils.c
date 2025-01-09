#ifndef H_UTILS
#define H_UTILS

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define internal static

#define GD_JOIN2(a, b) a##b
#define GD_JOIN3(a, b, c) GD_JOIN2(a, GD_JOIN2(b, c))
#define GD_JOIN4(a, b, c, d) GD_JOIN2(a, GD_JOIN2(b, GD_JOIN2(c, d)))

typedef struct {
  const char *value;
  int len;
} string_const;

typedef char bool;

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define ARRAY_LEN(array) (sizeof(array) / sizeof((array)[0]))
#define TRUE 1
#define FALSE 0


#define ASSERT assert

#define ASSERT_WITH_MSG(expr, msg, ...)                                        \
  if (!(expr)) {                                                               \
    fprintf(stderr, (msg), __VA_ARGS__);                                       \
    fprintf(stderr, "\n");                                                     \
    assert(expr);                                                              \
  }

#ifdef DEBUG
#define DEBUG_ASSERT(expr) assert(expr)
#else
#define DEBUG_ASSERT(expr) ((void)0)
#endif

string_const new_string_const(const char *str) {
  string_const s = {0};
  s.value = str;
  if (str == NULL) {
    s.len = 0;
  } else {
    s.len = strlen(str);
  }
  return s;
}

string_const string_const_from_slice(const char *str, int start, int end) {
  string_const s = {0};

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
  s.len = (end - start);

  return s;
}

bool string_const_eq(string_const a, string_const b) {
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

bool string_const_eq_s(string_const a, char *b) {
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

void string_const_print(string_const s) {
  for (int i = 0; i < s.len; i++) {
    printf("%c", s.value[i]);
  }
  printf(" (len: %d)\n", s.len);
}

#endif

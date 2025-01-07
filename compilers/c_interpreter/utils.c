#ifndef H_UTILS
#define H_UTILS

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
  const char *value;
  int len;
} string_const;

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define ARRAY_LEN(array) (sizeof(array) / sizeof((array)[0]))

#define ASSERT_WITH_MSG(expr, msg, ...)                                        \
  if (!(expr)) {                                                               \
    fprintf(stderr, (msg), __VA_ARGS__);                                       \
    fprintf(stderr, "\n");                                                     \
    assert(expr);                                                              \
  }

string_const new_string_const(const char *str) {
  string_const s;
  s.value = str;
  if (str == NULL) {
    s.len = 0;
  } else {
    s.len = strlen(str);
  }
  return s;
}

#endif

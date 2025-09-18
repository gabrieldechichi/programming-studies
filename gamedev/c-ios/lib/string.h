#ifndef H_STRING
#define H_STRING

#include "array.h"
#include "memory.h"
#include "typedefs.h"

// C dynamic string, still zero terminated
typedef struct {
  char *value;
  u32 len;
} String;

typedef struct {
  u32 len;
  char value[32];
} String32Bytes;
arr_define(String32Bytes);

typedef struct {
  u32 len;
  char value[64];
} String64Bytes;
arr_define(String64Bytes);

#define STR_FROM_CSTR(cstr)                                                    \
  ((String){.value = cstr, .len = ARRAY_SIZE(cstr) - 1})

String str_from_cstr_alloc(const char *cstr, u32 len, Allocator *allocator);
String32Bytes fixedstr32_from_cstr(char *cstr);
String64Bytes fixedstr64_from_cstr(char *cstr);

u32 str_len(const char *s);
u32 str_copy(char *to, char *from, u32 len);
b32 str_equal(const char *a, const char *b);
b32 str_equal_len(const char *a, u32 len_a, const char *b, u32 len_b);
b32 str_contains(const char *a, const char *b);
double str_to_double(const char *str);
String str_trim(String str, Allocator *allocator);
String str_trim_chars(String str, const char *trim_chars, Allocator *allocator);

size_t u32_to_str(uint32 n, char *str);
size_t hex32_to_str(uint32 n, char *str);

force_inline b32 char_is_space(char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' ||
         c == '\v';
}

force_inline b32 char_is_line_break(char c) { return c == '\n'; }

force_inline b32 char_is_digit(char c) { return c >= '0' && c <= '9'; }

#endif // !H_STRING

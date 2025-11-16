/*
    string.h - String types and utilities

    OVERVIEW

    --- No C stdlib strings, all strings store explicit length

    --- String: dynamic string (pointer + len)

    --- String32Bytes/String64Bytes: fixed-size stack strings

    --- utility functions: str_equal, str_contains, str_trim, conversions

    USAGE
        String s = STR_FROM_CSTR("hello");
        String s2 = str_from_cstr_alloc("world", &alloc);

        if (str_equal(s.value, s2.value)) { ... }

        String32Bytes fixed = fixedstr32_from_cstr("short string");
*/

#ifndef H_STRING
#define H_STRING

#include "array.h"
#include "memory.h"
#include "typedefs.h"

/* dynamic string with explicit length (still zero terminated) */
typedef struct {
  char *value;
  u32 len;
} String;
arr_define(String);

/* fixed-size string stored on stack (32 bytes) */
typedef struct {
  u32 len;
  char value[32];
} String32Bytes;
arr_define(String32Bytes);

/* fixed-size string stored on stack (64 bytes) */
typedef struct {
  u32 len;
  char value[64];
} String64Bytes;
arr_define(String64Bytes);

/* create String from C string literal (no allocation) */
#define STR_FROM_CSTR(cstr)                                                    \
  ((String){.value = cstr, .len = ARRAY_SIZE(cstr) - 1})

#define STR64_FROM_CSTR(cstr)                                                    \
  ((String64Bytes){.value = cstr, .len = ARRAY_SIZE(cstr) - 1})

/* allocate and copy C string to String */
String str_from_cstr_alloc(const char *cstr, Allocator *allocator);
String str_from_cstr_with_len_alloc(const char *cstr,u32 len, Allocator *allocator);
String32Bytes fixedstr32_from_cstr(char *cstr);
String64Bytes fixedstr64_from_cstr(char *cstr);

/* get length of C string */
u32 str_len(const char *s);
/* copy string, returns bytes copied */
u32 str_copy(char *to, char *from, u32 len);
/* compare strings for equality */
b32 str_equal(const char *a, const char *b);
b32 str_equal_len(const char *a, u32 len_a, const char *b, u32 len_b);
/* check if a contains b */
b32 str_contains(const char *a, const char *b);
/* parse double from string */
double str_to_double(const char *str);
/* trim whitespace from string */
String str_trim(String str, Allocator *allocator);
String str_trim_chars(String str, const char *trim_chars, Allocator *allocator);

/* convert u32 to string, returns bytes written */
size_t u32_to_str(uint32 n, char *str);
size_t hex32_to_str(uint32 n, char *str);

force_inline b32 char_is_space(char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' ||
         c == '\v';
}

force_inline b32 char_is_line_break(char c) { return c == '\n'; }

force_inline b32 char_is_digit(char c) { return c >= '0' && c <= '9'; }

#endif // !H_STRING

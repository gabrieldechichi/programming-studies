#ifndef LIB_H
#define LIB_H

#include "assert.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ERR_CODE_SUCCESS 0
#define ERR_CODE_FAIL -1
#define INDEX_INVALID -1

#define UNUSED(x) (void)x;
#define CODE(...) #__VA_ARGS__
#define ARRAY_LEN(arr) sizeof(arr) / sizeof(arr[0])
#define TYPE_NAME(t) #t

#define RESULT_STRUCT(t)                                                       \
    typedef struct {                                                           \
        t##_t value;                                                           \
        int error_code;                                                        \
    } t##_result_t

typedef struct {
    float *vertices;
    uint16_t *indices;
} mesh_t;

typedef struct {
    char *chars;
    uint16_t len;
    uint16_t cap;
} string_t;

RESULT_STRUCT(mesh);
RESULT_STRUCT(string);

static void str_free(string_t *s) {
    if (s && s->chars) {
        free(s->chars);
        *s = (string_t){0};
    }
}

static bool str_contains(string_t s, char c) {
    if (s.chars == NULL || s.len == 0) {
        return false;
    }
    return strchr(s.chars, c) != NULL;
}

// static int str_index_of(string_t s, int start, char c) {
//     if (s.chars == NULL || s.len == 0) {
//         return INDEX_INVALID;
//     }
//     for (int i = start; i < s.len; i++) {
//         if (s.chars[i] == c) {
//             return i;
//         }
//     }
//     return INDEX_INVALID;
// }

static bool str_eq_c(string_t a, char *b) {
    if (a.chars == b) {
        return true;
    }
    return strcmp(a.chars, b) == 0;
}

static string_t str_trim_start(string_t s) {
    if (s.chars == NULL || s.len == 0) {
        return s;
    }
    int i = 0;
    while (s.chars[i] == ' ' && i < s.len) {
        i++;
    }

    if (i == 0) {
        return s;
    }
    s.len -= i;
    memmove(s.chars, &s.chars[i], s.len);
    s.chars[s.len] = 0;
    return s;
}

// static bool str_eq(string_t a, string_t b) {
//     if (a.chars == b.chars){return true;}
//     if (a.len != b.len){return false;}
//     return strcmp(a.chars, b.chars) == 0;
// }

void println(const char *__restrict __format, ...);

char *fileReadAllText(const char *filePath);

string_result_t fileReadLine(FILE *file);

static inline uint8_t alignTo(uint8_t size, uint8_t alignment) {
    return (size + alignment - 1) & ~(alignment - 1);
}

mesh_result_t loadGeometry(const char *filename);

#endif

#ifndef STRING_H
#define STRING_H

#include "lib.h"
#include <stdlib.h>

typedef struct {
    char *chars;
    uint16_t len;
    uint16_t cap;
} string_t;

RESULT_STRUCT(string);

static string_result_t str_new(uint16_t len) {
    string_t str = {0};
    str.chars = (char *)malloc(len + 1);
    str.len = 0;
    str.cap = len;
    if (str.chars) {
        return (string_result_t){.value = str};
    }
    return (string_result_t){.error_code = ERR_CODE_FAIL};
}

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

string_result_t fileReadLine(FILE *file);
string_result_t fileReadAllText(const char *filePath);

// static bool str_eq(string_t a, string_t b) {
//     if (a.chars == b.chars){return true;}
//     if (a.len != b.len){return false;}
//     return strcmp(a.chars, b.chars) == 0;
// }
#endif

#ifndef STRING_H
#define STRING_H

#include "lib.h"
#include "stb/stb_ds.h"
#include <stdlib.h>

typedef char *String;

RESULT_STRUCT(String);

static StringResult str_new(uint16_t len) {
    String str = {0};
    arrsetcap(str, len + 1);
    if (str) {
        return (StringResult){.value = str};
    }
    return (StringResult){.errorCode = ERR_CODE_FAIL};
}

static void str_free(String *s) {
    if (s && *s) {
        arrfree(*s);
    }
}

static bool str_contains(String s, char c) {
    if (!s || !arrlen(s)) {
        return false;
    }
    return strchr(s, c) != NULL;
}

// static int str_index_of(string_t s, int start, char c) {
//     if (s== NULL || arrlen(s)== 0) {
//         return INDEX_INVALID;
//     }
//     for (int i = start; i < arrlen(s); i++) {
//         if (s[i] == c) {
//             return i;
//         }
//     }
//     return INDEX_INVALID;
// }

static bool str_eq_c(String a, char *b) {
    if (a == b) {
        return true;
    }
    return strcmp(a, b) == 0;
}

static String str_trim_start(String s) {
    if (s == NULL || arrlen(s) == 0) {
        return s;
    }
    int i = 0;
    while (s[i] == ' ' && i < arrlen(s)) {
        i++;
    }

    if (i == 0) {
        return s;
    }
    arrsetlen(s, arrlen(s) - i);
    memmove(s, &s[i], arrlen(s));
    s[arrlen(s)] = 0;
    return s;
}

StringResult fileReadLine(FILE *file);
StringResult fileReadAllText(const char *filePath);

// static bool str_eq(string_t a, string_t b) {
//     if (a== b){return true;}
//     if (arrlen(a)!= arrlen(b)){return false;}
//     return strcmp(a, b) == 0;
// }
#endif

#ifndef LIB_H
#define LIB_H

#include "assert.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int error_code_t;
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

RESULT_STRUCT(mesh);


void println(const char *__restrict __format, ...);


static inline uint8_t alignTo(uint8_t size, uint8_t alignment) {
    return (size + alignment - 1) & ~(alignment - 1);
}

mesh_result_t loadGeometry(const char *filename);

#endif

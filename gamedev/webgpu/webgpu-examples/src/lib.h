#ifndef LIB_H
#define LIB_H

#include "assert.h"
#include "cglm/types.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int ErrorCode;
#define ERR_CODE_SUCCESS 0
#define ERR_CODE_FAIL -1
#define INDEX_INVALID -1

#define UNUSED(x) (void)x;
#define CODE(...) #__VA_ARGS__
#define ARRAY_LEN(arr) sizeof(arr) / sizeof(arr[0])
#define TYPE_NAME(t) #t

#define RESULT_STRUCT(t)                                                       \
    typedef struct {                                                           \
        t value;                                                               \
        int errorCode;                                                         \
    } t##Result

#define CAST_ERROR(v, t)                                                       \
    (t) { .errorCode = v.errorCode }

#define RETURN_IF_ERROR(r, t)                                                  \
    if (r.errorCode) {                                                         \
        return CAST_ERROR(r, t);                                               \
    }

#define RETURN_CODE_IF_ERROR(r)                                                \
    if (r.errorCode) {                                                         \
        return r.errorCode;                                                    \
    }

typedef struct {
    float *vertices;
    uint16_t *indices;
} Mesh;

RESULT_STRUCT(Mesh);

typedef struct {
    vec3 pos;
    vec3 normal;
    vec4 col;
} VertexAttributes;

typedef struct {
    VertexAttributes* vertices;
} MeshObj;

RESULT_STRUCT(MeshObj);

void println(const char *__restrict __format, ...);

static inline uint8_t alignTo(uint8_t size, uint8_t alignment) {
    return (size + alignment - 1) & ~(alignment - 1);
}

static inline uint32_t ceilToNextMultiple(uint32_t value, uint32_t step) {
    uint32_t divide_and_ceil = value / step + (value % step == 0 ? 0 : 1);
    return step * divide_and_ceil;
}

MeshResult loadGeometry(const char *filename);
MeshObjResult loadObj(const char *filename);

#endif

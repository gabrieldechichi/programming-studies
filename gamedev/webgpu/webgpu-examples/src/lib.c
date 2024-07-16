#include "lib.h"
#include "fast_obj/fast_obj.h"
#include "lib/string.h"
#include "stb_ds.h"
#include <cglm/vec4.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void println(const char *__restrict __format, ...) {
    va_list args;
    va_start(args, __format);
    vprintf(__format, args);
    va_end(args);
    printf("\n");
}

typedef enum {
    GFILE_SECT_NONE = 0,
    GFILE_SECT_POINTS,
    GFILE_SECT_INDICES
} GeometryFileSection;

// Read file line by line
MeshResult loadGeometry(const char *filename) {
    MeshResult result = {0};

    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Error opening file %s\n", filename);
        perror("Error");
        result.errorCode = ERR_CODE_FAIL;
        return result;
    }

    GeometryFileSection section = GFILE_SECT_NONE;
    while (true) {
        if (feof(file)) {
            break;
        }
        StringResult lineResult = fileReadLine(file);
        if (lineResult.errorCode != ERR_CODE_SUCCESS) {
            result.errorCode = lineResult.errorCode;
            str_free(&lineResult.value);
            fclose(file);
            return result;
        }

        String line = str_trim_start(lineResult.value);
        if (!arrlen(line)) {
            str_free(&line);
            continue;
        }

        // comment
        if (str_contains(line, '#')) {
            printf("Skipping: %s\n", line);
            str_free(&line);
            continue;
        }

        // check section
        if (line[0] == '[') {
            if (str_eq_c(line, "[points]")) {
                section = GFILE_SECT_POINTS;
            } else if (str_eq_c(line, "[indices]")) {
                section = GFILE_SECT_INDICES;
            } else {
                fprintf(stderr, "Unexpected file section: %s", line);
                return (MeshResult){.errorCode = ERR_CODE_FAIL};
            }
        } else {
            switch (section) {
            case GFILE_SECT_NONE:
                break;
            case GFILE_SECT_POINTS: {
                char *startptr = &line[0];
                char *endptr = startptr;
                while (true) {
                    float n = strtof(startptr, &endptr);
                    if (startptr == endptr) {
                        if (endptr != NULL && *endptr != 0) {
                            fprintf(stderr,
                                    "Failed parsing number. Invalid character: "
                                    "%s\n",
                                    endptr);
                            return (MeshResult){.errorCode = ERR_CODE_FAIL};
                        }
                        break;
                    } else {
                        arrpush(result.value.vertices, n);
                        if (endptr == NULL || *endptr == '\n') {
                            break;
                        } else {
                            // check next character
                            startptr = endptr;
                        }
                    }
                };
                break;
            }
            case GFILE_SECT_INDICES: {
                char *startptr = &line[0];
                char *endptr = startptr;
                while (true) {
                    int n = strtod(startptr, &endptr);
                    if (startptr == endptr) {
                        if (endptr != NULL && *endptr != 0) {
                            fprintf(stderr,
                                    "Failed parsing number. Invalid character: "
                                    "%s\n",
                                    endptr);
                            return (MeshResult){.errorCode = ERR_CODE_FAIL};
                        }
                        break;
                    } else {
                        arrpush(result.value.indices, n);
                        if (endptr == NULL || *endptr == '\n') {
                            break;
                        } else {
                            // check next character
                            startptr = endptr;
                        }
                    }
                };
                break;
            }
            default:
                fprintf(stderr, "Unexpected section: %d", section);
                return (MeshResult){.errorCode = ERR_CODE_FAIL};
            }
        }

        str_free(&line);
    }

    fclose(file);

    return result;
}

MeshObjResult loadObj(const char *filename) {
    fastObjMesh *fastObjMesh = fast_obj_read(filename);
    if (!fastObjMesh) {
        return (MeshObjResult){.errorCode = ERR_CODE_FAIL};
    }

    MeshObj mesh = {0};
    // non indexed drawin so we can have different normals per vertex
    arrsetcap(mesh.vertices, fastObjMesh->face_count * 4);

    int indexIdx = 0;
    for (uint32_t f = 0; f < fastObjMesh->face_count; f++) {
        int faceVertCount = fastObjMesh->face_vertices[f];

        assert(faceVertCount <= 4);
        for (int i = 1; i < faceVertCount - 1; i++) {
            fastObjIndex tri1 = fastObjMesh->indices[0 + indexIdx];
            fastObjIndex tri2 = fastObjMesh->indices[i + indexIdx];
            fastObjIndex tri3 = fastObjMesh->indices[i + 1 + indexIdx];

            VertexAttributes v1 = {
                .pos = {fastObjMesh->positions[3 * tri1.p + 0],
                        fastObjMesh->positions[3 * tri1.p + 1],
                        fastObjMesh->positions[3 * tri1.p + 2]},
                .normal = {fastObjMesh->normals[3 * tri1.n + 0],
                           fastObjMesh->normals[3 * tri1.n + 1],
                           fastObjMesh->normals[3 * tri1.n + 2]},
                .col = {1, 1, 1, 1},
            };

            VertexAttributes v2 = {
                .pos = {fastObjMesh->positions[3 * tri2.p + 0],
                        fastObjMesh->positions[3 * tri2.p + 1],
                        fastObjMesh->positions[3 * tri2.p + 2]},
                .normal = {fastObjMesh->normals[3 * tri2.n + 0],
                           fastObjMesh->normals[3 * tri2.n + 1],
                           fastObjMesh->normals[3 * tri2.n + 2]},
                .col = {1, 1, 1, 1},
            };

            VertexAttributes v3 = {
                .pos = {fastObjMesh->positions[3 * tri3.p + 0],
                        fastObjMesh->positions[3 * tri3.p + 1],
                        fastObjMesh->positions[3 * tri3.p + 2]},
                .normal = {fastObjMesh->normals[3 * tri3.n + 0],
                           fastObjMesh->normals[3 * tri3.n + 1],
                           fastObjMesh->normals[3 * tri3.n + 2]},
                .col = {1, 1, 1, 1},
            };

            arrpush(mesh.vertices, v1);
            arrpush(mesh.vertices, v2);
            arrpush(mesh.vertices, v3);
        }

        indexIdx += faceVertCount;
    }

    fast_obj_destroy(fastObjMesh);

    return (MeshObjResult){.value = mesh};
}

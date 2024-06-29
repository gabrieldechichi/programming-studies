#include "lib.h"
#include "lib/string.h"
#include "stb_ds.h"
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
} geo_file_section_e;

// Read file line by line
mesh_result_t loadGeometry(const char *filename) {
    mesh_result_t result = {0};

    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Error opening file %s\n", filename);
        perror("Error");
        result.error_code = ERR_CODE_FAIL;
        return result;
    }

    geo_file_section_e section = GFILE_SECT_NONE;
    while (true) {
        if (feof(file)) {
            break;
        }
        string_result_t lineResult = fileReadLine(file);
        if (lineResult.error_code != ERR_CODE_SUCCESS) {
            result.error_code = lineResult.error_code;
            str_free(&lineResult.value);
            fclose(file);
            return result;
        }

        string_t line = str_trim_start(lineResult.value);
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
                return (mesh_result_t){.error_code = ERR_CODE_FAIL};
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
                            return (mesh_result_t){.error_code = ERR_CODE_FAIL};
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
                            return (mesh_result_t){.error_code = ERR_CODE_FAIL};
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
                return (mesh_result_t){.error_code = ERR_CODE_FAIL};
            }
        }

        str_free(&line);
    }

    fclose(file);

    return result;
}

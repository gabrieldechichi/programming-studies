#include "lib.h"
#include <stdarg.h>
#include <stdio.h>

#include <stdio.h>
#include <stdlib.h>

void println(const char *__restrict __format, ...) {
    va_list args;
    va_start(args, __format);
    vprintf(__format, args);
    va_end(args);
    printf("\n");
}

char *fileReadAllText(const char *filePath) {
    FILE *file = fopen(filePath, "r");
    if (file) {
        fseek(file, 0, SEEK_END);
        long length = ftell(file);
        fseek(file, 0, SEEK_SET);
        char *buffer = malloc(length + 1);
        if (buffer) {
            fread(buffer, 1, length, file);
            buffer[length] = '\0';
        }
        fclose(file);
        return buffer;
    }
    return NULL;
}

#include "lib.h"
#include <stdarg.h>
#include <stdio.h>

#include <stdio.h>

void println(const char *__restrict __format, ...) {
    va_list args;
    va_start(args, __format);
    vprintf(__format, args);
    va_end(args);
    printf("\n");
}

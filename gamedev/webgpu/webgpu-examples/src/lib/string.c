#include "string.h"
#include "lib.h"
#include "stb/stb_ds.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

StringResult fileReadLine(FILE *file) {
    const int max_size = 256;
    StringResult r = str_new(max_size);
    if (r.errorCode) {
        return r;
    }

    if (fgets(r.value, arrcap(r.value), file) != NULL) {
        // TODO: asset it's actually end of line
        arrsetlen(r.value, strlen(r.value) - 1);
        // skip '\n'
        r.value[arrlen(r.value)] = '\0';
    }

    return r;
}

StringResult fileReadAllText(const char *filePath) {
    FILE *file = fopen(filePath, "r");
    if (file) {
        fseek(file, 0, SEEK_END);
        long length = ftell(file);
        fseek(file, 0, SEEK_SET);

        StringResult r = str_new(length);
        if (r.errorCode != ERR_CODE_SUCCESS) {
            return r;
        }
        fread(r.value, 1, length, file);
        r.value[length] = '\0';
        fclose(file);
        return r;
    }
    return (StringResult){.errorCode = ERR_CODE_FAIL};
}

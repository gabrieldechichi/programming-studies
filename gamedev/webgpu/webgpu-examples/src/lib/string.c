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

string_result_t fileReadLine(FILE *file) {
    const int max_size = 256;
    string_result_t r = str_new(max_size);
    if (r.error_code) {
        return r;
    }

    if (fgets(r.value.chars, r.value.cap, file) != NULL) {
        // TODO: asset it's actually end of line
        r.value.len = strlen(r.value.chars) - 1;
        // skip '\n'
        r.value.chars[r.value.len] = '\0';

        printf("(%d, %d) %s\n", r.value.len, r.value.cap, r.value.chars);
    }

    return r;
}

string_result_t fileReadAllText(const char *filePath) {
    FILE *file = fopen(filePath, "r");
    if (file) {
        fseek(file, 0, SEEK_END);
        long length = ftell(file);
        fseek(file, 0, SEEK_SET);

        string_result_t r = str_new(length);
        if (r.error_code != ERR_CODE_SUCCESS) {
            return r;
        }
        fread(r.value.chars, 1, length, file);
        r.value.chars[length] = '\0';
        fclose(file);
        return r;
    }
    return (string_result_t){.error_code = ERR_CODE_FAIL};
}

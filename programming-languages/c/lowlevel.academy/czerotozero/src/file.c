#include "file.h"
#include "common.h"

int create_db_file(char *filename, FILE **file) {
    {
        FILE *f = fopen(filename, "r");
        if (f != NULL) {
            printf("File %s already exists!\n", filename);
            fclose(f);
            return STATUS_ERROR;
        }
    }

    {
        FILE *f = fopen(filename, "w+");
        if (!f) {
            printf("Failed to create file %s\n", filename);
            perror(filename);
            return STATUS_ERROR;
        }

        *file = f;
        return STATUS_SUCCESS;
    }
}

int open_db_file(char *filename, FILE **file) {
    FILE *f = fopen(filename, "r+");
    if (!f) {
        printf("Failed to open file %s\n", filename);
        perror(filename);
        return STATUS_ERROR;
    }
    *file = f;
    return STATUS_SUCCESS;
}

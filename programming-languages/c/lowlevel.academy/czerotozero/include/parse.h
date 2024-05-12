#ifndef PARSE_H
#define PARSE_H
#include "types.h"
#include <stdio.h>
#define HEADER_MAGIC 0x4c4c4144

typedef struct {
    uint magic;
    ushort version;
    ushort count;
    uint file_size;
} db_header_t;

typedef struct {
    char name[256];
    char address[256];
    uint hours;
} employee_t;

int new_db_header_alloc(db_header_t **header_out);
int read_header_alloc(FILE *f, db_header_t **header_out);
int write_db_file(FILE *f, db_header_t *header);
#endif

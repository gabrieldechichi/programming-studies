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

typedef struct {
    db_header_t* header;
    employee_t* employess;
} db_t;

int new_db_alloc(db_t **db_out);
int read_db_file(FILE *f, db_t **db_out);
int write_db_file(FILE *f, db_t *db);
int parse_employee(char* str, employee_t* out_employee);
int add_employee(db_t* db, const employee_t* employee);
void free_db(db_t** db);
#endif

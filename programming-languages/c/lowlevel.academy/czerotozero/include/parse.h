#ifndef PARSE_H
#define PARSE_H
#include "types.h"

int write_header(db_header_t *header, char *file_name); 
int read_header(db_header_t *header, char *file_name); 
#endif

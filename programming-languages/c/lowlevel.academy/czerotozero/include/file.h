#ifndef FILE_H
#define FILE_H
#include <stdio.h>

int create_db_file(char* filename, FILE** file);
int open_db_file(char* filename, FILE** file);
#endif

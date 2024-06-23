#ifndef LIB_H
#define LIB_H

#include "assert.h"

#define ERR_CODE_SUCCESS 0
#define ERR_CODE_FAIL -1

#define UNUSED(x) (void)x;
#define CODE(...) #__VA_ARGS__

void println(const char *__restrict __format, ...);

char* fileReadAllText(const char* filePath);

#endif

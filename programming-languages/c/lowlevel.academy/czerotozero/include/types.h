#ifndef TYPES_H
#define TYPES_H
typedef unsigned int uint;
typedef unsigned short ushort;

typedef struct {
    ushort version;
    ushort employess;
    uint file_size;
} db_header_t;
#endif

#include <stdio.h>
#include "types.h"
#include "parse.h"

int main(int argc, char *args[]) {
    if (argc != 2) {
        printf("Usage: %s file\n", args[0]);
        return -1;
    }
    char *file_name = args[1];
    {
        db_header_t header = {0};
        header.version = 1;
        header.employess = 64;
        header.file_size = sizeof(header);
        write_header(&header, file_name);
    }

    // test read
    {
        db_header_t header = {0};
        if (read_header(&header, file_name) < 0) {
            return -1;
        }
        printf("Version: %d\nEmployees: %d\nFile Size: %d\n", header.version,
               header.employess, header.file_size);
    }
    return 0;
}

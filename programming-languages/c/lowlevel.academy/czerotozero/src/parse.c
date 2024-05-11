#include "parse.h"
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>

int write_header(db_header_t *header, char *file_name) {
    FILE *file = fopen(file_name, "w+");
    if (!file) {
        printf("Error write_header:%d", __LINE__);
        perror(file_name);
        return -1;
    }
    if (fwrite(header, sizeof(*header), 1, file) != 1) {
        printf("Error writing to file %s. (write_header:%d)", file_name,
               __LINE__);
        perror(file_name);
        fclose(file);
        return -1;
    }
    fclose(file);
    return 0;
}

int read_header(db_header_t *header, char *file_name) {
    if (!header) {
        printf("invalid header pointer");
        return -1;
    }

    FILE *file = fopen(file_name, "r");
    if (!file) {
        printf("Error read_header:%d", __LINE__);
        perror(file_name);
        return -1;
    }
    if (fread(header, sizeof(*header), 1, file) != 1) {
        printf("Error reading header %s. (write_header:%d)", file_name,
               __LINE__);
        perror(file_name);
        fclose(file);
        return -1;
    }

    int fd = fileno(file);
    if (fd == -1) {
        perror("Error getting file descriptor from file");
        fclose(file);
        return -1;
    }

    struct stat file_stat = {0};
    if (fstat(fd, &file_stat) < 0) {
        printf("{%s}:{%d}", __FILE__, __LINE__);
        perror("fstat");
        fclose(file);
        return -1;
    }

    if (file_stat.st_size != header->file_size) {
        printf("Get out of here hacker! %ld, %d", file_stat.st_size,
               header->file_size);
        fclose(file);
        return -1;
    }

    fclose(file);
    return 0;
}

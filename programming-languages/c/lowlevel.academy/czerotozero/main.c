#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

typedef unsigned short ushort;
typedef unsigned int uint;

typedef struct {
    ushort version;
    ushort employees;
    uint filesize;
} db_header_t;

typedef struct stat file_stat;

int write_sample_header(char *file_name) {
    db_header_t header = {0};
    header.version = 1;
    header.employees = 65;
    header.filesize = sizeof(header);

    FILE *file = fopen(file_name, "w+");
    if (file == NULL) {
        perror("write_sample_header, fopen");
        return -1;
    }

    size_t size = fwrite(&header, sizeof(header), 1, file);
    if (size != 1) {
        perror("Error writing to file");
        return -1;
    }
    fclose(file);
    return 0;
}

int read_header(char *file_name) {
    FILE *file = fopen(file_name, "r");
    if (file == NULL) {
        perror("read_header");
        return -1;
    }
    db_header_t header = {0};
    size_t size = fread(&header, sizeof(db_header_t), 1, file);
    if (size != 1) {
        perror("Error reading file: mismatch size");
        fclose(file);
        return -1;
    }

    int fd = fileno(file);
    if (fd == -1) {
        perror("Error getting file descriptor from FILE*");
        fclose(file);
        return 1;
    }

    file_stat file_stat = {0};
    if (fstat(fd, &file_stat) < 0) {
        perror("fstat");
        fclose(file);
        return -1;
    }

    if (file_stat.st_size != header.filesize) {
        printf("Get out of here hacker!\n");
        return -1;
    }

    printf("version: %d\nemployes: %d\nfilesize: %d\n", header.version,
           header.employees, header.filesize);
    fclose(file);
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <filename>\n", argv[0]);
        return -1;
    }

    char *file_name = argv[1];
    write_sample_header(file_name);

    read_header(file_name);
    return 0;
}

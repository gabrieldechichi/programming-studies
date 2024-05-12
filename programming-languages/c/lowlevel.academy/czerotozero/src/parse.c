#include "parse.h"
#include "common.h"
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

void header_hton(db_header_t* header){
    header->magic = htonl(header->magic);
    header->version = htons(header->version);
    header->count = htons(header->count);
    header->file_size = htonl(header->file_size);
}
void header_ntoh(db_header_t* header){
    header->magic = ntohl(header->magic);
    header->version = ntohs(header->version);
    header->count = ntohs(header->count);
    header->file_size = ntohl(header->file_size);
}

int new_db_header_alloc(db_header_t** header_out){
    db_header_t* header = malloc(sizeof(db_header_t));
    header->magic = HEADER_MAGIC;
    header->version = 0x1;
    header->count = 0;
    header->file_size = sizeof(*header);
    *header_out = header;
    return STATUS_SUCCESS;
}

int read_header_alloc(FILE *f, db_header_t **header_out){
    fseek(f, 0, SEEK_SET);
    db_header_t* header = malloc(sizeof(db_header_t));
    if (fread(header, sizeof(db_header_t), 1, f) != 1){
        perror("fread");
        return STATUS_ERROR;
    }

    //read network compliant data
    header_ntoh(header);
    *header_out = header;
    return STATUS_SUCCESS;
}

int write_db_file(FILE *f, db_header_t *header){
    //write network compliant data
    header_hton(header);

    fseek(f, 0, SEEK_SET);
    if (fwrite(header, sizeof(*header), 1, f) != 1){
        perror("fwrite");
        return STATUS_ERROR;
    }
    header_ntoh(header);
    return STATUS_SUCCESS;
}

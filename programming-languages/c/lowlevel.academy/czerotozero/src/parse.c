#include "parse.h"
#include "common.h"
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

typedef struct stat file_stat;

void db_header_hton(db_header_t *header) {
    header->magic = htonl(header->magic);
    header->version = htons(header->version);
    header->count = htons(header->count);
    header->file_size = htonl(header->file_size);
}

void db_header_ntoh(db_header_t *header) {
    header->magic = ntohl(header->magic);
    header->version = ntohs(header->version);
    header->count = ntohs(header->count);
    header->file_size = ntohl(header->file_size);
}

void db_employess_hton(employee_t *employees, ushort employee_count) {
    for (int i = 0; i < employee_count; ++i) {
        employee_t *employee = &employees[i];
        employee->hours = htonl(employee->hours);
    }
}

void db_employess_ntoh(employee_t *employees, ushort employee_count) {
    for (int i = 0; i < employee_count; ++i) {
        employee_t *employee = &employees[i];
        employee->hours = ntohl(employee->hours);
    }
}

void db_hton(db_t *db) {
    int employee_count = db->header->count;
    db_header_hton(db->header);
    db_employess_hton(db->employess, employee_count);
}
void db_ntoh(db_t *db) {
    db_header_ntoh(db->header);
    db_employess_ntoh(db->employess, db->header->count);
}

uint calc_db_size(const db_t *db) {
    return sizeof(db_header_t) + sizeof(employee_t) * db->header->count;
}

int new_db_alloc(db_t **db_out) {
    db_t *db = malloc(sizeof(db_t));
    db->header = malloc(sizeof(db_header_t));
    db->header->magic = HEADER_MAGIC;
    db->header->version = 0x1;
    db->header->count = 0;
    db->header->file_size = calc_db_size(db);
    db->employess = NULL;

    *db_out = db;
    return STATUS_SUCCESS;
}

void free_db(db_t **db) {
    if (!db || !(*db)) {
        return;
    }

    db_t *db_ptr = *db;
    if (db_ptr->header) {

        free(db_ptr->header);
    }
    if (db_ptr->employess) {
        free(db_ptr->employess);
    }
    free(db_ptr);
    *db = NULL;
}

int read_db_file(FILE *f, db_t **db_out) {
    fseek(f, 0, SEEK_SET);
    db_t *db = malloc(sizeof(db_t));
    db->header = NULL;
    db->employess = NULL;
    db->header = malloc(sizeof(db_header_t));
    if (fread(db->header, sizeof(db_header_t), 1, f) != 1) {
        perror("fread");
        free_db(&db);
        return STATUS_ERROR;
    }
    db_header_ntoh(db->header);

    // validate header
    {
        if (db->header->magic != HEADER_MAGIC) {
            printf("Invalid magic number %d\n", db->header->magic);
            free_db(&db);
            return STATUS_ERROR;
        }

        // todo: error check
        int fd = fileno(f);
        file_stat stat = {0};
        fstat(fd, &stat);

        if (db->header->file_size != stat.st_size) {
            printf("Corrupted header\n");
            free_db(&db);
            return STATUS_ERROR;
        }
    }

    db->employess = malloc(sizeof(employee_t) * db->header->count);
    if (fread(db->employess, sizeof(employee_t), db->header->count, f) !=
        db->header->count) {
        perror("Error reading employees from file");
        free_db(&db);
        return STATUS_ERROR;
    }
    db_employess_ntoh(db->employess, db->header->count);

    *db_out = db;

    return STATUS_SUCCESS;
}

int write_db_file(FILE *f, db_t *db) {
    // write network compliant data
    ushort employee_count = db->header->count;
    db_hton(db);

    fseek(f, 0, SEEK_SET);
    if (fwrite(db->header, sizeof(*db->header), 1, f) != 1) {
        perror("fwrite header");
        return STATUS_ERROR;
    }
    if (fwrite(db->employess, sizeof(*db->employess), employee_count, f) !=
        employee_count) {
        perror("fwrite employees");
        return STATUS_ERROR;
    }

    db_ntoh(db);
    return STATUS_SUCCESS;
}

int parse_employee(char *str, employee_t *out_employee) {
    employee_t employee = {0};
    char *name = strtok(str, ",");
    char *address = strtok(NULL, ",");
    char *hour = strtok(NULL, ",");

    // todo: error check
    strcpy((char *)&employee.name, name);
    strcpy((char *)&employee.address, address);
    employee.hours = atoi(hour);

    *out_employee = employee;
    return STATUS_SUCCESS;
}

int add_employee(db_t *db, const employee_t *employee) {
    db->header->count++;
    db->employess =
        realloc(db->employess, sizeof(employee_t) * db->header->count);
    if (db->employess == NULL) {
        perror("realloc (add_employees)");
        return STATUS_ERROR;
    }
    db->employess[db->header->count - 1] = *employee;
    db->header->file_size = calc_db_size(db);
    return STATUS_SUCCESS;
}

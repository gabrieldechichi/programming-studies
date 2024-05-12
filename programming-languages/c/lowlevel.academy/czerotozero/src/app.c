#include "app.h"
#include "common.h"
#include "file.h"
#include "parse.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void print_usage(char *args[]) {
    printf("Usage: %s <flags>\n", args[0]);
    printf("\t -n: new file\n");
    printf("\t -f: file path\n");
}

int run(app_run_params_t app_run_params) {
    if (!app_run_params.filepath) {
        printf("Missing file path. Use the -f flag\n");
        print_usage(app_run_params.args);
        return STATUS_ERROR;
    }

    FILE *dbfile = NULL;
    db_header_t* header = NULL;
    if (app_run_params.newfile) {
        if (create_db_file(app_run_params.filepath, &dbfile) == STATUS_ERROR) {
            return STATUS_ERROR;
        }
        if (new_db_header_alloc(&header) == STATUS_ERROR){
            return STATUS_ERROR;
        }
    } else {
        if (open_db_file(app_run_params.filepath, &dbfile) == STATUS_ERROR) {
            return STATUS_ERROR;
        }
        if (read_header_alloc(dbfile, &header) == STATUS_ERROR){
            return STATUS_ERROR;
        }
    }

    write_db_file(dbfile, header);
    free(header);
    fclose(dbfile);

    return STATUS_SUCCESS;
}

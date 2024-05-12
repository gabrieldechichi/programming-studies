#include "app.h"
#include "common.h"
#include "file.h"
#include <stdio.h>
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
        return -1;
    }

    FILE *dbfile;
    if (app_run_params.newfile) {
        if (create_db_file(app_run_params.filepath, &dbfile) == STATUS_ERROR) {
            return -1;
        }
    } else {
        if (open_db_file(app_run_params.filepath, &dbfile) == STATUS_ERROR) {
            return -1;
        }
    }

    printf("%p\n", dbfile);

    fclose(dbfile);

    return STATUS_SUCCESS;
}

#include "app.h"
#include "common.h"
#include "file.h"
#include "db.h"
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
    db_t *db = NULL;
    if (app_run_params.newfile) {
        if (create_db_file(app_run_params.filepath, &dbfile) == STATUS_ERROR) {
            return STATUS_ERROR;
        }
        if (new_db_alloc(&db) == STATUS_ERROR) {
            return STATUS_ERROR;
        }
    } else {
        if (open_db_file(app_run_params.filepath, &dbfile) == STATUS_ERROR) {
            return STATUS_ERROR;
        }
        if (read_db_file(dbfile, &db) == STATUS_ERROR) {
            return STATUS_ERROR;
        }
    }

    if (app_run_params.employee_to_add) {
        db_employee_t new_employee;
        if (parse_employee(app_run_params.employee_to_add, &new_employee) ==
            STATUS_ERROR) {
            return STATUS_ERROR;
        }
        if (add_employee(db, &new_employee) == STATUS_ERROR) {
            return STATUS_ERROR;
        }
    }

    if (app_run_params.list_employees) {
        for (int i = 0; i < db->header->count; ++i) {
            db_employee_t *employee = &db->employess[i];
            printf("Employee %d:\n\tName: %s\n\tAddress: %s\n\tHours: %u\n",
                   i + 1, employee->name, employee->address, employee->hours);
        }
    }

    write_db_file(dbfile, db);
    free_db(&db);
    fclose(dbfile);

    return STATUS_SUCCESS;
}

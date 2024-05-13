#ifndef APP_H
#define APP_H

#include "db.h"
#include <stdbool.h>

typedef struct {
    int argc;
    char** args;
    bool newfile;
    char *filepath;
    char* employee_to_add;
    bool list_employees;
} app_run_params_t;

void print_usage(char *args[]);
int run(app_run_params_t app_run_params);

#endif

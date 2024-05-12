#ifndef APP_H
#define APP_H

#include <stdbool.h>

typedef struct {
    int argc;
    char** args;
    bool newfile;
    char *filepath;
} app_run_params_t;

void print_usage(char *args[]);
int run(app_run_params_t app_run_params);

#endif

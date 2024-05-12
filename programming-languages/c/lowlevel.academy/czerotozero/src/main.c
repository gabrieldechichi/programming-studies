#include "app.h"
#include <stdio.h>
#include <unistd.h>

int main(int argc, char *args[]) {
    app_run_params_t app_run_params = {0};
    app_run_params.argc = argc;
    app_run_params.args = args;
    int f = -1;
    while ((f = getopt(argc, args, "nf:")) != -1) {
        switch (f) {
        case 'n':
            app_run_params.newfile = true;
            break;
        case 'f':
            app_run_params.filepath = optarg;
            break;
        case '?':
            printf("Unknown option: %c", f);
            break;
        }
    }

    return run(app_run_params);
}

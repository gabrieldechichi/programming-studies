#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

void print_usage(char* args[]){
    printf("Usage: %s <flags>\n", args[0]);
    printf("\t -n: new file\n");
    printf("\t -f: file path\n");
}

int main(int argc, char *args[]) {
    bool newfile = false;
    char *filepath = NULL;
    int f = -1;
    while ((f = getopt(argc, args, "nf:")) != -1) {
        switch (f) {
        case 'n':
            newfile = true;
            break;
        case 'f':
            filepath = optarg;
            break;
        case '?':
            printf("Unknown option: %c", f);
            break;
        }
    }

    if (!filepath){
        printf("Missing file path. Use the -f flag\n");
        print_usage(args);
        return -1;
    }

    printf("New file: %b\n", newfile);
    printf("Filepath: %s\n", filepath);

}

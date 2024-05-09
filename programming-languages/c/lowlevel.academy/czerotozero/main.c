#include <stdio.h>
#include <string.h>

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <filename>\n", argv[0]);
        return -1;
    }

    char *file_name = argv[1];

    FILE *file = fopen(file_name, "w+");
    if (file == NULL) {
        perror("Error opening file");
        return -1;
    }

    char *buffer = "Hello new file!";
    fwrite(buffer, sizeof(*buffer), strlen(buffer), file);

    fclose(file);
    return 0;
}

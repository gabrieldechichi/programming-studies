#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

int main(void) {
    uint32_t x = 22;
    // Prints to stderr
    fprintf(stderr, "Not hellb, %u!\n", x);

    // Basic stdout print in C
    printf("Run `make test` to run the tests.\n");
    fflush(stdout); // equivalent to Zig's `flush`

    char msg_buf[4096];
    // Prompt for user input
    fprintf(stdout, "Write something: ");
    fflush(stdout); // Make sure 'Write something' is printed before reading input

    char* msg = fgets(msg_buf, sizeof(msg_buf), stdin);
    if (msg != NULL) {
        // Remove the newline character if it exists
        char* newline = strchr(msg, '\n');
        if (newline) {
            *newline = '\0';
        }
        fprintf(stderr, "msg: %s\n", msg);
    }

    return 0;
}

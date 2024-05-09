#include <stdio.h>


int main() {
    printf("heyo\n");
    #if DEBUG
    printf("We are in debug mode: %s/%d\n\n", __FILE__, __LINE__); 
    #endif
}

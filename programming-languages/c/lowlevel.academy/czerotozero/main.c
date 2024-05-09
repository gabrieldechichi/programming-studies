#include <stdbool.h>
#include <stdio.h>

typedef struct {
    int id;
    char firstname[64];
    char lastname[64];
    float income;
    bool ismanager;
} __attribute((__packed__)) employee_t;

int main() {
    int x = 2;
    int y = 2;
    bool b = true;
    printf("&x: %p\n&y: %p\n&b: %p\n", &x, &y, &b);
    printf("&x - &y: %ld\n", (long)((char *)&y - (char *)&x));
}

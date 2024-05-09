#include <stdbool.h>
#include <stdio.h>

typedef struct {
    int id;
    char firstname[64];
    char lastname[64];
    float income;
    bool ismanager;
} __attribute((__packed__)) employee_t;

int main() { printf("Size of struct %d \n", (int)sizeof(employee_t)); }

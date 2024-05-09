#include <stdbool.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    int id;
    char firstname[64];
    char lastname[64];
    float income;
    bool ismanager;
} employee_t;

int main() {
    employee_t fred = {};
    fred.id = 0;
    fred.ismanager = true;
    strcpy(fred.firstname, "Fred");
    printf("Employee name: %s (%d)\n", fred.firstname, (int) strlen(fred.firstname));
}

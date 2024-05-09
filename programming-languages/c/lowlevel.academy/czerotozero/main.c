#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int id;
    char firstname[64];
    char lastname[64];
    float income;
    bool ismanager;
} __attribute((__packed__)) employee_t;

void init_employee(employee_t *employee) {
    memset(employee, 0, sizeof(employee_t));
    employee->id = 1;
    strcpy(employee->firstname, "Fred");
    employee->ismanager = false;
}

void print_employee(employee_t *employee) {
    if (employee) {
        printf("ID: %d\nName: %s %s (%d)\n", employee->id, employee->firstname,
               employee->lastname,
               (int) (strlen(employee->firstname) + strlen(employee->lastname)));
    } else {
        printf("Null employee");
    }
}

int main() {
    // comes from db header
    int n = 4;

    employee_t *employees = malloc(sizeof(employee_t) * n);
    if (employees == NULL) {
        printf("Failed allocating employees. %s:%d\n", __FILE__, __LINE__);
        return -1;
    }
    for (int i = 0; i < n; i++) {
        init_employee(&employees[i]);
        print_employee(&employees[i]);
    }
}

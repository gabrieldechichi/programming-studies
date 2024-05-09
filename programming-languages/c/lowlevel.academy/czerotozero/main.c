#include <stdio.h>
typedef union {
    int x;
    char c;
    short s;
} myunion_n;

int main() {
    myunion_n n;
    n.c = 'h';
    printf("%#x, %c\n", n.x, n.c);
}

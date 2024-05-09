#include <stdio.h>
#include <string.h>

int main() {
    char *str = "hehe";
    char other_str[] = {'h', 'e', 'h', 'e', 0};
    if (!strcmp(str, other_str)){
        printf("Yay!\n");
    } else{
        printf("Nay!\n");
    }
}

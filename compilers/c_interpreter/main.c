#include "./lexer_test.c"
#include "./parser_test.c"
#include "global.c"
#include <stdio.h>

int main() {
  ASSERT(global_context_init() == 0);
  test_lexer();
  test_parser();
  printf("ALL TEST SUCCEEDED");
}

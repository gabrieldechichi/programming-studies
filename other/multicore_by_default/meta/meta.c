#include "lib/memory.h"
#include "lib/string.h"
#include "tokenizer.h"
#include <stdio.h>
#include <stdlib.h>
#include "lib/typedefs.h"

#include "lib/string.c"
#include "lib/memory.c"
#include "lib/common.c"
#include "os/os.c"
#include "meta/tokenizer.c"

int main() {
  const char *source = "HZ_TASK() \
    typedef struct { \
        HZ_READ()\
        u64 values_start; \
        HZ_WRITE()\
        i64_Array numbers; \
    } TaskWideSumInit;";

  ArenaAllocator arena = arena_from_buffer(malloc(MB(8)), MB(8));
  Allocator allocator = make_arena_allocator(&arena);
  Tokenizer tokenizer =
      tokenizer_create("test", source, str_len(source), &allocator);

  while (true) {
    Token tok = tokenizer_next_token(&tokenizer);
    if (tok.type == TOKEN_EOF || tok.type == TOKEN_INVALID) {
      break;
    }

    printf("%s\n", token_type_to_string(tok.type));
  }

  return 0;
}

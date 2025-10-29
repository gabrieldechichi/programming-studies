#include "lib/memory.h"
#include "lib/string.h"
#include "parser.h"
#include "tokenizer.h"
#include <stdio.h>
#include <stdlib.h>
#include "lib/typedefs.h"

#include "lib/string.c"
#include "lib/memory.c"
#include "lib/common.c"
#include "os/os.c"
#include "meta/tokenizer.c"
#include "meta/parser.c"

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
  Parser parser = parser_create("test", source, str_len(source), &allocator);

  while (!parser_current_token_is(&parser, TOKEN_EOF) && !parser.has_error) {
    if (parser_current_token_is(&parser, TOKEN_HZ_TASK)) {
      parser_advance_token(&parser);
      if (!parser_expect_token_and_advance(&parser, TOKEN_LPAREN)) {
        parser_error(&parser, "Expected ( after HZ_TASK");
        break;
      }
      if (!parser_expect_token_and_advance(&parser, TOKEN_RPAREN)) {
        parser_error(&parser, "Expected ) after HZ_TASK");
        break;
      }
      if (!parser_expect_token_and_advance(&parser, TOKEN_TYPEDEF)) {
        parser_error(&parser, "Expected typedef after HZ_TASK");
        break;
      }
      if (!parser_expect_token_and_advance(&parser, TOKEN_STRUCT)) {
        parser_error(&parser, "Expected struct after typedef");
        break;
      }
      if (!parser_expect_token_and_advance(&parser, TOKEN_LBRACE)) {
        parser_error(&parser, "Expected { after struct keyword");
        break;
      }

      while (!parser_current_token_is(&parser, TOKEN_RBRACE)) {
        Token read_or_write = parser.current_token;
        b32 has_access_meta = false;
        b32 is_write_meta = false;
        if (parser_current_token_is(&parser, TOKEN_HZ_READ)) {
          has_access_meta = true;
        } else if (parser_current_token_is(&parser, TOKEN_HZ_WRITE)) {
          has_access_meta = true;
          is_write_meta = true;
        }

        if (!has_access_meta) {
          parser_error(&parser, "Expected HZ_READ or HZ_WRITE on struct field");
          break;
        }

        parser_advance_token(&parser);

        if (!parser_expect_token_and_advance(&parser, TOKEN_LPAREN)) {
          parser_error(&parser, "Expected (\n");
          break;
        }

        if (!parser_expect_token_and_advance(&parser, TOKEN_RPAREN)) {
          parser_error(&parser, "Expected )\n");
          break;
        }

        Token type = parser.current_token;
        if (!parser_expect_token_and_advance(&parser, TOKEN_IDENTIFIER)) {
          parser_error(&parser, "Expected type for struct field\n");
          break;
        }
        Token field = parser.current_token;
        if (!parser_expect_token_and_advance(&parser, TOKEN_IDENTIFIER)) {
          parser_error(&parser, "Expected field name after type\n");
          break;
        }
        printf("%.*s, %.*s\n", type.length, type.lexeme, field.length,
               field.lexeme);
        break;

        parser_advance_token(&parser);
      }
    }

    parser_advance_token(&parser);
  }

  if (parser.has_error) {
    printf("ERROR: %s\n", parser.error_message.value);
  } else {
    printf("SUCCESS\n");
  }

  return 0;
}

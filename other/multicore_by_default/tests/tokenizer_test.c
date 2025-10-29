#include "../meta/tokenizer.h"
#include "../lib/string.h"
#include "../lib/test.h"

void test_basic_tokens(TestContext* ctx) {
  Allocator* allocator = &ctx->allocator;

  const char *source = "struct MyStruct { } ( ) ;";
  Tokenizer tokenizer = tokenizer_create("test.c", source, str_len(source), allocator);

  Token token = tokenizer_next_token(&tokenizer);
  assert_eq(token.type, TOKEN_STRUCT);

  token = tokenizer_next_token(&tokenizer);
  assert_eq(token.type, TOKEN_IDENTIFIER);
  assert_eq(token.length, 8);

  token = tokenizer_next_token(&tokenizer);
  assert_eq(token.type, TOKEN_LBRACE);

  token = tokenizer_next_token(&tokenizer);
  assert_eq(token.type, TOKEN_RBRACE);

  token = tokenizer_next_token(&tokenizer);
  assert_eq(token.type, TOKEN_LPAREN);

  token = tokenizer_next_token(&tokenizer);
  assert_eq(token.type, TOKEN_RPAREN);

  token = tokenizer_next_token(&tokenizer);
  assert_eq(token.type, TOKEN_SEMICOLON);

  token = tokenizer_next_token(&tokenizer);
  assert_eq(token.type, TOKEN_EOF);

  tokenizer_destroy(&tokenizer);
}

void test_skip_comments(TestContext* ctx) {
  Allocator* allocator = &ctx->allocator;

  const char *source = "// this is a comment\n /* muti line comment \n with another line */struct";
  Tokenizer tokenizer = tokenizer_create("test.c", source, str_len(source), allocator);

  Token token = tokenizer_next_token(&tokenizer);
  assert_eq(token.type, TOKEN_STRUCT);
  token = tokenizer_next_token(&tokenizer);
  assert_eq(token.type, TOKEN_EOF);

  tokenizer_destroy(&tokenizer);
}

void test_typedef_keyword(TestContext* ctx) {
  Allocator* allocator = &ctx->allocator;

  const char *source = "typedef struct Point { int x; } Point;";
  Tokenizer tokenizer = tokenizer_create("test.c", source, str_len(source), allocator);

  Token token = tokenizer_next_token(&tokenizer);
  assert_eq(token.type, TOKEN_TYPEDEF);
  assert_eq(token.length, 7);

  token = tokenizer_next_token(&tokenizer);
  assert_eq(token.type, TOKEN_STRUCT);

  token = tokenizer_next_token(&tokenizer);
  assert_eq(token.type, TOKEN_IDENTIFIER);

  tokenizer_destroy(&tokenizer);
}

void test_identifier_with_parens(TestContext* ctx) {
  Allocator* allocator = &ctx->allocator;

  const char *source = "HZ_TASK() HZ_READ() struct Data { }";
  Tokenizer tokenizer = tokenizer_create("test.c", source, str_len(source), allocator);

  // First attribute: HZ_TASK()
  Token token = tokenizer_next_token(&tokenizer);
  assert_eq(token.type, TOKEN_IDENTIFIER);
  assert_eq(token.length, 7);
  assert_true(str_equal_len("HZ_TASK", 7, token.lexeme, token.length));

  token = tokenizer_next_token(&tokenizer);
  assert_eq(token.type, TOKEN_LPAREN);

  token = tokenizer_next_token(&tokenizer);
  assert_eq(token.type, TOKEN_RPAREN);

  // Second attribute: HZ_READ()
  token = tokenizer_next_token(&tokenizer);
  assert_eq(token.type, TOKEN_IDENTIFIER);
  assert_eq(token.length, 7);
  assert_true(str_equal_len("HZ_READ", 7, token.lexeme, token.length));

  token = tokenizer_next_token(&tokenizer);
  assert_eq(token.type, TOKEN_LPAREN);

  token = tokenizer_next_token(&tokenizer);
  assert_eq(token.type, TOKEN_RPAREN);

  // Struct keyword
  token = tokenizer_next_token(&tokenizer);
  assert_eq(token.type, TOKEN_STRUCT);

  tokenizer_destroy(&tokenizer);
}

void test_multiline_with_line_tracking(TestContext* ctx) {
  Allocator* allocator = &ctx->allocator;

  const char *source = "struct Point\n{\n  int x;\n  int y;\n}";
  Tokenizer tokenizer = tokenizer_create("test.c", source, str_len(source), allocator);

  Token token = tokenizer_next_token(&tokenizer);
  assert_eq(token.type, TOKEN_STRUCT);
  assert_eq(token.line, 1);

  token = tokenizer_next_token(&tokenizer);
  assert_eq(token.type, TOKEN_IDENTIFIER);
  assert_eq(token.line, 1);

  token = tokenizer_next_token(&tokenizer);
  assert_eq(token.type, TOKEN_LBRACE);
  assert_eq(token.line, 2);

  token = tokenizer_next_token(&tokenizer);
  assert_eq(token.type, TOKEN_IDENTIFIER);
  assert_eq(token.line, 3);

  token = tokenizer_next_token(&tokenizer);
  assert_eq(token.type, TOKEN_IDENTIFIER);
  assert_eq(token.line, 3);

  token = tokenizer_next_token(&tokenizer);
  assert_eq(token.type, TOKEN_SEMICOLON);
  assert_eq(token.line, 3);

  token = tokenizer_next_token(&tokenizer);
  assert_eq(token.type, TOKEN_IDENTIFIER);
  assert_eq(token.line, 4);

  tokenizer_destroy(&tokenizer);
}

void test_invalid_character(TestContext* ctx) {
  Allocator* allocator = &ctx->allocator;

  const char *source = "struct @ Data { }";
  Tokenizer tokenizer = tokenizer_create("test.c", source, str_len(source), allocator);

  Token token = tokenizer_next_token(&tokenizer);
  assert_eq(token.type, TOKEN_STRUCT);

  token = tokenizer_next_token(&tokenizer);
  assert_eq(token.type, TOKEN_INVALID);
  assert_eq(token.lexeme[0], '@');

  tokenizer_destroy(&tokenizer);
}

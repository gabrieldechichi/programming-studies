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
}

void test_skip_comments(TestContext* ctx) {
  Allocator* allocator = &ctx->allocator;
  
  const char *source = "// this is a comment\n /* muti line comment \n with another line */struct";
  Tokenizer tokenizer = tokenizer_create("test.c", source, str_len(source), allocator);

  Token token = tokenizer_next_token(&tokenizer);
  assert_eq(token.type, TOKEN_STRUCT);
  token = tokenizer_next_token(&tokenizer);
  assert_eq(token.type, TOKEN_EOF);
}

void test_hm_reflect_struct(TestContext* ctx) {
  Allocator* allocator = &ctx->allocator;
  
  const char *source = "HM_REFLECT() struct PlayerData { int health; };";
  Tokenizer tokenizer = tokenizer_create("test.c", source, str_len(source), allocator);

  Token token = tokenizer_next_token(&tokenizer);
  assert_eq(token.type, TOKEN_HM_REFLECT);
  assert_eq(token.length, 10);

  token = tokenizer_next_token(&tokenizer);
  assert_eq(token.type, TOKEN_LPAREN);

  token = tokenizer_next_token(&tokenizer);
  assert_eq(token.type, TOKEN_RPAREN);

  token = tokenizer_next_token(&tokenizer);
  assert_eq(token.type, TOKEN_STRUCT);

  token = tokenizer_next_token(&tokenizer);
  assert_eq(token.type, TOKEN_IDENTIFIER);
  assert_eq(token.length, 10);

  token = tokenizer_next_token(&tokenizer);
  assert_eq(token.type, TOKEN_LBRACE);

  token = tokenizer_next_token(&tokenizer);
  assert_eq(token.type, TOKEN_IDENTIFIER);

  token = tokenizer_next_token(&tokenizer);
  assert_eq(token.type, TOKEN_IDENTIFIER);

  token = tokenizer_next_token(&tokenizer);
  assert_eq(token.type, TOKEN_SEMICOLON);

  token = tokenizer_next_token(&tokenizer);
  assert_eq(token.type, TOKEN_RBRACE);

  token = tokenizer_next_token(&tokenizer);
  assert_eq(token.type, TOKEN_SEMICOLON);

  token = tokenizer_next_token(&tokenizer);
  assert_eq(token.type, TOKEN_EOF);
}
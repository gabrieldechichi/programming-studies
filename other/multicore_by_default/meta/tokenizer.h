#ifndef H_TOKENIZER
#define H_TOKENIZER

#include "lib/memory.h"
#include "lib/typedefs.h"

typedef enum {
  TOKEN_STRUCT,
  TOKEN_TYPEDEF,
  TOKEN_IDENTIFIER,
  TOKEN_LBRACE,     // {
  TOKEN_RBRACE,     // }
  TOKEN_LPAREN,     // (
  TOKEN_RPAREN,     // )
  TOKEN_SEMICOLON,  // ;
  TOKEN_HM_REFLECT, // HM_REFLECT() macro
  TOKEN_EOF,
  TOKEN_INVALID,
} TokenType;

typedef struct {
  TokenType type;
  const char *lexeme;
  u32 length;
  u32 line;
  u32 column;
} Token;

typedef struct {
  const char *filename;
  const char *source;
  const char *source_end;
  const char *current;
  char current_char;
  u32 line;
  u32 column;
  const char **line_starts;
  u32 line_count;
  u32 line_capacity;
  Allocator *allocator;
} Tokenizer;

Tokenizer tokenizer_create(const char *filename, const char *source, u32 source_length, Allocator *allocator);
Token tokenizer_next_token(Tokenizer *tokenizer);
b32 tokenizer_match(Tokenizer *tokenizer, TokenType expected_type);
const char *token_type_to_string(TokenType type);
const char *tokenizer_get_line_text(Tokenizer *tokenizer, u32 line_num, u32 *line_length);
void tokenizer_destroy(Tokenizer *tokenizer);

#endif
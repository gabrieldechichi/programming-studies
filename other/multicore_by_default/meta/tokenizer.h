#ifndef H_TOKENIZER
#define H_TOKENIZER

#include "lib/memory.h"
#include "lib/string.h"
#include "lib/typedefs.h"

#define TOKEN_TYPES                                                            \
  TOKEN_TYPE(TOKEN_STRUCT, "struct")                                           \
  TOKEN_TYPE(TOKEN_TYPEDEF, "typedef")                                         \
  TOKEN_TYPE(TOKEN_IDENTIFIER, "identifier")                                   \
  TOKEN_TYPE(TOKEN_LBRACE, "{")                                                \
  TOKEN_TYPE(TOKEN_RBRACE, "}")                                                \
  TOKEN_TYPE(TOKEN_LPAREN, "(")                                                \
  TOKEN_TYPE(TOKEN_RPAREN, ")")                                                \
  TOKEN_TYPE(TOKEN_LBRACKET, "[")                                              \
  TOKEN_TYPE(TOKEN_RBRACKET, "]")                                              \
  TOKEN_TYPE(TOKEN_SEMICOLON, ";")                                             \
  TOKEN_TYPE(TOKEN_ASTERISK, "*")                                              \
  TOKEN_TYPE(TOKEN_NUMBER, "number")                                           \
  TOKEN_TYPE(TOKEN_EOF, "EOF")                                                 \
  TOKEN_TYPE(TOKEN_INVALID, "INVALID")

typedef enum {
#define TOKEN_TYPE(name, str) name,
  TOKEN_TYPES
#undef TOKEN_TYPE
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
  // todo: use dyn array here
  const char **line_starts;
  u32 line_count;
  u32 line_capacity;
  Allocator *allocator;
} Tokenizer;

Tokenizer tokenizer_create(const char *filename, const char *source,
                           u32 source_length, Allocator *allocator);
Token tokenizer_next_token(Tokenizer *tokenizer);
b32 tokenizer_match(Tokenizer *tokenizer, TokenType expected_type);
const char *token_type_to_string(TokenType type);
const char *tokenizer_get_line_text(Tokenizer *tokenizer, u32 line_num,
                                    u32 *line_length);
String token_to_string(Token token, Allocator *allocator);
void tokenizer_destroy(Tokenizer *tokenizer);

#endif
#include "tokenizer.h"
#include "lib/assert.h"
#include "lib/memory.h"
#include "lib/string.h"

typedef struct {
  const char *keyword;
  TokenType type;
} KeywordEntry;

global KeywordEntry keywords[] = {
    {"struct", TOKEN_STRUCT},
    {"typedef", TOKEN_TYPEDEF},
    {"HM_REFLECT", TOKEN_HM_REFLECT},
};

#define KEYWORD_COUNT ARRAY_SIZE(keywords)

internal b32 char_is_alpha(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

internal b32 char_is_alnum(char c) {
  return char_is_alpha(c) || char_is_digit(c);
}

internal TokenType lookup_keyword(const char *str, u32 len) {
  for (u32 i = 0; i < KEYWORD_COUNT; i++) {
    b32 match = str_equal_len(keywords[i].keyword, str_len(keywords[i].keyword),
                              str, len);
    if (match) {
      return keywords[i].type;
    }
  }
  return TOKEN_IDENTIFIER;
}

internal void track_line_start(Tokenizer *tokenizer) {
  if (tokenizer->line_count >= tokenizer->line_capacity) {
    u32 new_capacity = tokenizer->line_capacity * 2;
    if (new_capacity == 0) new_capacity = 64;
    
    const char **new_line_starts = ALLOC_ARRAY(tokenizer->allocator, const char*, new_capacity);
    if (tokenizer->line_starts && tokenizer->line_count > 0) {
      for (u32 i = 0; i < tokenizer->line_count; i++) {
        new_line_starts[i] = tokenizer->line_starts[i];
      }
    }
    tokenizer->line_starts = new_line_starts;
    tokenizer->line_capacity = new_capacity;
  }
  
  if (tokenizer->line_count == 0 || tokenizer->line_count <= tokenizer->line) {
    tokenizer->line_starts[tokenizer->line_count++] = tokenizer->current;
  }
}

internal void advance_char(Tokenizer *tokenizer) {
  if (tokenizer->current < tokenizer->source_end) {
    if (char_is_line_break(tokenizer->current_char)) {
      tokenizer->line++;
      tokenizer->column = 1;
      tokenizer->current++;
      if (tokenizer->current < tokenizer->source_end) {
        track_line_start(tokenizer);
      }
    } else {
      tokenizer->column++;
      tokenizer->current++;
    }
    tokenizer->current_char = (tokenizer->current < tokenizer->source_end)
                                  ? *tokenizer->current
                                  : '\0';
  }
}

internal char tokenizer_peek_char(Tokenizer *tokenizer) {
  if (tokenizer->current + 1 < tokenizer->source_end) {
    return *(tokenizer->current + 1);
  }
  return '\0';
}

internal void tokenizer_skip_whitespace(Tokenizer *tokenizer) {
  while (char_is_space(tokenizer->current_char)) {
    advance_char(tokenizer);
  }
}

internal void skip_line_comment(Tokenizer *tokenizer) {
  while (!char_is_line_break(tokenizer->current_char) &&
         tokenizer->current_char != '\0') {
    advance_char(tokenizer);
  }
}

internal void skip_block_comment(Tokenizer *tokenizer) {
  advance_char(tokenizer);
  advance_char(tokenizer);

  while (tokenizer->current_char != '\0') {
    if (tokenizer->current_char == '*' &&
        tokenizer_peek_char(tokenizer) == '/') {
      advance_char(tokenizer);
      advance_char(tokenizer);
      break;
    }
    advance_char(tokenizer);
  }
}

internal Token make_token(TokenType type, const char *start, u32 length,
                          u32 line, u32 column) {
  return (Token){
      .type = type,
      .lexeme = start,
      .length = length,
      .line = line,
      .column = column,
  };
}

internal Token scan_identifier(Tokenizer *tokenizer) {
  const char *start = tokenizer->current;
  u32 start_line = tokenizer->line;
  u32 start_column = tokenizer->column;

  while (char_is_alnum(tokenizer->current_char)) {
    advance_char(tokenizer);
  }

  u32 length = (u32)(tokenizer->current - start);
  TokenType type = lookup_keyword(start, length);

  return make_token(type, start, length, start_line, start_column);
}

Tokenizer tokenizer_create(const char *filename, const char *source, u32 source_length, Allocator *allocator) {
  Tokenizer tokenizer = {
      .filename = filename,
      .source = source,
      .source_end = source + source_length,
      .current = source,
      .current_char = source_length > 0 ? *source : '\0',
      .line = 1,
      .column = 1,
      .line_starts = NULL,
      .line_count = 0,
      .line_capacity = 0,
      .allocator = allocator,
  };

  // Track the first line
  track_line_start(&tokenizer);

  return tokenizer;
}

Token tokenizer_next_token(Tokenizer *tokenizer) {
  tokenizer_skip_whitespace(tokenizer);

  // skip comments
  if (tokenizer->current_char == '/' && tokenizer_peek_char(tokenizer) == '/') {
    skip_line_comment(tokenizer);
    tokenizer_skip_whitespace(tokenizer);
  }

  // skip block comments
  if (tokenizer->current_char == '/' && tokenizer_peek_char(tokenizer) == '*') {
    skip_block_comment(tokenizer);
    tokenizer_skip_whitespace(tokenizer);
  }

  if (tokenizer->current_char == '\0') {
    return make_token(TOKEN_EOF, tokenizer->current, 0, tokenizer->line,
                      tokenizer->column);
  }

  u32 start_line = tokenizer->line;
  u32 start_column = tokenizer->column;
  char c = tokenizer->current_char;

  if (char_is_alpha(c)) {
    return scan_identifier(tokenizer);
  }

  advance_char(tokenizer);

  switch (c) {
  case '{':
    return make_token(TOKEN_LBRACE, tokenizer->current - 1, 1, start_line,
                      start_column);
  case '}':
    return make_token(TOKEN_RBRACE, tokenizer->current - 1, 1, start_line,
                      start_column);
  case '(':
    return make_token(TOKEN_LPAREN, tokenizer->current - 1, 1, start_line,
                      start_column);
  case ')':
    return make_token(TOKEN_RPAREN, tokenizer->current - 1, 1, start_line,
                      start_column);
  case ';':
    return make_token(TOKEN_SEMICOLON, tokenizer->current - 1, 1, start_line,
                      start_column);
  default:
    return make_token(TOKEN_INVALID, tokenizer->current - 1, 1, start_line,
                      start_column);
  }
}

b32 tokenizer_match(Tokenizer *tokenizer, TokenType expected_type) {
  Token token = tokenizer_next_token(tokenizer);
  return token.type == expected_type;
}

const char *tokenizer_get_line_text(Tokenizer *tokenizer, u32 line_num, u32 *line_length) {
  if (line_num == 0 || line_num > tokenizer->line_count) {
    if (line_length) *line_length = 0;
    return NULL;
  }
  
  u32 line_index = line_num - 1;
  const char *line_start = tokenizer->line_starts[line_index];
  const char *line_end = tokenizer->source_end;
  
  // Find the end of this line
  if (line_index + 1 < tokenizer->line_count) {
    line_end = tokenizer->line_starts[line_index + 1] - 1; // -1 to exclude newline
  } else {
    // Last line - scan to end or newline
    const char *scan = line_start;
    while (scan < tokenizer->source_end && !char_is_line_break(*scan)) {
      scan++;
    }
    line_end = scan;
  }
  
  if (line_length) {
    *line_length = (u32)(line_end - line_start);
  }
  
  return line_start;
}

void tokenizer_destroy(Tokenizer *tokenizer) {
  tokenizer->line_starts = NULL;
  tokenizer->line_count = 0;
  tokenizer->line_capacity = 0;
  tokenizer->allocator = NULL;
}

const char *token_type_to_string(TokenType type) {
  switch (type) {
  case TOKEN_STRUCT:
    return "TOKEN_STRUCT";
  case TOKEN_TYPEDEF:
    return "TOKEN_TYPEDEF";
  case TOKEN_IDENTIFIER:
    return "TOKEN_IDENTIFIER";
  case TOKEN_LBRACE:
    return "TOKEN_LBRACE";
  case TOKEN_RBRACE:
    return "TOKEN_RBRACE";
  case TOKEN_LPAREN:
    return "TOKEN_LPAREN";
  case TOKEN_RPAREN:
    return "TOKEN_RPAREN";
  case TOKEN_SEMICOLON:
    return "TOKEN_SEMICOLON";
  case TOKEN_HM_REFLECT:
    return "TOKEN_HM_REFLECT";
  case TOKEN_EOF:
    return "TOKEN_EOF";
  case TOKEN_INVALID:
    return "TOKEN_INVALID";
  default:
    return "UNKNOWN_TOKEN";
  }
}
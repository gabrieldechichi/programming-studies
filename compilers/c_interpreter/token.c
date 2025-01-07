#ifndef H_TOKEN
#define H_TOKEN

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "./utils.c"

typedef enum {
  TP_ILLEGAL = 0,
  TP_EOF,

  TP_IDENT,
  TP_INT,

  TP_STRING,

  TP_ASSIGN,
  TP_PLUS,
  TP_MINUS,
  TP_BANG,
  TP_ASTERISK,
  TP_SLASH,
  TP_LT,
  TP_GT,
  TP_LTOREQ,
  TP_GTOREQ,
  TP_EQ,
  TP_NOT_EQ,

  TP_COMMA,
  TP_SEMICOLON,

  TP_LPAREN,
  TP_RPAREN,
  TP_LBRACE,
  TP_RBRACE,
  TP_LBRACKET,
  TP_RBRACKET,

  TP_FUNC,
  TP_LET,
  TP_TRUE,
  TP_FALSE,
  TP_IF,
  TP_ELSE,
  TP_RETURN,
} TokenType;

#define TOKEN_LITERAL_MAX_LEN 3

typedef struct {
  TokenType type;
  short len_literal;
  char literal[TOKEN_LITERAL_MAX_LEN];
} Token;



Token new_token_c(TokenType type, char literal) {
  Token t;
  t.type = type;
  t.literal[0] = literal;
  t.len_literal = 1;
  return t;
}

Token new_token_s(TokenType type, char *literal) {
  Token t;
  t.type = type;
  t.len_literal = MIN(strlen(literal), TOKEN_LITERAL_MAX_LEN);
  for (int i = 0; i < t.len_literal; ++i) {
    t.literal[i] = literal[i];
  }
  return t;
}

typedef struct {
  int pos;
  int readPos;
  char c;
  string_const input;
} Lexer;

Lexer lexer_new(const char *input) {
  Lexer lexer;
  lexer.pos = lexer.readPos = lexer.c = 0;
  lexer.input = new_string_const(input);
  return lexer;
}

void lexer_read_char(Lexer *l) {
  if (l->readPos >= l->input.len) {
    l->c = 0;
  } else {
    l->c = l->input.value[l->readPos];
  }
  l->pos = l->readPos;
  l->readPos++;
}

void lexer_eat_whitespace(Lexer *l) {
  while (l->c == ' ' || l->c == '\t' || l->c == '\n' || l->c == '\r') {
    lexer_read_char(l);
  }
}

char lexer_peek_char(const Lexer *l) {
  if (l->readPos < l->input.len) {
    return l->input.value[l->readPos];
  }
  return 0;
}

Token lexer_next_token(Lexer *l) {
  Token token;
  lexer_read_char(l);
  lexer_eat_whitespace(l);

  switch (l->c) {
  case '=':
    if (lexer_peek_char(l) == '=') {
      token = new_token_s(TP_EQ, "==");
      lexer_read_char(l);
    } else {
      token = new_token_c(TP_ASSIGN, l->c);
    }
    break;
  case '!':
    if (lexer_peek_char(l) == '=') {
      token = new_token_s(TP_NOT_EQ, "!=");
      lexer_read_char(l);
    } else {
      token = new_token_c(TP_BANG, l->c);
    }
    break;
  case '<':
    if (lexer_peek_char(l) == '=') {
      token = new_token_s(TP_LTOREQ, "<=");
      lexer_read_char(l);
    } else {
      token = new_token_c(TP_LT, l->c);
    }
    break;
  case '>':
    if (lexer_peek_char(l) == '=') {
      token = new_token_s(TP_GTOREQ, ">=");
      lexer_read_char(l);
    } else {
      token = new_token_c(TP_GT, l->c);
    }
    break;
  case '+':
    token = new_token_c(TP_PLUS, l->c);
    break;
  case '-':
    token = new_token_c(TP_MINUS, l->c);
    break;
  case '*':
    token = new_token_c(TP_ASTERISK, l->c);
    break;
  case '/':
    token = new_token_c(TP_SLASH, l->c);
    break;
  case ',':
    token = new_token_c(TP_COMMA, l->c);
    break;
  case ';':
    token = new_token_c(TP_SEMICOLON, l->c);
    break;
  case '(':
    token = new_token_c(TP_LPAREN, l->c);
    break;
  case ')':
    token = new_token_c(TP_RPAREN, l->c);
    break;
  case '{':
    token = new_token_c(TP_LBRACE, l->c);
    break;
  case '}':
    token = new_token_c(TP_RBRACE, l->c);
    break;
  case '[':
    token = new_token_c(TP_LBRACKET, l->c);
    break;
  case ']':
    token = new_token_c(TP_RBRACKET, l->c);
    break;
  case 0:
    token.type = TP_EOF;
    token.len_literal = 0;
    token.literal[0] = '\0';
    break;
  }
  return token;
}

const char *token_type_to_str(TokenType type) {
  switch (type) {
  case TP_ILLEGAL:
    return "ILLEGAL";
  case TP_EOF:
    return "EOF";
  case TP_IDENT:
    return "IDENT";
  case TP_INT:
    return "INT";
  case TP_STRING:
    return "STRING";
  case TP_ASSIGN:
    return "=";
  case TP_PLUS:
    return "+";
  case TP_MINUS:
    return "-";
  case TP_BANG:
    return "!";
  case TP_ASTERISK:
    return "*";
  case TP_SLASH:
    return "/";
  case TP_LT:
    return "<";
  case TP_GT:
    return ">";
  case TP_LTOREQ:
    return "<=";
  case TP_GTOREQ:
    return ">=";
  case TP_EQ:
    return "==";
  case TP_NOT_EQ:
    return "!=";
  case TP_COMMA:
    return ",";
  case TP_SEMICOLON:
    return ";";
  case TP_LPAREN:
    return "(";
  case TP_RPAREN:
    return ")";
  case TP_LBRACE:
    return "{";
  case TP_RBRACE:
    return "}";
  case TP_LBRACKET:
    return "[";
  case TP_RBRACKET:
    return "]";
  case TP_FUNC:
    return "FUNC";
  case TP_LET:
    return "LET";
  case TP_TRUE:
    return "TRUE";
  case TP_FALSE:
    return "FALSE";
  case TP_IF:
    return "IF";
  case TP_ELSE:
    return "ELSE";
  case TP_RETURN:
    return "RETURN";
  default:
    return "UNKNOWN";
  }
}
#endif

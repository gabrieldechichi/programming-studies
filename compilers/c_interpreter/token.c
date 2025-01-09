#ifndef H_TOKEN
#define H_TOKEN

#include "./utils.c"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TOKEN_KINDS \
  TOKEN_KIND(TP_ILLEGAL,"Illegal"), \
  TOKEN_KIND(TP_EOF,"Eof"), \
  TOKEN_KIND(TP_IDENT,"Ident"), \
  TOKEN_KIND(TP_INT,"Int"), \
  TOKEN_KIND(TP_STRING,"String"), \
\
  TOKEN_KIND(TP_ASSIGN,"Assign"), \
  TOKEN_KIND(TP_PLUS,"Plus"), \
  TOKEN_KIND(TP_MINUS,"Minus"), \
  TOKEN_KIND(TP_BANG,"Bang"), \
  TOKEN_KIND(TP_ASTERISK,"Asterisk"), \
  TOKEN_KIND(TP_SLASH,"Slash"), \
  TOKEN_KIND(TP_LT,"Lt"), \
  TOKEN_KIND(TP_GT,"Gt"), \
  TOKEN_KIND(TP_LTOREQ,"Ltoreq"), \
  TOKEN_KIND(TP_GTOREQ,"Gtoreq"), \
  TOKEN_KIND(TP_EQ,"Eq"), \
  TOKEN_KIND(TP_NOT_EQ,"Not_eq"), \
\
  TOKEN_KIND(TP_COMMA,"Comma"), \
  TOKEN_KIND(TP_SEMICOLON,"Semicolon"), \
\
  TOKEN_KIND(TP_LPAREN,"Lparen"), \
  TOKEN_KIND(TP_RPAREN,"Rparen"), \
  TOKEN_KIND(TP_LBRACE,"Lbrace"), \
  TOKEN_KIND(TP_RBRACE,"Rbrace"), \
  TOKEN_KIND(TP_LBRACKET,"Lbracket"), \
  TOKEN_KIND(TP_RBRACKET,"Rbracket"), \
\
  TOKEN_KIND(TP_FUNC,"Func"), \
  TOKEN_KIND(TP_LET,"Let"), \
  TOKEN_KIND(TP_TRUE,"True"), \
  TOKEN_KIND(TP_FALSE,"False"), \
  TOKEN_KIND(TP_IF,"If"), \
  TOKEN_KIND(TP_ELSE,"Else"), \
  TOKEN_KIND(TP_RETURN,"Return"), \

typedef enum {
#define TOKEN_KIND(e,s) e
    TOKEN_KINDS
#undef TOKEN_KIND
} TokenType;

static const char* g_token_names[] = {
#define TOKEN_KIND(e,s) s
    TOKEN_KINDS
#undef TOKEN_KIND
};

typedef struct {
  TokenType type;
  string_const literal;
} Token;

Token new_token_c(TokenType type, char literal) {
  Token t = {0};
  t.type = type;
  t.literal.value = &literal;
  t.literal.len = 1;
  return t;
}

Token new_token_s(TokenType type, char *literal) {
  Token t = {0};
  t.type = type;
  t.literal = new_string_const(literal);
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

void lexer_go_back(Lexer *l) {
  if (l->pos > 0) {
    l->readPos = l->pos;
    l->pos--;
  }
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

static bool is_identifier(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static string_const read_identifier(Lexer *l) {
  int start = l->pos;
  while (is_identifier(l->c)) {
    lexer_read_char(l);
  }
  lexer_go_back(l);

  return string_const_from_slice(l->input.value, start, l->pos + 1);
}

static bool is_digit(char c) { return c >= '0' && c <= '9'; }

static string_const read_digit(Lexer *l) {
  int start = l->pos;
  while (is_digit(l->c)) {
    lexer_read_char(l);
  }
  lexer_go_back(l);
  return string_const_from_slice(l->input.value, start, l->pos + 1);
}

static string_const read_string(Lexer *l) {
  lexer_read_char(l);
  int start = l->pos;
  while (l-> c != '"') {
    lexer_read_char(l);
  }

  return string_const_from_slice(l->input.value, start, l->pos);
}

TokenType identifier_to_token_type(string_const s) {
  if (string_const_eq_s(s, "let")) {
    return TP_LET;
  } else if (string_const_eq_s(s, "fn")) {
    return TP_FUNC;
  } else if (string_const_eq_s(s, "true")) {
    return TP_TRUE;
  } else if (string_const_eq_s(s, "false")) {
    return TP_FALSE;
  } else if (string_const_eq_s(s, "if")) {
    return TP_IF;
  } else if (string_const_eq_s(s, "else")) {
    return TP_ELSE;
  } else if (string_const_eq_s(s, "return")) {
    return TP_RETURN;
  }
  return TP_IDENT;
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
  case '"':
    token.type = TP_STRING;
    token.literal = read_string(l);
    break;
  case 0:
    token.type = TP_EOF;
    token.literal = new_string_const("");
    break;
  default: {
    if (is_identifier(l->c)) {
      string_const literal = read_identifier(l);
      token.type = identifier_to_token_type(literal);
      token.literal = literal;
    } else if (is_digit(l->c)) {
      token.type = TP_INT;
      token.literal = read_digit(l);
    } else {
      token.type = TP_ILLEGAL;
      token.literal = new_string_const("");
    }
    break;
  }
  }
  return token;
}

const char *token_type_to_str(TokenType type) {
    return g_token_names[(int)type];
}
#endif

#ifndef H_TOKEN
#define H_TOKEN

#include "./utils.c"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

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

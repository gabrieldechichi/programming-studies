#ifndef H_LEXER
#define H_LEXER

#include "string.c"
#include "token.c"

typedef struct {
  int pos;
  int readPos;
  char c;
  StringSlice input;
} Lexer;

Lexer lexer_new(const char *input) {
  Lexer lexer;
  lexer.pos = lexer.readPos = lexer.c = 0;
  lexer.input = strslice_from_char_str(input);
  return lexer;
}

void lexer_read_char(Lexer *l) {
  if (l->readPos >= l->input.len) {
    l->c = 0;
    return;
  } else {
    l->c = l->input.value[l->readPos];
  }
  l->pos = l->readPos;
  l->readPos++;
}

void lexer_go_back(Lexer *l) {
  if (l->pos >= 0) {
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

static StringSlice read_identifier(Lexer *l) {
  int start = l->pos;
  while (is_identifier(l->c)) {
    lexer_read_char(l);
  }

  if (l->c) {
    lexer_go_back(l);
  }

  int end = l->pos;
  if (start >= end) {
    return strslice_new_len(&l->input.value[start], 1);
  }

  return strslice_new(l->input.value, start, end);
}

static bool is_digit(char c) { return c >= '0' && c <= '9'; }

static StringSlice read_digit(Lexer *l) {
  int start = l->pos;
  while (is_digit(l->c)) {
    lexer_read_char(l);
  }
  if (l->c) {
    lexer_go_back(l);
  }

  return strslice_new(l->input.value, start, l->pos);
}

static StringSlice read_string(Lexer *l) {
  lexer_read_char(l);
  int start = l->pos;
  while (l->c != '"') {
    lexer_read_char(l);
  }

  return strslice_new(l->input.value, start, l->pos - 1);
}

TokenType identifier_to_token_type(StringSlice s) {
  if (strslice_eq_s(s, "let")) {
    return TP_LET;
  } else if (strslice_eq_s(s, "fn")) {
    return TP_FUNC;
  } else if (strslice_eq_s(s, "true")) {
    return TP_TRUE;
  } else if (strslice_eq_s(s, "false")) {
    return TP_FALSE;
  } else if (strslice_eq_s(s, "if")) {
    return TP_IF;
  } else if (strslice_eq_s(s, "else")) {
    return TP_ELSE;
  } else if (strslice_eq_s(s, "return")) {
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
      token = new_token_from_c_str(TP_EQ, "==");
      lexer_read_char(l);
    } else {
      token = new_token_from_char(TP_ASSIGN, &l->input.value[l->pos]);
    }
    break;
  case '!':
    if (lexer_peek_char(l) == '=') {
      token = new_token_from_c_str(TP_NOT_EQ, "!=");
      lexer_read_char(l);
    } else {
      token = new_token_from_char(TP_BANG, &l->input.value[l->pos]);
    }
    break;
  case '<':
    if (lexer_peek_char(l) == '=') {
      token = new_token_from_c_str(TP_LTOREQ, "<=");
      lexer_read_char(l);
    } else {
      token = new_token_from_char(TP_LT, &l->input.value[l->pos]);
    }
    break;
  case '>':
    if (lexer_peek_char(l) == '=') {
      token = new_token_from_c_str(TP_GTOREQ, ">=");
      lexer_read_char(l);
    } else {
      token = new_token_from_char(TP_GT, &l->input.value[l->pos]);
    }
    break;
  case '+':
    token = new_token_from_char(TP_PLUS, &l->input.value[l->pos]);
    break;
  case '-':
    token = new_token_from_char(TP_MINUS, &l->input.value[l->pos]);
    break;
  case '*':
    token = new_token_from_char(TP_ASTERISK, &l->input.value[l->pos]);
    break;
  case '/':
    token = new_token_from_char(TP_SLASH, &l->input.value[l->pos]);
    break;
  case ',':
    token = new_token_from_char(TP_COMMA, &l->input.value[l->pos]);
    break;
  case ';':
    token = new_token_from_char(TP_SEMICOLON, &l->input.value[l->pos]);
    break;
  case '(':
    token = new_token_from_char(TP_LPAREN, &l->input.value[l->pos]);
    break;
  case ')':
    token = new_token_from_char(TP_RPAREN, &l->input.value[l->pos]);
    break;
  case '{':
    token = new_token_from_char(TP_LBRACE, &l->input.value[l->pos]);
    break;
  case '}':
    token = new_token_from_char(TP_RBRACE, &l->input.value[l->pos]);
    break;
  case '[':
    token = new_token_from_char(TP_LBRACKET, &l->input.value[l->pos]);
    break;
  case ']':
    token = new_token_from_char(TP_RBRACKET, &l->input.value[l->pos]);
    break;
  case '"':
    token.type = TP_STRING;
    token.literal = read_string(l);
    break;
  case 0:
    token.type = TP_EOF;
    token.literal = strslice_from_char_str("");
    break;
  default: {
    if (is_identifier(l->c)) {
      StringSlice literal = read_identifier(l);
      token.type = identifier_to_token_type(literal);
      token.literal = literal;
    } else if (is_digit(l->c)) {
      token.type = TP_INT;
      token.literal = read_digit(l);
    } else {
      token.type = TP_ILLEGAL;
      token.literal = strslice_from_char_str("");
    }
    break;
  }
  }
  return token;
}
#endif /* ifndef H_LEXER */

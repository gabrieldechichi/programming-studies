#ifndef H_TOKEN
#define H_TOKEN

#include "./string.c"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TOKEN_KINDS                                                            \
  TOKEN_KIND(TP_ILLEGAL, "Illegal"), TOKEN_KIND(TP_EOF, "Eof"),                \
      TOKEN_KIND(TP_IDENT, "Ident"), TOKEN_KIND(TP_INT, "Int"),                \
      TOKEN_KIND(TP_STRING, "String"),                                         \
                                                                               \
      TOKEN_KIND(TP_ASSIGN, "Assign"), TOKEN_KIND(TP_PLUS, "Plus"),            \
      TOKEN_KIND(TP_MINUS, "Minus"), TOKEN_KIND(TP_BANG, "Bang"),              \
      TOKEN_KIND(TP_ASTERISK, "Asterisk"), TOKEN_KIND(TP_SLASH, "Slash"),      \
      TOKEN_KIND(TP_LT, "Lt"), TOKEN_KIND(TP_GT, "Gt"),                        \
      TOKEN_KIND(TP_LTOREQ, "Ltoreq"), TOKEN_KIND(TP_GTOREQ, "Gtoreq"),        \
      TOKEN_KIND(TP_EQ, "Eq"), TOKEN_KIND(TP_NOT_EQ, "Not_eq"),                \
                                                                               \
      TOKEN_KIND(TP_COMMA, "Comma"), TOKEN_KIND(TP_SEMICOLON, "Semicolon"),    \
                                                                               \
      TOKEN_KIND(TP_LPAREN, "Lparen"), TOKEN_KIND(TP_RPAREN, "Rparen"),        \
      TOKEN_KIND(TP_LBRACE, "Lbrace"), TOKEN_KIND(TP_RBRACE, "Rbrace"),        \
      TOKEN_KIND(TP_LBRACKET, "Lbracket"),                                     \
      TOKEN_KIND(TP_RBRACKET, "Rbracket"),                                     \
                                                                               \
      TOKEN_KIND(TP_FUNC, "Func"), TOKEN_KIND(TP_LET, "Let"),                  \
      TOKEN_KIND(TP_TRUE, "True"), TOKEN_KIND(TP_FALSE, "False"),              \
      TOKEN_KIND(TP_IF, "If"), TOKEN_KIND(TP_ELSE, "Else"),                    \
      TOKEN_KIND(TP_RETURN, "Return"),

typedef enum {
#define TOKEN_KIND(e, s) e
  TOKEN_KINDS
#undef TOKEN_KIND
} TokenType;

static const char *g_token_names[] = {
#define TOKEN_KIND(e, s) s
    TOKEN_KINDS
#undef TOKEN_KIND
};

typedef enum {
  P_LOWEST = 0,
  P_EQUALS,      // ==
  P_LESSGREATER, // > or <
  P_SUM,         // +
  P_PRODUCT,     // *
  P_PREFIX,      // -X or !X
  P_CALL,        // myFunction(X)
  P_INDEXING,    // [
} TokenPrecedence;

typedef struct {
  TokenType type;
  StringSlice literal;
} Token;

Token new_token(TokenType type, const char *literal, int len) {
  Token t = {0};
  t.type = type;
  t.literal.value = literal;
  t.literal.len = len;
  return t;
}

Token new_token_from_char(TokenType type, const char *literal) {
  return new_token(type, literal, 1);
}

Token new_token_from_c_str(TokenType type, const char *literal) {
  return new_token(type, literal, strlen(literal));
}

const char *token_type_to_str(TokenType type) {
  return g_token_names[(int)type];
}

TokenPrecedence get_token_precedence(TokenType type) {
  switch (type) {
  case TP_EQ:
  case TP_NOT_EQ:
    return P_EQUALS;
  case TP_GT:
  case TP_LT:
  case TP_GTOREQ:
  case TP_LTOREQ:
    return P_LESSGREATER;
  case TP_PLUS:
  case TP_MINUS:
    return P_SUM;
  case TP_ASTERISK:
  case TP_SLASH:
    return P_PRODUCT;
  case TP_LBRACKET:
    return P_INDEXING;
  case TP_LPAREN:
    return P_CALL;
  default:
    return P_LOWEST;
  }
}

#undef TOKEN_KINDS
#endif

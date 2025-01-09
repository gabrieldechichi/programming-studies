#ifndef H_PARSER
#define H_PARSER

#include "ast.c"
#include "lexer.c"
#include "token.c"

typedef struct {
  Lexer *lexer;
  Token curToken;
  Token peekToken;
} Parser;

void parser_next_token(Parser *p) {
    Ast a;
  p->curToken = p->peekToken;
  p->peekToken = lexer_next_token(p->lexer);
}

Parser parser_new(Lexer *lexer) {
  Parser p = {0};
  p.lexer = lexer;
  parser_next_token(&p);
  parser_next_token(&p);
  return p;
}

#endif

#ifndef H_PARSER
#define H_PARSER

#include "ast.c"
#include "global.c"
#include "lexer.c"
#include "token.c"
#include "utils.c"
#include "vendor/stb/stb_ds.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct {
  Lexer *lexer;
  Token curToken;
  Token peekToken;
} Parser;

typedef Ast (*ParsePrefixExpressionFn)(Parser *p);
internal Ast parse_expression(Parser *p);

internal void next_token(Parser *p) {
  p->curToken = p->peekToken;
  p->peekToken = lexer_next_token(p->lexer);
}

Parser parser_new(Lexer *lexer) {
  Parser p = {0};
  p.lexer = lexer;
  next_token(&p);
  next_token(&p);
  return p;
}

internal bool cur_token_is(const Parser *p, TokenType tokType) {
  return p->curToken.type == tokType;
}

internal bool peek_token_is(const Parser *p, TokenType tokType) {
  return p->peekToken.type == tokType;
}

internal Ast parse_integer_literal(Parser *p) {
  Ast statement = {0};
  statement.kind = Ast_Integer;
  statement.Integer.token = p->curToken;

  if (parse_int(p->curToken.literal.value, p->curToken.literal.len,
                &statement.Integer.value)) {
    // todo: error
  }

  return statement;
}

internal Ast parse_boolean_literal(Parser *p) {
  Ast statement = {0};
  statement.kind = Ast_Boolean;
  statement.Boolean.token = p->curToken;
  statement.Boolean.value = p->curToken.type == TP_TRUE;

  return statement;
}

internal Ast parse_string_literal(Parser *p) {
  Ast statement = {0};
  statement.kind = Ast_String;
  statement.String.token = p->curToken;
  statement.String.value = p->curToken.literal;

  return statement;
}

internal Ast parse_prefix_operator(Parser *p) {
  Ast statement = {0};
  statement.kind = Ast_PrefixOperator;
  statement.PrefixOperator.token = p->curToken;
  statement.PrefixOperator.operator= p->curToken.literal;

  next_token(p);
  statement.PrefixOperator.right =
      arena_alloc(&global_ctx()->arena_alloc, sizeof(Ast));
  *statement.PrefixOperator.right = parse_expression(p);

  return statement;
}

internal ParsePrefixExpressionFn get_prefix_parse_fn(TokenType tokType) {
  switch (tokType) {
  case TP_INT:
    return parse_integer_literal;
    break;
  case TP_TRUE:
  case TP_FALSE:
    return parse_boolean_literal;
    break;
  case TP_STRING:
    return parse_string_literal;
    break;
  case TP_BANG:
  case TP_MINUS:
    return parse_prefix_operator;
    break;
  default:
    return NULL;
  }
}

internal Ast parse_expression(Parser *p) {
  Ast statement = {0};
  statement.kind = Ast_Invalid;

  ParsePrefixExpressionFn prefix_parse_fn =
      get_prefix_parse_fn(p->curToken.type);
  if (prefix_parse_fn == NULL) {
    // todo: error
    return statement;
  }

  statement = prefix_parse_fn(p);
  while (!cur_token_is(p, TP_SEMICOLON) && !cur_token_is(p, TP_EOF)) {
    next_token(p);
  }
  return statement;
}

internal Ast parse_return_statement(Parser *p) {
  Ast ret_statement = {0};
  ret_statement.kind = Ast_Return;
  ret_statement.Return.token = p->curToken;

  next_token(p);
  ret_statement.Return.expression =
      arena_alloc(&global_ctx()->arena_alloc, sizeof(Ast));
  *ret_statement.Return.expression = parse_expression(p);

  if (peek_token_is(p, TP_SEMICOLON)) {
    next_token(p);
  }
  return ret_statement;
}

internal Ast parse_let_statement(Parser *p) {
  Ast let_statement = {0};
  let_statement.kind = Ast_Let;
  let_statement.Let.token = p->curToken;

  if (!peek_token_is(p, TP_IDENT)) {
    // todo: error + line number
    return let_statement;
  }

  next_token(p);
  let_statement.Let.identifier.token = p->curToken;
  let_statement.Let.identifier.value = p->curToken.literal;

  if (!peek_token_is(p, TP_ASSIGN)) {
    // todo: error + line number
    return let_statement;
  }
  next_token(p);
  next_token(p);

  let_statement.Let.expression = (Ast *)malloc(sizeof(Ast));
  *let_statement.Let.expression = parse_expression(p);

  return let_statement;
}

internal Ast parse_statement(Parser *p) {
  switch (p->curToken.type) {
  case TP_LET:
    return parse_let_statement(p);
    break;
  case TP_RETURN:
    return parse_return_statement(p);
    break;
  default: {
    return parse_expression(p);
  }
  }
}

AstProgram parse_program(Parser *parser) {
  AstProgram program = {0};
  program.statements = NULL;

  while (!cur_token_is(parser, TP_EOF)) {
    Ast statement = parse_statement(parser);
    arrpush(program.statements, statement);
    next_token(parser);
  }
  return program;
}

#endif

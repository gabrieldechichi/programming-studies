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
typedef Ast (*ParseInfixExpressionFn)(Parser *p, Ast left);

internal Ast parse_expression(Parser *p, TokenPrecedence precedence);

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

internal TokenPrecedence parser_peek_precedence(Parser *p) {
  return get_token_precedence(p->peekToken.type);
}

internal TokenPrecedence parser_current_precedence(Parser *p) {
  return get_token_precedence(p->curToken.type);
}

internal Ast parse_identifier(Parser *p) {
  Ast statement = {0};
  statement.kind = Ast_Identifier;
  statement.Identifier.token = p->curToken;
  statement.Identifier.value = p->curToken.literal;

  return statement;
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
  *statement.PrefixOperator.right = parse_expression(p, P_PREFIX);

  return statement;
}

Ast parse_expr_infix(Parser *p, Ast left) {
  Ast infix_expr = {0};
  infix_expr.kind = Ast_InfixExpression;
  infix_expr.InfixExpression.token = p->curToken;
  infix_expr.InfixExpression.left = GD_ARENA_ALLOC_T(Ast);
  *infix_expr.InfixExpression.left = left;
  next_token(p);
  infix_expr.InfixExpression.right = GD_ARENA_ALLOC_T(Ast);
  TokenPrecedence precedence = parser_current_precedence(p);
  *infix_expr.InfixExpression.right = parse_expression(p, precedence);
  next_token(p);
  return infix_expr;
}

internal ParseInfixExpressionFn get_infix_parse_fn(TokenType tokType) {
  switch (tokType) {
  case TP_EQ:
  case TP_NOT_EQ:
  case TP_GT:
  case TP_LT:
  case TP_LTOREQ:
  case TP_GTOREQ:
  case TP_PLUS:
  case TP_MINUS:
  case TP_ASTERISK:
  case TP_SLASH:
    return parse_expr_infix;
  default:
    return NULL;
  }
}

internal ParsePrefixExpressionFn get_prefix_parse_fn(TokenType tokType) {
  switch (tokType) {
  case TP_IDENT:
    return parse_identifier;
  case TP_INT:
    return parse_integer_literal;
  case TP_TRUE:
  case TP_FALSE:
    return parse_boolean_literal;
  case TP_STRING:
    return parse_string_literal;
  case TP_BANG:
  case TP_MINUS:
    return parse_prefix_operator;
  default:
    return NULL;
  }
}

internal Ast parse_expression(Parser *p, TokenPrecedence precedence) {
  Ast leftExpr = {0};
  leftExpr.kind = Ast_Invalid;

  ParsePrefixExpressionFn prefix_parse_fn =
      get_prefix_parse_fn(p->curToken.type);
  if (prefix_parse_fn == NULL) {
    // todo: error
    return leftExpr;
  }

  leftExpr = prefix_parse_fn(p);
  while (!cur_token_is(p, TP_SEMICOLON) &&
         precedence < parser_peek_precedence(p)) {
    ParseInfixExpressionFn infix_parse_fn =
        get_infix_parse_fn(p->peekToken.type);
    if (!infix_parse_fn) {
      // todo error
      return leftExpr;
    }
    next_token(p);
    leftExpr = infix_parse_fn(p, leftExpr);
  }

  return leftExpr;
}

internal Ast parse_return_statement(Parser *p) {
  Ast ret_statement = {0};
  ret_statement.kind = Ast_Return;
  ret_statement.Return.token = p->curToken;

  next_token(p);
  ret_statement.Return.expression = GD_ARENA_ALLOC_T(Ast);
  *ret_statement.Return.expression = parse_expression(p, P_LOWEST);

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
  *let_statement.Let.expression = parse_expression(p, P_LOWEST);

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
    return parse_expression(p, P_LOWEST);
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

    while (cur_token_is(parser, TP_SEMICOLON)) {
      next_token(parser);
    }
  }
  return program;
}

#endif

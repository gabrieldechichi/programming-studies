#ifndef H_AST
#define H_AST

#include "global.c"
#include "macros.h"
#include "str_builder.c"
#include "string.c"
#include "token.c"
#include <stdio.h>
#include <string.h>

typedef struct Ast Ast;

typedef struct {
  Token token;
  StringSlice value;
} Identifier;

#define AST_KINDS                                                              \
  AST_KIND(                                                                    \
      Let, "Let", struct {                                                     \
        Token token;                                                           \
        Identifier identifier;                                                 \
        Ast *expression;                                                       \
      })                                                                       \
  AST_KIND(                                                                    \
      Return, "Return", struct {                                               \
        Token token;                                                           \
        Ast *expression;                                                       \
      })                                                                       \
  AST_KIND(                                                                    \
      Integer, "Integer", struct {                                             \
        Token token;                                                           \
        int value;                                                             \
      })                                                                       \
  AST_KIND(                                                                    \
      Boolean, "Boolean", struct {                                             \
        Token token;                                                           \
        bool value;                                                            \
      })                                                                       \
  AST_KIND(                                                                    \
      String, "String", struct {                                               \
        Token token;                                                           \
        StringSlice value;                                                     \
      })                                                                       \
  AST_KIND(                                                                    \
      PrefixOperator, "Prefix Operator", struct {                              \
        Token token;                                                           \
        StringSlice operator;                                                  \
        Ast *right;                                                            \
      })                                                                       \
  AST_KIND(                                                                    \
      Identifier, "Identifier", struct {                                       \
        Token token;                                                           \
        StringSlice value;                                                     \
      })                                                                       \
  AST_KIND(                                                                    \
      InfixExpression, "Infix Expression", struct {                            \
        Token token;                                                           \
        Ast *left;                                                             \
        Ast *right;                                                            \
        TokenOperation operator;                                               \
      })                                                                       \
  AST_KIND(                                                                    \
      FunctionCallExpression, "Function Call Expression", struct {             \
        Token token;                                                           \
        Ast *functionName;                                                     \
        Ast *arguments;                                                        \
        size_t arguments_len;                                                  \
      })                                                                       \
  AST_KIND(                                                                    \
      BlockStatement, "Block Statement", struct {                              \
        Ast *statements;                                                       \
        size_t statement_len;                                                  \
      })                                                                       \
  AST_KIND(                                                                    \
      IfExpression, "If Expression", struct {                                  \
        Token token;                                                           \
        Ast *condition;                                                        \
        Ast *consequence;                                                      \
        Ast *alternative;                                                      \
      })

// Ast enums
typedef enum {
  Ast_Invalid,
#define AST_KIND(kind, ...) GD_JOIN2(Ast_, kind),
  AST_KINDS
#undef AST_KIND
      Ast_Count
} AstKind;

// typedef Ast structs
#define AST_KIND(kind, name, ...) typedef __VA_ARGS__ GD_JOIN2(Ast, kind);
AST_KINDS
#undef AST_KIND

// Ast union
struct Ast {
  AstKind kind;
  union {
#define AST_KIND(kind, name, ...) GD_JOIN2(Ast, kind) kind;
    AST_KINDS
#undef AST_KIND
  };
};

typedef struct {
  Ast *statements;
} AstProgram;

StringSlice expression_to_string(const Ast *ast) {
  StringSlice ret_val = {0};

  DEBUG_ASSERT(ast);
  if (!ast) {
    return ret_val;
  }

  StringBuilder *sb = GD_TEMP_ALLOC_T(StringBuilder);
  sb_init(sb, gd_temp_realloc);

  switch (ast->kind) {
  case Ast_Invalid:
    break;
  case Ast_Let:
    break;
  case Ast_Return: {
    StringSlice right_string = expression_to_string(ast->Return.expression);
    sb_append(sb, "return ");
    sb_append_slice(sb, right_string);
    sb_append(sb, ";");
    break;
  }
  case Ast_Integer: {
    char temp_s[16];
    sprintf(temp_s, "%d", ast->Integer.value);
    sb_append(sb, temp_s);
    break;
  }
  case Ast_Boolean:
    sb_append(sb, ast->Boolean.value ? "true" : "false");
    break;
  case Ast_String:
    break;
  case Ast_PrefixOperator: {
    sb_append(sb, "(");
    sb_append_slice(sb, ast->PrefixOperator.operator);
    sb_append_slice(sb, expression_to_string(ast->PrefixOperator.right));
    sb_append(sb, ")");
    break;
  }
  case Ast_Identifier:
    sb_append_slice(sb, ast->Identifier.value);
    break;
  case Ast_Count:
    break;
  case Ast_InfixExpression: {
    sb_append(sb, "(");
    StringSlice left = expression_to_string(ast->InfixExpression.left);
    StringSlice right = expression_to_string(ast->InfixExpression.right);
    StringSlice operator= ast->InfixExpression.token.literal;
    sb_append_slice(sb, left);
    sb_append(sb, " ");
    sb_append_slice(sb, operator);
    sb_append(sb, " ");
    sb_append_slice(sb, right);
    sb_append(sb, ")");
    break;
  }
  case Ast_FunctionCallExpression: {
    StringSlice funcName =
        expression_to_string(ast->FunctionCallExpression.functionName);
    sb_append_slice(sb, funcName);
    sb_append(sb, "(");
    for (size_t i = 0; i < ast->FunctionCallExpression.arguments_len; i++) {
      Ast arg_expr = ast->FunctionCallExpression.arguments[i];
      StringSlice arg = expression_to_string(&arg_expr);
      sb_append_slice(sb, arg);
      if (i < ast->FunctionCallExpression.arguments_len - 1) {
        sb_append(sb, ", ");
      }
    }
    sb_append(sb, ")");
    break;
  }
  case Ast_BlockStatement: {
    sb_append(sb, "{");
    if (ast->BlockStatement.statement_len > 1) {
      sb_append(sb, "\n");
    }
    for (size_t i = 0; i < ast->BlockStatement.statement_len; i++) {
      StringSlice statement =
          expression_to_string(&ast->BlockStatement.statements[i]);
      sb_append_slice(sb, statement);
      if (i < ast->BlockStatement.statement_len - 1) {
        sb_append(sb, "\n");
      }
    }
    if (ast->BlockStatement.statement_len > 1) {
      sb_append(sb, "\n");
    }
    sb_append(sb, "}\n");
    break;
  }
  case Ast_IfExpression: {
    sb_append(sb, "if (");
    StringSlice condition = expression_to_string(ast->IfExpression.condition);
    sb_append_slice(sb, condition);
    sb_append(sb, ") ");
    StringSlice consequence =
        expression_to_string(ast->IfExpression.consequence);
    sb_append_slice(sb, consequence);

    if (ast->IfExpression.alternative) {
      sb_append(sb, "else ");
      StringSlice alternative =
          expression_to_string(ast->IfExpression.alternative);
      sb_append_slice(sb, alternative);
    }
    break;
  }
  }

  ret_val.value = sb->str;
  ret_val.len = sb->length;
  return ret_val;
}
#endif

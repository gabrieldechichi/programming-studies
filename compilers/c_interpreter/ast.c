#ifndef H_AST
#define H_AST

#include "token.c"
#include "utils.c"
#include <string.h>

typedef struct Ast Ast;

typedef struct {
  Token token;
  string_const value;
} Identifier;

#define AST_KINDS                                                              \
  AST_KIND(                                                                    \
      Let, "Let", struct {                                                     \
        Token token;                                                           \
        Identifier identifier;                                                 \
        Ast *expression;                                                \
      })                                                                       \
  AST_KIND(                                                                    \
      Integer, "Integer", struct {                                             \
        Token token;                                                           \
        int value;                                                             \
      })                                                                       \
  AST_KIND(                                                                    \
      Boolean, "Boolean", struct {                                             \
        Token token;                                                           \
        bool value;                                                             \
      })                                                                       \
  AST_KIND(                                                                    \
      Identifier, "Identifier", struct {                                       \
        Token token;                                                           \
        string_const value;                                                    \
      })                                                                       \
  AST_KIND(Return, "Return", struct { Token token; })

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

#endif

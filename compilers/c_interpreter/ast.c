#ifndef H_AST
#define H_AST

#include "token.c"
#include "utils.c"

#define AST_KINDS                                                              \
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
typedef struct {
  union {
#define AST_KIND(kind, name, ...) GD_JOIN2(Ast, kind) kind;
    AST_KINDS
#undef AST_KIND
  };

} Ast;

#endif

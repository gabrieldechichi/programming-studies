#include "ast.c"
#include "lexer.c"
#include "parser.c"
#include "utils.c"
#include "vendor/stb/stb_ds.h"
#include <stdio.h>

AstProgram parse_input(const char *input) {
  Lexer lexer = lexer_new(input);
  Parser parser = parser_new(&lexer);
  AstProgram ast = parse_program(&parser);
  return ast;
}

void test_let_statements() {
  const char *input = "\
let x = 5;\
let y = 10;\
let foobar = 838383;\
";

  struct {
    char *expectedIdentifier;
  } tests[] = {{"x"}, {"y"}, {"foobar"}};

  Lexer lexer = lexer_new(input);
  Parser parser = parser_new(&lexer);
  AstProgram ast = parse_program(&parser);

  ASSERT_EQ_INT(arrlen(ast.statements), ARRAY_LEN(tests));

  for (int i = 0; i < ARRAY_LEN(tests); i++) {
    Ast statement = ast.statements[i];
    ASSERT(statement.kind == Ast_Let);

    ASSERT(string_const_eq_s(statement.Let.identifier.value,
                             tests[i].expectedIdentifier));
  }
}

void test_integer_expression() {
  const char *input = "\
5;\
6;\
-1;\
";

  struct {
    int expectedValue;
  } tests[] = {{5}, {6}, {-1}};

  Lexer lexer = lexer_new(input);
  Parser parser = parser_new(&lexer);
  AstProgram ast = parse_program(&parser);

  ASSERT_EQ_INT(arrlen(ast.statements), ARRAY_LEN(tests));

  for (int i = 0; i < ARRAY_LEN(tests); i++) {
    Ast statement = ast.statements[i];
    ASSERT_EQ_INT(statement.kind, Ast_Integer);

    ASSERT_EQ_INT(statement.Integer.value, tests[i].expectedValue);
  }
}

void test_boolean_expression() {
  const char *input = "\
true;\
false;\
";
  struct {
    bool expectedValue;
  } tests[] = {{TRUE}, {FALSE}};

  AstProgram ast = parse_input(input);

  ASSERT_EQ_INT(arrlen(ast.statements), ARRAY_LEN(tests));

  for (int i = 0; i < ARRAY_LEN(tests); i++) {
    Ast stm = ast.statements[i];
    ASSERT_EQ_INT(stm.kind, Ast_Boolean);
    ASSERT_EQ_INT(stm.Boolean.value, tests[i].expectedValue);
  }
}

void test_parser() {
  test_let_statements();
  test_integer_expression();
  // test_boolean_expression();
}

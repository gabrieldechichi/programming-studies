#include "ast.c"
#include "lexer.c"
#include "parser.c"
#include "string.c"
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

    ASSERT(strslice_eq_s(statement.Let.identifier.value,
                         tests[i].expectedIdentifier));
  }
}

void test_integer_expression() {
  const char *input = "\
5;\
6;\
1;\
";

  struct {
    int expectedValue;
  } tests[] = {{5}, {6}, {1}};

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

void test_string_expression() {
  const char *input = "\
\"foobar\";\
\"foo bar\";\
";
  struct {
    const char *expectedValue;
  } tests[] = {{"foobar"}, {"foo bar"}};

  AstProgram ast = parse_input(input);

  ASSERT_EQ_INT(arrlen(ast.statements), ARRAY_LEN(tests));

  for (int i = 0; i < ARRAY_LEN(tests); i++) {
    Ast stm = ast.statements[i];
    ASSERT_EQ_INT(stm.kind, Ast_String);
    ASSERT(strslice_eq_s(stm.String.value, tests[i].expectedValue));
  }
}

void test_prefix_operator() {
  const char *input = "\
!5;\
-10;\
!true;\
!false;\
";

  struct TestCase {
    const char *operator;
    union {
      int number;
      bool flag;
    } value;
  } tests[] = {{"!", {5}}, {"-", {10}}, {"!", {TRUE}}, {"!", {FALSE}}};

  AstProgram ast = parse_input(input);

  ASSERT_EQ_INT(arrlen(ast.statements), ARRAY_LEN(tests));

  for (int i = 0; i < ARRAY_LEN(tests); i++) {
    Ast stm = ast.statements[i];
    ASSERT_EQ_INT(stm.kind, Ast_PrefixOperator);

    ASSERT_WITH_MSG(
        strslice_eq_s(stm.PrefixOperator.operator, tests[i].operator),
        "Expected %s got %.*s",
        tests[i].operator,(int) stm.PrefixOperator.operator.len,
        stm.PrefixOperator.operator.value);
    // todo: test prefix expression
  }
}

void test_return_expression() {
  //   const char *input = "\
// return 5;\
// return x + 1;\
// return x + y * 2;\
// return add(1,2);\
// ";
  const char *input = "\
return 5;\
return x;\
return y;\
";

  struct TestCase {
    const char *expectedIdentifier;
  } tests[] = {{"5"}, {"x"}, {"y"}};

  AstProgram ast = parse_input(input);

  ASSERT_EQ_INT(ARRAY_LEN(tests), arrlen(ast.statements));

  for (int i = 0; i < ARRAY_LEN(tests); i++) {
    Ast stm = ast.statements[i];
    ASSERT_EQ_INT(stm.kind, Ast_Return);
    Ast *right = stm.Return.expression;
    StringSlice right_str = expression_to_string(right);
    ASSERT(strslice_eq_s(right_str, tests[i].expectedIdentifier));
  }
}

void test_parser() {
  test_let_statements();
  test_integer_expression();
  test_boolean_expression();
  test_string_expression();
  test_prefix_operator();
  test_return_expression();
}

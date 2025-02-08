#include "ast.c"
#include "lexer.c"
#include "macros.h"
#include "parser.c"
#include "string.c"
#include "token.c"
#include "vendor/stb/stb_ds.h"
#include <stdio.h>

#define ASSERT_TEST_STR(i, actual, expected)                                   \
  ASSERT_WITH_MSG(strslice_eq_s(actual, expected),                             \
                  "Test %d failed. Expected %s got %.*s", i, expected,         \
                  STRING_SLICE_PRINTARGS(actual))

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
  const char *input = "\
return 5;\
return x + 1;\
return x + y * 2;\
return add(1,2);\
";
  // return add(1,2);

  struct TestCase {
    const char *expectedIdentifier;
  } tests[] = {{"5"}, {"(x + 1)"}, {"(x + (y * 2))"}, {"add(1, 2)"}};

  //   const char *input = "\
// return 5;\
// return x;\
// return y;\
// ";

  // struct TestCase {
  //   const char *expectedIdentifier;
  // } tests[] = {{"5"}, {"x"}, {"y"}};

  AstProgram ast = parse_input(input);

  ASSERT_EQ_INT(ARRAY_LEN(tests), arrlen(ast.statements));

  for (int i = 0; i < ARRAY_LEN(tests); i++) {
    Ast stm = ast.statements[i];
    ASSERT_EQ_INT(stm.kind, Ast_Return);
    Ast *right = stm.Return.expression;
    StringSlice right_str = expression_to_string(right);

    ASSERT_WITH_MSG(strslice_eq_s(right_str, tests[i].expectedIdentifier),
                    "Test %d failed. Expected %s got %.*s", i,
                    tests[i].expectedIdentifier,
                    STRING_SLICE_PRINTARGS(right_str))
  }
}

void test_infix_expression() {
  const char *input = "5 + 5;\
5 - 5;\
5 * 5;\
5 / 5;\
5 > 5;\
5 < 5;\
5 == 5;\
5 != 5;\
5 <= 5;\
5 >= 5;\
true == false;\
true != false;\
";

  typedef union {
    int n;
    bool flag;
  } Value;

  typedef struct {
    Value left;
    Value right;
    const TokenOperation operator;
  } TestCase;

  TestCase tests[] = {
      {{5}, {5}, OP_ADD},       {{5}, {5}, OP_SUB},
      {{5}, {5}, OP_MUL},       {{5}, {5}, OP_DIV},
      {{5}, {5}, OP_GT},        {{5}, {5}, OP_LT},
      {{5}, {5}, OP_EQ},        {{5}, {5}, OP_NOTEQ},
      {{5}, {5}, OP_LTOREQ},    {{5}, {5}, OP_GTOREQ},
      {{TRUE}, {FALSE}, OP_EQ}, {{TRUE}, {FALSE}, OP_NOTEQ},
  };

  AstProgram ast = parse_input(input);

  ASSERT_EQ_INT(ARRAY_LEN(tests), arrlen(ast.statements));

  for (int i = 0; i < ARRAY_LEN(tests); i++) {
    Ast stm = ast.statements[i];
    TestCase test = tests[i];
    ASSERT_EQ_INT(stm.kind, Ast_InfixExpression);
    ASSERT_EQ_INT(stm.InfixExpression.operator, test.operator);

    ASSERT_EQ_INT(test.left.n, stm.InfixExpression.left->Integer.value);
    ASSERT_EQ_INT(test.right.n, stm.InfixExpression.right->Integer.value);
  }
}

void test_operator_precedence() {
  typedef struct {
    const char *input;
    const char *expected;
  } TestCase;

  TestCase tests[] = {
      {
          "-a * b",
          "((-a) * b)",
      },
      {
          "1 + 2 * 3",
          "(1 + (2 * 3))",
      },
      {
          "!-a",
          "(!(-a))",
      },
      {
          "a + b + c",
          "((a + b) + c)",
      },
      {
          "a + b - c",
          "((a + b) - c)",
      },
      {
          "a * b * c",
          "((a * b) * c)",
      },
      {
          "a * b / c",
          "((a * b) / c)",
      },
      {
          "a + b / c",
          "(a + (b / c))",
      },
      {
          "a + b * c + d / e - f",
          "(((a + (b * c)) + (d / e)) - f)",
      },
      {
          "5 > 4 == 3 < 4",
          "((5 > 4) == (3 < 4))",
      },
      {
          "5 < 4 != 3 > 4",
          "((5 < 4) != (3 > 4))",
      },
      {
          "3 + 4 * 5 == 3 * 1 + 4 * 5",
          "((3 + (4 * 5)) == ((3 * 1) + (4 * 5)))",
      },
      {
          "3 + 4 * 5 == 3 * 1 + 4 * 5",
          "((3 + (4 * 5)) == ((3 * 1) + (4 * 5)))",
      },
      {
          "3 > 5 == true",
          "((3 > 5) == true)",
      },
      {
          "3 == 5 != false",
          "((3 == 5) != false)",
      },
      {
          "1 + (2 + 3) + 4",
          "((1 + (2 + 3)) + 4)",
      },
      {
          "(5 + 5) * 2",
          "((5 + 5) * 2)",
      },
      {
          "2 / (5 + 5)",
          "(2 / (5 + 5))",
      },
      {
          "-(5 + 5)",
          "(-(5 + 5))",
      },
      {
          "!(true == true)",
          "(!(true == true))",
      },
      // {
      //     "a * [1, 2, 3, 4][b * c] * d",
      //     "((a * ([1, 2, 3, 4][(b * c)])) * d)",
      // },
      // {
      //     "add(a * b[2], b[1], 2 * [1, 2][1])",
      //     "add((a * (b[2])), (b[1]), (2 * ([1, 2][1])))",
      // },
  };

  for (int i = 0; i < ARRAY_LEN(tests); i++) {
    TestCase t = tests[i];
    AstProgram ast = parse_input(t.input);
    ASSERT_EQ_INT(1, arrlen(ast.statements));

    StringSlice statement_str = expression_to_string(&ast.statements[0]);
    ASSERT(strslice_eq_s(statement_str, t.expected));
  }
}

void test_func_call_expression() {
  const char *input = "add(2, 3);\
add(2+3*4, 2 * 4 + 4);\
add((2+3)*4, 3);\
add(pow(2,3), 3);\
";

  typedef struct {
    const char *expected;
  } TestCase;

  TestCase tests[] = {
      {"add(2, 3)"},
      {"add((2 + (3 * 4)), ((2 * 4) + 4))"},
      {"add(((2 + 3) * 4), 3)"},
      {"add(pow(2, 3), 3)"},
  };

  AstProgram ast = parse_input(input);
  ASSERT_EQ_INT(arrlen(ast.statements), ARRAY_LEN(tests));

  for (int i = 0; i < ARRAY_LEN(tests); i++) {
    TestCase t = tests[i];
    Ast expr = ast.statements[i];
    ASSERT_EQ_INT(Ast_FunctionCallExpression, expr.kind);
    StringSlice func = expression_to_string(&expr);
    ASSERT_TEST_STR(i, func, t.expected);
  }
}

void test_if_expression() {
  const char *input = "\
if (x < y) { x }\
if (x < y) { x } else { y }\
if (x * 2 > y - 1) {\
    let a = x * x;\
    return a;\
}\
";

  typedef struct {
    const char *condition;
    const char *consequence;
    const bool hasAlternative;
    const char *alternative;
  } TestCase;

  TestCase tests[] = {
      {"(x < y)", "{x}\n", FALSE, ""},
      {"(x < y)", "{x}\n", TRUE, "{y}\n"},
      {"((x * 2) > (y - 1))", "let a = (x * x)\nreturn a", FALSE, ""},
  };

  AstProgram ast = parse_input(input);
  ASSERT_EQ_INT(arrlen(ast.statements), ARRAY_LEN(tests));

  for (int i = 0; i < ARRAY_LEN(tests); i++) {
    TestCase test = tests[i];
    Ast expr = ast.statements[i];
    ASSERT_EQ_INT(Ast_IfExpression, expr.kind);
    StringSlice condition = expression_to_string(expr.IfExpression.condition);
    ASSERT_TEST_STR(i, condition, test.condition);
    StringSlice consequence =
        expression_to_string(expr.IfExpression.consequence);
    ASSERT_TEST_STR(i, consequence, test.consequence);
    if (test.hasAlternative) {
      ASSERT_NOT_NULL(expr.IfExpression.alternative);
      StringSlice alternative =
          expression_to_string(expr.IfExpression.alternative);
      ASSERT_TEST_STR(i, alternative, test.alternative);
    }
  }
}

void test_parser() {
  // test_let_statements();
  // test_integer_expression();
  // test_boolean_expression();
  // test_string_expression();
  // test_prefix_operator();
  // test_return_expression();
  // test_infix_expression();
  // test_operator_precedence();
  // test_func_call_expression();
  test_if_expression();
}

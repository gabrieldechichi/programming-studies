#import "./token.c"
#import "./utils.c"
#import "./lexer.c"
#include <assert.h>
#include <stdio.h>
#include <string.h>

void test_lexer_reserved() {
  const char *input = "\
=;\
==\
!;\
!=\
+\
-\
*\
/\
,\
;\
(\
)\
{\
}\
[\
]\
";

  struct {
    TokenType expectedType;
  } tests[] = {
      {TP_ASSIGN},    //
      {TP_SEMICOLON}, //
      {TP_EQ},        //
      {TP_BANG},      //
      {TP_SEMICOLON}, //
      {TP_NOT_EQ},    //
      {TP_PLUS},      //
      {TP_MINUS},     //
      {TP_ASTERISK},  //
      {TP_SLASH},     //
      {TP_COMMA},     //
      {TP_SEMICOLON}, //
      {TP_LPAREN},    //
      {TP_RPAREN},    //
      {TP_LBRACE},    //
      {TP_RBRACE},    //
      {TP_LBRACKET},  //
      {TP_RBRACKET},  //
  };
  Lexer l = lexer_new(input);

  for (int i = 0; i < ARRAY_LEN(tests); ++i) {
    Token t = lexer_next_token(&l);
    ASSERT_WITH_MSG(
        t.type == tests[i].expectedType, "Test %d failed. Expected %s found %s",
        i, token_type_to_str(tests[i].expectedType), token_type_to_str(t.type));
  }
}

void test_lexer_identifiers() {

  const char *input = "\
let five; \
fn;\
\"foo\";\
true;\
false;\
if;\
else;\
return;\
";

  struct {
    TokenType expectedType;
    char *literal;
  } tests[] = {
      {TP_LET, "let"},       //
      {TP_IDENT, "five"},    //
      {TP_SEMICOLON, ";"},   //
      {TP_FUNC, "fn"},       //
      {TP_SEMICOLON, ";"},   //
      {TP_STRING, "foo"},    //
      {TP_SEMICOLON, ";"},   //
      {TP_TRUE, "true"},     //
      {TP_SEMICOLON, ";"},   //
      {TP_FALSE, "false"},   //
      {TP_SEMICOLON, ";"},   //
      {TP_IF, "if"},         //
      {TP_SEMICOLON, ";"},   //
      {TP_ELSE, "else"},     //
      {TP_SEMICOLON, ";"},   //
      {TP_RETURN, "return"}, //
      {TP_SEMICOLON, ";"},   //
  };

  Lexer l = lexer_new(input);

  for (int i = 0; i < ARRAY_LEN(tests); ++i) {
    Token t = lexer_next_token(&l);
    ASSERT_WITH_MSG(
        t.type == tests[i].expectedType, "Test %d failed. Expected %s found %s",
        i, token_type_to_str(tests[i].expectedType), token_type_to_str(t.type));

    ASSERT_WITH_MSG(string_const_eq_s(t.literal, tests[i].literal),
                    "Test %d failed. Expected %s found %s", i, tests[i].literal,
                    t.literal.value);
  }
}

void test_lexer_all() {

  const char *input = "let five = 5; \
let ten = 10; \
\
let add = fn(x, y) { \
  x + y; \
}; \
\
let result = add(five, ten); \
!-/*5; \
5 < 10 > 5; \
\
if (5 < 10) { \
  return true; \
} else { \
  return false; \
} \
\
10 == 10; \
10 != 9; \
10 >= 9; \
9 <= 10; \
\
\"foo\" \
\"foo bar\" \
\
[1,2]";

  struct {
    TokenType expectedType;
    char *literal;
  } tests[] = {
      {TP_LET, "let"},        // let
      {TP_IDENT, "five"},     // five
      {TP_ASSIGN, "="},       // =
      {TP_INT, "5"},          // 5
      {TP_SEMICOLON, ";"},    // ;
      {TP_LET, "let"},        // let
      {TP_IDENT, "ten"},      // ten
      {TP_ASSIGN, "="},       // =
      {TP_INT, "10"},         // 10
      {TP_SEMICOLON, ";"},    // ;
      {TP_LET, "let"},        // let
      {TP_IDENT, "add"},      // add
      {TP_ASSIGN, "="},       // =
      {TP_FUNC, "fn"},        // fn
      {TP_LPAREN, "("},       // (
      {TP_IDENT, "x"},        // x
      {TP_COMMA, ","},        // ,
      {TP_IDENT, "y"},        // y
      {TP_RPAREN, ")"},       // )
      {TP_LBRACE, "{"},       // {
      {TP_IDENT, "x"},        // x
      {TP_PLUS, "+"},         // +
      {TP_IDENT, "y"},        // y
      {TP_SEMICOLON, ";"},    // ;
      {TP_RBRACE, "}"},       // }
      {TP_SEMICOLON, ";"},    // ;
      {TP_LET, "let"},        // let
      {TP_IDENT, "result"},   // result
      {TP_ASSIGN, "="},       // =
      {TP_IDENT, "add"},      // add
      {TP_LPAREN, "("},       // (
      {TP_IDENT, "five"},     // five
      {TP_COMMA, ","},        // ,
      {TP_IDENT, "ten"},      // ten
      {TP_RPAREN, ")"},       // )
      {TP_SEMICOLON, ";"},    // ;
      {TP_BANG, "!"},         // !
      {TP_MINUS, "-"},        // -
      {TP_SLASH, "/"},        // /
      {TP_ASTERISK, "*"},     // *
      {TP_INT, "5"},          // 5
      {TP_SEMICOLON, ";"},    // ;
      {TP_INT, "5"},          // 5
      {TP_LT, "<"},           // <
      {TP_INT, "10"},         // 10
      {TP_GT, ">"},           // >
      {TP_INT, "5"},          // 5
      {TP_SEMICOLON, ";"},    // ;
      {TP_IF, "if"},          // if
      {TP_LPAREN, "("},       // (
      {TP_INT, "5"},          // 5
      {TP_LT, "<"},           // <
      {TP_INT, "10"},         // 10
      {TP_RPAREN, ")"},       // )
      {TP_LBRACE, "{"},       // {
      {TP_RETURN, "return"},  // return
      {TP_TRUE, "true"},      // true
      {TP_SEMICOLON, ";"},    // ;
      {TP_RBRACE, "}"},       // }
      {TP_ELSE, "else"},      // else
      {TP_LBRACE, "{"},       // {
      {TP_RETURN, "return"},  // return
      {TP_FALSE, "false"},    // false
      {TP_SEMICOLON, ";"},    // ;
      {TP_RBRACE, "}"},       // }
      {TP_INT, "10"},         // 10
      {TP_EQ, "=="},          // ==
      {TP_INT, "10"},         // 10
      {TP_SEMICOLON, ";"},    // ;
      {TP_INT, "10"},         // 10
      {TP_NOT_EQ, "!="},      // !=
      {TP_INT, "9"},          // 9
      {TP_SEMICOLON, ";"},    // ;
      {TP_INT, "10"},         // 10
      {TP_GTOREQ, ">="},      // >=
      {TP_INT, "9"},          // 9
      {TP_SEMICOLON, ";"},    // ;
      {TP_INT, "9"},          // 9
      {TP_LTOREQ, "<="},      // <=
      {TP_INT, "10"},         // 10
      {TP_SEMICOLON, ";"},    // ;
      {TP_STRING, "foo"},     // "foo"
      {TP_STRING, "foo bar"}, // "foo bar"
      {TP_LBRACKET, "["},     // [
      {TP_INT, "1"},          // 1
      {TP_COMMA, ","},        // ,
      {TP_INT, "2"},          // 2
      {TP_RBRACKET, "]"},     // ]
  };

  Lexer l = lexer_new(input);

  for (int i = 0; i < ARRAY_LEN(tests); ++i) {
    Token t = lexer_next_token(&l);
    ASSERT_WITH_MSG(
        t.type == tests[i].expectedType, "Test %d failed. Expected %s found %s",
        i, token_type_to_str(tests[i].expectedType), token_type_to_str(t.type));

    ASSERT_WITH_MSG(string_const_eq_s(t.literal, tests[i].literal),
                    "Test %d failed. Expected %s found %s", i, tests[i].literal,
                    t.literal.value);
  }
}

void test_lexer() {
  test_lexer_reserved();
  test_lexer_identifiers();
  test_lexer_all();
}

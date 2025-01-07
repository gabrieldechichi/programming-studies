#import "./token.c"
#import "./utils.c"
#include <assert.h>
#include <stdio.h>

void test_token() {
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

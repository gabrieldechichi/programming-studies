#ifndef H_PARSER
#define H_PARSER

#include "lib/array.h"
#include "lib/memory.h"
#include "lib/string.h"
#include "lib/typedefs.h"
#include "meta/tokenizer.h"

typedef struct {
  String type_name;
  String field_name;
} StructField;
arr_define(StructField);


typedef struct {
  String struct_name;
  u32 type_id;
  StructField_Array fields;
} ReflectedStruct;
arr_define(ReflectedStruct);


typedef struct {
  Tokenizer tokenizer;
  Token current_token;
  b32 has_error;
  String error_message;
  Allocator *allocator;
} Parser;

Parser parser_create(const char *filename, const char *source, u32 source_length,
                     Allocator *allocator);
b32 parse_file(Parser *parser, ReflectedStruct_DynArray *out_structs);
void parser_destroy(Parser *parser);
void parser_reset_type_id();

b32 parser_current_token_is(Parser *parser, TokenType type);
void parser_advance_token(Parser *parser);
b32 parser_expect_token_and_advance(Parser *parser, TokenType type);
void parser_error(Parser *parser, const char *message);
void skip_to_next_token_of_type(Parser *parser, TokenType token_type);

#endif
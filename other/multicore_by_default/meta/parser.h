#ifndef H_PARSER
#define H_PARSER

#include "lib/array.h"
#include "lib/memory.h"
#include "lib/string.h"
#include "lib/typedefs.h"
#include "meta/tokenizer.h"

// Forward declarations
typedef struct StructField StructField;
typedef struct ReflectedStruct ReflectedStruct;

// Attribute types
typedef struct {
  String name;
  StructField *parent_field;
} FieldAttribute;
arr_define(FieldAttribute);

typedef struct {
  String name;
  ReflectedStruct *parent_struct;
} StructAttribute;
arr_define(StructAttribute);

// Main struct definitions
struct StructField {
  String type_name;
  String field_name;
  u32 pointer_depth;
  b32 is_array;
  u32 array_size;
  FieldAttribute_DynArray attributes;
};
arr_define(StructField);

struct ReflectedStruct {
  String struct_name;
  String typedef_name;
  u32 type_id;
  StructAttribute_DynArray attributes;
  StructField_DynArray fields;
};
arr_define(ReflectedStruct);


#define PARSER_TOKEN_HISTORY_SIZE 16

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
b32 parse_struct(Parser *parser, ReflectedStruct *out_struct);
void parser_destroy(Parser *parser);
void parser_reset_type_id();

b32 parser_current_token_is(Parser *parser, TokenType type);
void parser_advance_token(Parser *parser);
b32 parser_expect_token_and_advance(Parser *parser, TokenType type);
void parser_error(Parser *parser, const char *message);
void parser_skip_to_next_token_type(Parser *parser, TokenType token_type);
void parser_skip_to_next_attribute(Parser *parser);

#endif
#ifndef H_JSON_PARSER
#define H_JSON_PARSER

#include "memory.h"
#include "typedefs.h"

typedef struct {
  const char *str;
  uint32 pos;
  uint32 len;
  Allocator *arena;
} JsonParser;

// Parser initialization
JsonParser json_parser_init(const char *input, Allocator *arena);

// Primitive value parsers
char *json_parse_string_value(JsonParser *parser);
double json_parse_number_value(JsonParser *parser);
bool32 json_parse_bool_value(JsonParser *parser);
bool32 json_parse_null_value(JsonParser *parser);

// Structural parsing helpers
bool32 json_expect_object_start(JsonParser *parser);
bool32 json_expect_object_end(JsonParser *parser);
bool32 json_expect_colon(JsonParser *parser);
bool32 json_expect_comma(JsonParser *parser);
char *json_expect_key(JsonParser *parser, const char *expected_key);
bool32 json_is_at_end(JsonParser *parser);

// Low-level character functions
void json_skip_whitespace(JsonParser *parser);
char json_peek_char(JsonParser *parser);
char json_consume_char(JsonParser *parser);
bool32 json_expect_char(JsonParser *parser, char expected);

#endif
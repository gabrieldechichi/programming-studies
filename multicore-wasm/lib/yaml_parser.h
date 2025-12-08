#ifndef H_YAML_PARSER
#define H_YAML_PARSER

#include "memory.h"
#include "typedefs.h"

typedef struct {
  const char *str;
  uint32 pos;
  uint32 len;
  uint32 indent_stack[32];  // Stack of indentation levels
  uint32 indent_depth;       // Current depth in indent stack
  Allocator *arena;
} YamlParser;

// Parser initialization
YamlParser yaml_parser_init(const char *input, Allocator *arena);

// Primitive value parsers
char *yaml_parse_string_value(YamlParser *parser);
double yaml_parse_number_value(YamlParser *parser);
bool32 yaml_parse_bool_value(YamlParser *parser);
bool32 yaml_parse_null_value(YamlParser *parser);

// Structural parsing helpers
bool32 yaml_expect_key(YamlParser *parser, const char *expected_key);
bool32 yaml_expect_list_item(YamlParser *parser);
bool32 yaml_is_at_block_end(YamlParser *parser);
void yaml_push_indent(YamlParser *parser);
void yaml_pop_indent(YamlParser *parser);
bool32 yaml_is_at_end(YamlParser *parser);

// Low-level character functions
void yaml_skip_whitespace_inline(YamlParser *parser);
void yaml_skip_to_next_line(YamlParser *parser);
void yaml_skip_empty_lines_and_comments(YamlParser *parser);
uint32 yaml_get_current_indent(YamlParser *parser);
char yaml_peek_char(YamlParser *parser);
char yaml_consume_char(YamlParser *parser);
bool32 yaml_expect_char(YamlParser *parser, char expected);

#endif
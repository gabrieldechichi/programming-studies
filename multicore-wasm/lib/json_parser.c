#include "json_parser.h"
#include "common.h"
#include "lib/string.h"
#include "lib/memory.h"

// Parser initialization
JsonParser json_parser_init(const char *input, Allocator *arena) {
  JsonParser parser = {0};
  parser.str = input;
  parser.pos = 0;
  parser.len = str_len(input);
  parser.arena = arena;
  return parser;
}

// Low-level character functions
void json_skip_whitespace(JsonParser *parser) {
  while (parser->pos < parser->len && char_is_space(parser->str[parser->pos])) {
    parser->pos++;
  }
}

char json_peek_char(JsonParser *parser) {
  if (parser->pos >= parser->len)
    return '\0';
  return parser->str[parser->pos];
}

char json_consume_char(JsonParser *parser) {
  if (parser->pos >= parser->len)
    return '\0';
  return parser->str[parser->pos++];
}

b32 json_expect_char(JsonParser *parser, char expected) {
  json_skip_whitespace(parser);
  if (json_peek_char(parser) == expected) {
    json_consume_char(parser);
    return true;
  }
  return false;
}

// Primitive value parsers
char *json_parse_string_value(JsonParser *parser) {
  json_skip_whitespace(parser);
  if (!json_expect_char(parser, '"'))
    return NULL;

  uint32 start = parser->pos;
  uint32 len = 0;

  while (parser->pos < parser->len && parser->str[parser->pos] != '"') {
    if (parser->str[parser->pos] == '\\') {
      parser->pos++;
      if (parser->pos < parser->len)
        parser->pos++;
    } else {
      parser->pos++;
    }
    len++;
  }

  if (parser->pos >= parser->len)
    return NULL;

  parser->pos++;

  char *result = ALLOC_ARRAY(parser->arena, char, len + 1);
  if (!result)
    return NULL;

  uint32 src = start;
  uint32 dst = 0;
  while (dst < len) {
    if (parser->str[src] == '\\') {
      src++;
      char c = parser->str[src++];
      switch (c) {
        case 'n': result[dst++] = '\n'; break;
        case 'r': result[dst++] = '\r'; break;
        case 't': result[dst++] = '\t'; break;
        case '\\': result[dst++] = '\\'; break;
        case '"': result[dst++] = '"'; break;
        case '/': result[dst++] = '/'; break;
        default: result[dst++] = c; break;
      }
    } else {
      result[dst++] = parser->str[src++];
    }
  }
  result[len] = '\0';

  return result;
}

double json_parse_number_value(JsonParser *parser) {
  json_skip_whitespace(parser);
  uint32 start = parser->pos;

  // Skip optional minus
  if (json_peek_char(parser) == '-')
    json_consume_char(parser);

  // Parse digits
  while (parser->pos < parser->len && char_is_digit(parser->str[parser->pos])) {
    parser->pos++;
  }

  // Parse optional decimal part
  if (json_peek_char(parser) == '.') {
    json_consume_char(parser);
    while (parser->pos < parser->len &&
           char_is_digit(parser->str[parser->pos])) {
      parser->pos++;
    }
  }

  // Parse optional exponent
  char c = json_peek_char(parser);
  if (c == 'e' || c == 'E') {
    json_consume_char(parser);
    c = json_peek_char(parser);
    if (c == '+' || c == '-')
      json_consume_char(parser);
    while (parser->pos < parser->len &&
           char_is_digit(parser->str[parser->pos])) {
      parser->pos++;
    }
  }

  uint32 len = parser->pos - start;
  char *temp = ALLOC_ARRAY(parser->arena, char, len + 1);
  memcpy(temp, parser->str + start, len);
  temp[len] = '\0';

  return str_to_double(temp);
}

b32 json_parse_bool_value(JsonParser *parser) {
  json_skip_whitespace(parser);
  char c = json_peek_char(parser);

  if (c == 't') {
    if (parser->pos + 4 <= parser->len &&
        str_equal_len(parser->str + parser->pos, 4, "true", 4)) {
      parser->pos += 4;
      return true;
    }
  } else if (c == 'f') {
    if (parser->pos + 5 <= parser->len &&
        str_equal_len(parser->str + parser->pos, 5, "false", 5)) {
      parser->pos += 5;
      return false;
    }
  }

  assert_msg(false, "Expected boolean value");
  return false;
}

b32 json_parse_null_value(JsonParser *parser) {
  json_skip_whitespace(parser);
  if (parser->pos + 4 <= parser->len &&
      str_equal_len(parser->str + parser->pos, 4, "null", 4)) {
    parser->pos += 4;
    return true;
  }
  assert_msg(false, "Expected null value");
  return false;
}

// Structural parsing helpers
b32 json_expect_object_start(JsonParser *parser) {
  return json_expect_char(parser, '{');
}

b32 json_expect_object_end(JsonParser *parser) {
  return json_expect_char(parser, '}');
}

b32 json_expect_colon(JsonParser *parser) {
  return json_expect_char(parser, ':');
}

b32 json_expect_comma(JsonParser *parser) {
  return json_expect_char(parser, ',');
}

char *json_expect_key(JsonParser *parser, const char *expected_key) {
  char *key = json_parse_string_value(parser);
  assert_msg(key && str_equal(key, expected_key), "Expected specific key");
  return key;
}

b32 json_is_at_end(JsonParser *parser) {
  json_skip_whitespace(parser);
  return parser->pos >= parser->len;
}
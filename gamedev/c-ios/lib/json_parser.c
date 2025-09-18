#include "json_parser.h"
#include "common.h"
#include "string.h"

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

bool32 json_expect_char(JsonParser *parser, char expected) {
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
      parser->pos++; // Skip escape character
      if (parser->pos < parser->len)
        parser->pos++; // Skip escaped character
    } else {
      parser->pos++;
    }
    len++;
  }

  if (parser->pos >= parser->len)
    return NULL; // Unterminated string

  parser->pos++; // Skip closing quote

  char *result = ALLOC_ARRAY(parser->arena, char, len + 1);
  if (!result)
    return NULL;

  // Simple copy (TODO: handle escapes properly)
  strncpy(result, parser->str + start, len);
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
  strncpy(temp, parser->str + start, len);
  temp[len] = '\0';

  return str_to_double(temp);
}

bool32 json_parse_bool_value(JsonParser *parser) {
  json_skip_whitespace(parser);
  char c = json_peek_char(parser);

  if (c == 't') {
    if (parser->pos + 4 <= parser->len &&
        strncmp(parser->str + parser->pos, "true", 4) == 0) {
      parser->pos += 4;
      return true;
    }
  } else if (c == 'f') {
    if (parser->pos + 5 <= parser->len &&
        strncmp(parser->str + parser->pos, "false", 5) == 0) {
      parser->pos += 5;
      return false;
    }
  }

  assert_msg(false, "Expected boolean value");
  return false;
}

bool32 json_parse_null_value(JsonParser *parser) {
  json_skip_whitespace(parser);
  if (parser->pos + 4 <= parser->len &&
      strncmp(parser->str + parser->pos, "null", 4) == 0) {
    parser->pos += 4;
    return true;
  }
  assert_msg(false, "Expected null value");
  return false;
}

// Structural parsing helpers
void json_expect_object_start(JsonParser *parser) {
  assert_msg(json_expect_char(parser, '{'), "Expected '{'");
}

void json_expect_object_end(JsonParser *parser) {
  assert_msg(json_expect_char(parser, '}'), "Expected '}'");
}

void json_expect_colon(JsonParser *parser) {
  assert_msg(json_expect_char(parser, ':'), "Expected ':'");
}

void json_expect_comma(JsonParser *parser) {
  assert_msg(json_expect_char(parser, ','), "Expected ','");
}

char *json_expect_key(JsonParser *parser, const char *expected_key) {
  char *key = json_parse_string_value(parser);
  assert_msg(key && str_equal(key, expected_key), "Expected specific key");
  return key;
}

bool32 json_is_at_end(JsonParser *parser) {
  json_skip_whitespace(parser);
  return parser->pos >= parser->len;
}
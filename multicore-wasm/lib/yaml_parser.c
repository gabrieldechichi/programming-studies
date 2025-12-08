#include "yaml_parser.h"
#include "common.h"
#include "lib/string.h"

// Parser initialization
YamlParser yaml_parser_init(const char *input, Allocator *arena) {
  YamlParser parser = {0};
  parser.str = input;
  parser.pos = 0;
  parser.len = str_len(input);
  parser.arena = arena;
  parser.indent_depth = 0;
  parser.indent_stack[0] = 0;
  return parser;
}

// Low-level character functions
void yaml_skip_whitespace_inline(YamlParser *parser) {
  while (parser->pos < parser->len && 
         (parser->str[parser->pos] == ' ' || parser->str[parser->pos] == '\t')) {
    parser->pos++;
  }
}

void yaml_skip_to_next_line(YamlParser *parser) {
  while (parser->pos < parser->len && parser->str[parser->pos] != '\n') {
    parser->pos++;
  }
  if (parser->pos < parser->len && parser->str[parser->pos] == '\n') {
    parser->pos++;
  }
}

void yaml_skip_empty_lines_and_comments(YamlParser *parser) {
  while (parser->pos < parser->len) {
    // Skip leading whitespace to check line content
    uint32 temp_pos = parser->pos;
    while (temp_pos < parser->len && 
           (parser->str[temp_pos] == ' ' || parser->str[temp_pos] == '\t')) {
      temp_pos++;
    }
    
    // Check if line is empty or a comment
    if (temp_pos >= parser->len) {
      break;
    } else if (parser->str[temp_pos] == '\n') {
      parser->pos = temp_pos + 1;
    } else if (parser->str[temp_pos] == '#') {
      parser->pos = temp_pos;
      yaml_skip_to_next_line(parser);
    } else {
      break;
    }
  }
}

uint32 yaml_get_current_indent(YamlParser *parser) {
  uint32 saved_pos = parser->pos;
  
  // Move to start of current line
  while (parser->pos > 0 && parser->str[parser->pos - 1] != '\n') {
    parser->pos--;
  }
  
  // Count spaces at start of line
  uint32 indent = 0;
  while (parser->pos < parser->len && parser->str[parser->pos] == ' ') {
    indent++;
    parser->pos++;
  }
  
  parser->pos = saved_pos;
  return indent;
}

char yaml_peek_char(YamlParser *parser) {
  if (parser->pos >= parser->len)
    return '\0';
  return parser->str[parser->pos];
}

char yaml_consume_char(YamlParser *parser) {
  if (parser->pos >= parser->len)
    return '\0';
  return parser->str[parser->pos++];
}

bool32 yaml_expect_char(YamlParser *parser, char expected) {
  yaml_skip_whitespace_inline(parser);
  if (yaml_peek_char(parser) == expected) {
    yaml_consume_char(parser);
    return true;
  }
  return false;
}

// Indentation management
void yaml_push_indent(YamlParser *parser) {
  uint32 current_indent = yaml_get_current_indent(parser);
  assert_msg(parser->indent_depth < 31, "Indent stack overflow");
  parser->indent_stack[++parser->indent_depth] = current_indent;
}

void yaml_pop_indent(YamlParser *parser) {
  if (parser->indent_depth > 0) {
    parser->indent_depth--;
  }
}

bool32 yaml_is_at_block_end(YamlParser *parser) {
  yaml_skip_empty_lines_and_comments(parser);
  if (parser->pos >= parser->len) {
    return true;
  }
  
  uint32 current_indent = yaml_get_current_indent(parser);
  uint32 expected_indent = parser->indent_stack[parser->indent_depth];
  return current_indent <= expected_indent;
}

// Primitive value parsers
char *yaml_parse_string_value(YamlParser *parser) {
  yaml_skip_whitespace_inline(parser);
  
  uint32 start = parser->pos;
  uint32 len = 0;
  
  // Check for quoted string
  char quote = yaml_peek_char(parser);
  if (quote == '"' || quote == '\'') {
    yaml_consume_char(parser);
    start = parser->pos;
    
    while (parser->pos < parser->len && parser->str[parser->pos] != quote) {
      if (parser->str[parser->pos] == '\\' && quote == '"') {
        parser->pos++; // Skip escape character
        if (parser->pos < parser->len)
          parser->pos++; // Skip escaped character
      } else {
        parser->pos++;
      }
      len++;
    }
    
    if (parser->pos >= parser->len) {
      assert_msg(false, "Unterminated string");
      return NULL;
    }
    
    parser->pos++; // Skip closing quote
  } else {
    // Unquoted string - read until newline or comment
    while (parser->pos < parser->len && 
           parser->str[parser->pos] != '\n' && 
           parser->str[parser->pos] != '#') {
      parser->pos++;
      len++;
    }
    
    // Trim trailing whitespace
    while (len > 0 && (parser->str[start + len - 1] == ' ' || 
                       parser->str[start + len - 1] == '\t')) {
      len--;
    }
  }
  
  char *result = ALLOC_ARRAY(parser->arena, char, len + 1);
  if (!result)
    return NULL;
  
  strncpy(result, parser->str + start, len);
  result[len] = '\0';
  
  return result;
}

double yaml_parse_number_value(YamlParser *parser) {
  yaml_skip_whitespace_inline(parser);
  uint32 start = parser->pos;
  
  // Skip optional minus
  if (yaml_peek_char(parser) == '-')
    yaml_consume_char(parser);
  
  // Parse digits
  while (parser->pos < parser->len && char_is_digit(parser->str[parser->pos])) {
    parser->pos++;
  }
  
  // Parse optional decimal part
  if (yaml_peek_char(parser) == '.') {
    yaml_consume_char(parser);
    while (parser->pos < parser->len && char_is_digit(parser->str[parser->pos])) {
      parser->pos++;
    }
  }
  
  // Parse optional exponent
  char c = yaml_peek_char(parser);
  if (c == 'e' || c == 'E') {
    yaml_consume_char(parser);
    c = yaml_peek_char(parser);
    if (c == '+' || c == '-')
      yaml_consume_char(parser);
    while (parser->pos < parser->len && char_is_digit(parser->str[parser->pos])) {
      parser->pos++;
    }
  }
  
  uint32 len = parser->pos - start;
  char *temp = ALLOC_ARRAY(parser->arena, char, len + 1);
  strncpy(temp, parser->str + start, len);
  temp[len] = '\0';
  
  return str_to_double(temp);
}

bool32 yaml_parse_bool_value(YamlParser *parser) {
  yaml_skip_whitespace_inline(parser);
  
  // Check for various YAML boolean representations
  if (parser->pos + 4 <= parser->len &&
      strncmp(parser->str + parser->pos, "true", 4) == 0) {
    parser->pos += 4;
    return true;
  } else if (parser->pos + 3 <= parser->len &&
             strncmp(parser->str + parser->pos, "yes", 3) == 0) {
    parser->pos += 3;
    return true;
  } else if (parser->pos + 2 <= parser->len &&
             strncmp(parser->str + parser->pos, "on", 2) == 0) {
    parser->pos += 2;
    return true;
  } else if (parser->pos + 5 <= parser->len &&
             strncmp(parser->str + parser->pos, "false", 5) == 0) {
    parser->pos += 5;
    return false;
  } else if (parser->pos + 2 <= parser->len &&
             strncmp(parser->str + parser->pos, "no", 2) == 0) {
    parser->pos += 2;
    return false;
  } else if (parser->pos + 3 <= parser->len &&
             strncmp(parser->str + parser->pos, "off", 3) == 0) {
    parser->pos += 3;
    return false;
  }
  
  assert_msg(false, "Expected boolean value");
  return false;
}

bool32 yaml_parse_null_value(YamlParser *parser) {
  yaml_skip_whitespace_inline(parser);
  
  if (parser->pos + 4 <= parser->len &&
      strncmp(parser->str + parser->pos, "null", 4) == 0) {
    parser->pos += 4;
    return true;
  } else if (parser->pos + 1 <= parser->len &&
             parser->str[parser->pos] == '~') {
    parser->pos++;
    return true;
  }
  
  assert_msg(false, "Expected null value");
  return false;
}

// Structural parsing helpers
bool32 yaml_expect_key(YamlParser *parser, const char *expected_key) {
  yaml_skip_empty_lines_and_comments(parser);
  yaml_skip_whitespace_inline(parser);
  
  uint32 key_len = str_len(expected_key);
  
  // Check if the key matches
  if (parser->pos + key_len <= parser->len &&
      strncmp(parser->str + parser->pos, expected_key, key_len) == 0) {
    parser->pos += key_len;
    
    // Expect colon after key
    yaml_skip_whitespace_inline(parser);
    if (yaml_peek_char(parser) == ':') {
      yaml_consume_char(parser);
      yaml_skip_whitespace_inline(parser);
      
      // Check if value is on next line (indented block)
      if (yaml_peek_char(parser) == '\n') {
        yaml_skip_to_next_line(parser);
        yaml_skip_empty_lines_and_comments(parser);
      }
      
      return true;
    }
  }
  
  assert_msg(false, "Expected key");
  return false;
}

bool32 yaml_expect_list_item(YamlParser *parser) {
  yaml_skip_empty_lines_and_comments(parser);
  yaml_skip_whitespace_inline(parser);
  
  if (yaml_peek_char(parser) == '-') {
    yaml_consume_char(parser);
    yaml_skip_whitespace_inline(parser);
    
    // Check if value is on next line (indented block)
    if (yaml_peek_char(parser) == '\n') {
      yaml_skip_to_next_line(parser);
      yaml_skip_empty_lines_and_comments(parser);
    }
    
    return true;
  }
  
  return false;
}

bool32 yaml_is_at_end(YamlParser *parser) {
  yaml_skip_empty_lines_and_comments(parser);
  return parser->pos >= parser->len;
}
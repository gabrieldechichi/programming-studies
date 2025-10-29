#include "parser.h"
#include "../lib/assert.h"
#include "../lib/common.h"
#include "../lib/hash.h"

global u32 next_type_id = 1;

void parser_reset_type_id() { next_type_id = 1; }

internal void advance_token(Parser *parser) {
  parser->current_token = tokenizer_next_token(&parser->tokenizer);
}

internal b32 expect_token_and_advance(Parser *parser, TokenType type) {
  if (parser->current_token.type == type) {
    advance_token(parser);
    return true;
  }
  return false;
}

internal b32 current_token_is(Parser *parser, TokenType type) {
  return parser->current_token.type == type;
}

internal void parser_error_with_context(Parser *parser, const char *message) {
  parser->has_error = true;

  char error_buffer[1024];
  u32 pos = 0;

  // Header with file and position
  const char *filename =
      parser->tokenizer.filename ? parser->tokenizer.filename : "<unknown>";
  const char *header_fmt = "Error in file '";
  u32 header_len = str_len(header_fmt);
  for (u32 i = 0; i < header_len && pos < 1023; i++) {
    error_buffer[pos++] = header_fmt[i];
  }

  u32 filename_len = str_len(filename);
  for (u32 i = 0; i < filename_len && pos < 1023; i++) {
    error_buffer[pos++] = filename[i];
  }

  const char *line_fmt = "' at line ";
  u32 line_fmt_len = str_len(line_fmt);
  for (u32 i = 0; i < line_fmt_len && pos < 1023; i++) {
    error_buffer[pos++] = line_fmt[i];
  }

  pos += u64_to_str(parser->current_token.line, &error_buffer[pos]);

  const char *col_fmt = ", column ";
  u32 col_fmt_len = str_len(col_fmt);
  for (u32 i = 0; i < col_fmt_len && pos < 1023; i++) {
    error_buffer[pos++] = col_fmt[i];
  }

  pos += u64_to_str(parser->current_token.column, &error_buffer[pos]);

  const char *newline = ":\n";
  for (u32 i = 0; i < 2 && pos < 1023; i++) {
    error_buffer[pos++] = newline[i];
  }

  // Show context: 2 lines before, error line, 2 lines after
  u32 error_line = parser->current_token.line;
  u32 start_line = error_line >= 3 ? error_line - 2 : 1;
  u32 end_line = error_line + 2;
  if (end_line > parser->tokenizer.line_count) {
    end_line = parser->tokenizer.line_count;
  }

  for (u32 line_num = start_line; line_num <= end_line && pos < 1020;
       line_num++) {
    u32 line_length = 0;
    const char *line_text =
        tokenizer_get_line_text(&parser->tokenizer, line_num, &line_length);

    if (line_text && line_length > 0) {
      // Add line number
      if (line_num == error_line) {
        error_buffer[pos++] = ' ';
        error_buffer[pos++] = '>';
        error_buffer[pos++] = '>';
        error_buffer[pos++] = ' ';
      } else {
        error_buffer[pos++] = ' ';
        error_buffer[pos++] = ' ';
        error_buffer[pos++] = ' ';
        error_buffer[pos++] = ' ';
      }

      // Add line number
      pos += u64_to_str(line_num, &error_buffer[pos]);
      error_buffer[pos++] = ' ';
      error_buffer[pos++] = '|';
      error_buffer[pos++] = ' ';

      for (u32 i = 0; i < line_length && pos < 1023; i++) {
        error_buffer[pos++] = line_text[i];
      }

      error_buffer[pos++] = '\n';

      // Add caret indicator only for the error line
      if (line_num == error_line) {
        // Calculate spaces needed for line number alignment
        char line_num_str[16];
        u32 line_num_len = u64_to_str(line_num, line_num_str);

        for (u32 i = 0; i < 7 + line_num_len && pos < 1023; i++) {
          error_buffer[pos++] = ' ';
        }

        // Add spaces up to the error column
        for (u32 i = 1; i < parser->current_token.column && pos < 1023; i++) {
          error_buffer[pos++] = ' ';
        }
        error_buffer[pos++] = '^';
        error_buffer[pos++] = '\n';
      }
    }
  }

  // Add the error message
  u32 msg_len = str_len(message);
  for (u32 i = 0; i < msg_len && pos < 1023; i++) {
    error_buffer[pos++] = message[i];
  }

  error_buffer[pos] = '\0';

  parser->error_message =
      str_from_cstr_with_len_alloc(error_buffer, pos - 1, parser->allocator);
}

internal void parser_error(Parser *parser, const char *message) {
  parser_error_with_context(parser, message);
}

internal String token_to_string(Token token, Allocator *allocator) {
  return str_from_cstr_with_len_alloc(token.lexeme, token.length, allocator);
}

internal void skip_to_next_hm_reflect(Parser *parser) {
  while (!current_token_is(parser, TOKEN_EOF)) {
    if (current_token_is(parser, TOKEN_HM_REFLECT)) {
      break;
    }
    advance_token(parser);
  }
}

internal b32 parse_struct_fields(Parser *parser, StructField_DynArray *fields) {
  if (!expect_token_and_advance(parser, TOKEN_LBRACE)) {
    parser_error(parser, "Expected '{' after struct name");
    return false;
  }

  while (!current_token_is(parser, TOKEN_RBRACE) &&
         !current_token_is(parser, TOKEN_EOF)) {
    Token type_token = parser->current_token;
    if (!expect_token_and_advance(parser, TOKEN_IDENTIFIER)) {
      parser_error(parser, "Expected field type");
      return false;
    }

    Token name_token = parser->current_token;
    if (!expect_token_and_advance(parser, TOKEN_IDENTIFIER)) {
      parser_error(parser, "Expected field name");
      return false;
    }

    if (!expect_token_and_advance(parser, TOKEN_SEMICOLON)) {
      parser_error(parser, "Expected ';' after field declaration");
      return false;
    }

    if (fields->len < fields->cap) {
      StructField field = {
          .type_name = token_to_string(type_token, parser->allocator),
          .field_name = token_to_string(name_token, parser->allocator)};
      arr_append(*fields, field);
    }
  }

  if (!expect_token_and_advance(parser, TOKEN_RBRACE)) {
    parser_error(parser, "Expected '}' after struct fields");
    return false;
  }

  return true;
}

internal b32 parse_typedef_struct(Parser *parser, ReflectedStruct *out_struct) {
  // typedef struct [optional_name] { fields } struct_name;
  if (!expect_token_and_advance(parser, TOKEN_TYPEDEF)) {
    return false;
  }

  if (!expect_token_and_advance(parser, TOKEN_STRUCT)) {
    parser_error(parser, "Expected 'struct' after typedef");
    return false;
  }

  // Check if there's an optional struct name (e.g., typedef struct Point { ...
  // } Point;)
  b32 has_struct_name = current_token_is(parser, TOKEN_IDENTIFIER);
  if (has_struct_name) {
    advance_token(parser); // Skip the optional struct name
  }

  StructField_DynArray fields =
      dyn_arr_new_alloc(parser->allocator, StructField, 32);

  if (!parse_struct_fields(parser, &fields)) {
    return false;
  }

  // The typedef name comes after the closing brace
  Token typedef_name_token = parser->current_token;
  if (!expect_token_and_advance(parser, TOKEN_IDENTIFIER)) {
    parser_error(parser, "Expected typedef name after struct definition");
    return false;
  }

  if (!expect_token_and_advance(parser, TOKEN_SEMICOLON)) {
    parser_error(parser, "Expected ';' after typedef definition");
    return false;
  }

  String struct_name = token_to_string(typedef_name_token, parser->allocator);
  u32 type_id = next_type_id++;
  StructField_Array final_fields = {.items = fields.items, .len = fields.len};

  *out_struct = (ReflectedStruct){
      .struct_name = struct_name, .type_id = type_id, .fields = final_fields};

  return true;
}

internal b32 parse_regular_struct(Parser *parser, ReflectedStruct *out_struct) {
  // struct struct_name { fields };
  if (!expect_token_and_advance(parser, TOKEN_STRUCT)) {
    parser_error(parser, "Expected 'struct'");
    return false;
  }

  Token struct_name_token = parser->current_token;
  if (!expect_token_and_advance(parser, TOKEN_IDENTIFIER)) {
    parser_error(parser, "Expected struct name");
    return false;
  }

  String struct_name = token_to_string(struct_name_token, parser->allocator);

  StructField_DynArray fields =
      dyn_arr_new_alloc(parser->allocator, StructField, 32);

  if (!parse_struct_fields(parser, &fields)) {
    return false;
  }

  if (!expect_token_and_advance(parser, TOKEN_SEMICOLON)) {
    parser_error(parser, "Expected ';' after struct definition");
    return false;
  }

  u32 type_id = next_type_id++;
  StructField_Array final_fields = {.items = fields.items, .len = fields.len};

  *out_struct = (ReflectedStruct){
      .struct_name = struct_name, .type_id = type_id, .fields = final_fields};

  return true;
}

internal b32 parse_hm_reflect_struct(Parser *parser,
                                     ReflectedStruct *out_struct) {
  if (!expect_token_and_advance(parser, TOKEN_HM_REFLECT)) {
    return false;
  }

  if (!expect_token_and_advance(parser, TOKEN_LPAREN)) {
    parser_error(parser, "Expected '(' after HM_REFLECT");
    return false;
  }

  if (!expect_token_and_advance(parser, TOKEN_RPAREN)) {
    parser_error(parser, "Expected ')' after HM_REFLECT(");
    return false;
  }

  // Check what comes after HM_REFLECT(): typedef or struct
  if (current_token_is(parser, TOKEN_TYPEDEF)) {
    return parse_typedef_struct(parser, out_struct);
  } else if (current_token_is(parser, TOKEN_STRUCT)) {
    return parse_regular_struct(parser, out_struct);
  } else {
    parser_error(parser, "Expected 'typedef' or 'struct' after HM_REFLECT()");
    return false;
  }
}

Parser parser_create(const char *filename, const char *source,
                     u32 source_length, Allocator *allocator) {
  Parser parser = {
      .tokenizer = tokenizer_create(filename, source, source_length, allocator),
      .has_error = false,
      .error_message = {0},
      .allocator = allocator};

  advance_token(&parser);

  return parser;
}

b32 parse_file(Parser *parser, ReflectedStruct_DynArray *out_structs) {
  while (!current_token_is(parser, TOKEN_EOF) && !parser->has_error) {
    if (current_token_is(parser, TOKEN_HM_REFLECT)) {
      ReflectedStruct reflected_struct = {0};
      if (parse_hm_reflect_struct(parser, &reflected_struct)) {
        arr_append(*out_structs, reflected_struct);
      } else if (parser->has_error) {
        return false;
      }
    } else {
      skip_to_next_hm_reflect(parser);
    }
  }

  return !parser->has_error;
}

void parser_destroy(Parser *parser) {
  tokenizer_destroy(&parser->tokenizer);
  parser->has_error = false;
  parser->error_message = (String){0};
}
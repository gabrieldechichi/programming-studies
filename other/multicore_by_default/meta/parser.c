#include "parser.h"
#include "../lib/common.h"
#include "tokenizer.h"

global u32 next_type_id = 1;

void parser_reset_type_id() { next_type_id = 1; }

void parser_advance_token(Parser *parser) {
  parser->current_token = tokenizer_next_token(&parser->tokenizer);
}

b32 parser_expect_token_and_advance(Parser *parser, TokenType type) {
  if (parser->current_token.type == type) {
    parser_advance_token(parser);
    return true;
  }
  return false;
}

b32 parser_current_token_is(Parser *parser, TokenType type) {
  return parser->current_token.type == type;
}

void parser_error_with_context(Parser *parser, const char *message) {
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
      str_from_cstr_with_len_alloc(error_buffer, pos, parser->allocator);
}

void parser_error(Parser *parser, const char *message) {
  parser_error_with_context(parser, message);
}

void parser_skip_to_next_token_type(Parser *parser, TokenType token_type) {
  while (!parser_current_token_is(parser, TOKEN_EOF)) {
    if (parser_current_token_is(parser, token_type)) {
      break;
    }
    parser_advance_token(parser);
  }
}

Parser parser_create(const char *filename, const char *source,
                     u32 source_length, Allocator *allocator) {
  Parser parser = {
      .tokenizer = tokenizer_create(filename, source, source_length, allocator),
      .has_error = false,
      .error_message = {0},
      .allocator = allocator};

  parser_advance_token(&parser);

  return parser;
}

internal u32 parse_number_from_token(Token token) {
  u32 result = 0;
  for (u32 i = 0; i < token.length; i++) {
    char c = token.lexeme[i];
    if (char_is_digit(c)) {
      result = result * 10 + (u32)(c - '0');
    }
  }
  return result;
}

b32 try_parse_attribute(Parser *parser, MetaAttribute *attr) {

  if (parser->current_token.type != TOKEN_IDENTIFIER) {
    return false;
  }

  // Save the identifier and tokenizer state
  Token identifier_token = parser->current_token;
  Tokenizer saved_tokenizer = parser->tokenizer;

  parser_advance_token(parser);

  if (parser->current_token.type != TOKEN_LPAREN) {
    // Not an attribute pattern - restore everything
    parser->tokenizer = saved_tokenizer;
    parser->current_token = identifier_token;
    return false;
  }

  parser_advance_token(parser);

  if (parser->current_token.type != TOKEN_RPAREN) {
    parser_error(parser, "Expected ')' after '(' in attribute");
    return false;
  }

  // Successfully parsed IDENTIFIER()
  *attr = (MetaAttribute){
      .name = token_to_string(identifier_token, parser->allocator)};
  return true;
}

internal void collect_field_attributes(Parser *parser,
                                       MetaAttribute_DynArray *out_attributes) {
  // Collect attributes by looking forward from current token
  // Consume all IDENTIFIER() patterns

  while (parser->current_token.type != TOKEN_EOF) {
    if (parser->current_token.type != TOKEN_IDENTIFIER) {
      break;
    }
    MetaAttribute attr = {0};
    if (!try_parse_attribute(parser, &attr)) {
      break;
    }

    arr_append(*out_attributes, attr);

    parser_advance_token(parser); // Move past ')'
  }
}

void parser_skip_to_next_attribute(Parser *parser) {
  while (!parser_current_token_is(parser, TOKEN_EOF)) {
    // try parse attribute in the format {attr_name}()
    if (parser_current_token_is(parser, TOKEN_IDENTIFIER)) {
      Parser saved_parser = *parser;
      parser_advance_token(parser);
      if (parser_expect_token_and_advance(parser, TOKEN_LPAREN)) {
        if (parser_expect_token_and_advance(parser, TOKEN_RPAREN)) {
          // we consider an attribute if a struct or identifier comes after;
          Token cur_tok = parser->current_token;
          if (cur_tok.type == TOKEN_TYPEDEF || cur_tok.type == TOKEN_STRUCT ||
              cur_tok.type == TOKEN_IDENTIFIER) {
            *parser = saved_parser;
            return;
          }
        }
      }
    }
    parser_advance_token(parser);
  }
}


b32 parse_struct(Parser *parser, ReflectedStruct *out_struct) {
  // Collect struct-level attributes (forward scan until we hit 'typedef' or
  // 'struct')
  MetaAttribute_DynArray struct_attributes =
      dyn_arr_new_alloc(parser->allocator, MetaAttribute, 8);
  collect_field_attributes(parser, &struct_attributes);

  // Check if this is a typedef struct
  b32 has_typedef = false;
  if (parser_current_token_is(parser, TOKEN_TYPEDEF)) {
    has_typedef = true;
    parser_advance_token(parser);
  }

  // Now we should be at TOKEN_STRUCT
  if (!parser_expect_token_and_advance(parser, TOKEN_STRUCT)) {
    parser_error(parser, "Expected 'struct' keyword");
    return false;
  }

  // Parse optional struct name
  String struct_name = {0};
  if (parser_current_token_is(parser, TOKEN_IDENTIFIER)) {
    struct_name = token_to_string(parser->current_token, parser->allocator);
    parser_advance_token(parser);
  }

  // Expect opening brace
  if (!parser_expect_token_and_advance(parser, TOKEN_LBRACE)) {
    parser_error(parser, "Expected '{' after struct keyword");
    return false;
  }

  // Initialize fields array
  StructField_DynArray fields =
      dyn_arr_new_alloc(parser->allocator, StructField, 16);

  // Parse fields until closing brace
  while (!parser_current_token_is(parser, TOKEN_RBRACE) &&
         !parser_current_token_is(parser, TOKEN_EOF)) {
    // Collect field-level attributes
    MetaAttribute_DynArray field_attributes =
        dyn_arr_new_alloc(parser->allocator, MetaAttribute, 8);
    collect_field_attributes(parser, &field_attributes);

    // Parse type name
    if (!parser_current_token_is(parser, TOKEN_IDENTIFIER)) {
      parser_error(parser, "Expected type name for struct field");
      return false;
    }
    String type_name =
        token_to_string(parser->current_token, parser->allocator);
    parser_advance_token(parser);

    // Count pointer depth
    u32 pointer_depth = 0;
    while (parser_current_token_is(parser, TOKEN_ASTERISK)) {
      pointer_depth++;
      parser_advance_token(parser);
    }

    // Parse field name
    if (!parser_current_token_is(parser, TOKEN_IDENTIFIER)) {
      parser_error(parser, "Expected field name after type");
      return false;
    }
    String field_name =
        token_to_string(parser->current_token, parser->allocator);
    parser_advance_token(parser);

    // Check for array syntax
    b32 is_array = false;
    u32 array_size = 0;
    if (parser_current_token_is(parser, TOKEN_LBRACKET)) {
      is_array = true;
      parser_advance_token(parser);

      // Parse array size (must be present)
      if (!parser_current_token_is(parser, TOKEN_NUMBER)) {
        parser_error(parser, "Expected number in array size");
        return false;
      }
      array_size = parse_number_from_token(parser->current_token);
      parser_advance_token(parser);

      // Expect closing bracket
      if (!parser_expect_token_and_advance(parser, TOKEN_RBRACKET)) {
        parser_error(parser, "Expected ']' in array declaration");
        return false;
      }
    }

    // Expect semicolon
    if (!parser_expect_token_and_advance(parser, TOKEN_SEMICOLON)) {
      parser_error(parser, "Expected ';' after struct field");
      return false;
    }

    // Add field to array
    StructField field = {.type_name = type_name,
                         .field_name = field_name,
                         .pointer_depth = pointer_depth,
                         .is_array = is_array,
                         .array_size = array_size,
                         .attributes = field_attributes};
    arr_append(fields, field);
  }

  // Expect closing brace
  if (!parser_expect_token_and_advance(parser, TOKEN_RBRACE)) {
    parser_error(parser, "Expected '}' at end of struct");
    return false;
  }

  // Parse typedef name if this is a typedef struct
  String typedef_name = {0};
  if (has_typedef) {
    if (!parser_current_token_is(parser, TOKEN_IDENTIFIER)) {
      parser_error(parser, "Expected typedef name after '}'");
      return false;
    }
    typedef_name = token_to_string(parser->current_token, parser->allocator);
    parser_advance_token(parser);

    // Expect semicolon after typedef name
    if (!parser_expect_token_and_advance(parser, TOKEN_SEMICOLON)) {
      parser_error(parser, "Expected ';' after typedef name");
      return false;
    }
  }

  // Populate output struct
  out_struct->struct_name = struct_name;
  out_struct->typedef_name = typedef_name;
  out_struct->type_id = next_type_id++;
  out_struct->attributes = struct_attributes;
  out_struct->fields = fields;

  return true;
}

void parser_destroy(Parser *parser) {
  tokenizer_destroy(&parser->tokenizer);
  parser->has_error = false;
  parser->error_message = (String){0};
}
#include "lib/string.c"
#include "meta/tokenizer.c"
#include "meta/parser.c"
#include "lib/common.c"
#include "lib/memory.c"
#include "lib/handle.c"
#include "lib/json_parser.c"
#include "lib/json_serializer.c"
#include "lib/yaml_parser.c"
#include "lib/cmd_line.c"
#include "os/os.c"

#include "tokenizer_test.c"
#include "parser_test.c"

int main() {
  u8 buffer[4096];
  ArenaAllocator arena = arena_from_buffer(buffer, sizeof(buffer));

  TestContext ctx = {0};
  ctx.allocator = make_arena_allocator(&arena);

  // Tokenizer tests
  RUN_TEST(test_basic_tokens, &ctx);
  RUN_TEST(test_skip_comments, &ctx);
  RUN_TEST(test_typedef_keyword, &ctx);
  RUN_TEST(test_identifier_with_parens, &ctx);
  RUN_TEST(test_multiline_with_line_tracking, &ctx);
  RUN_TEST(test_invalid_character, &ctx);
  RUN_TEST(test_asterisk_token, &ctx);
  RUN_TEST(test_multiple_asterisks, &ctx);

  // Parser basic tests
  RUN_TEST(test_parse_struct_basic, &ctx);
  RUN_TEST(test_parse_struct_empty, &ctx);
  RUN_TEST(test_parse_struct_anonymous, &ctx);

  // Parser typedef tests
  RUN_TEST(test_parse_typedef_named_same, &ctx);
  RUN_TEST(test_parse_typedef_anonymous, &ctx);
  RUN_TEST(test_parse_typedef_different_names, &ctx);

  // Parser attribute tests
  RUN_TEST(test_parse_struct_with_struct_attributes, &ctx);
  RUN_TEST(test_parse_struct_with_field_attributes, &ctx);
  RUN_TEST(test_parse_struct_with_multiple_attributes, &ctx);
  RUN_TEST(test_parse_struct_no_attributes, &ctx);

  // Parser error handling tests
  RUN_TEST(test_parse_struct_error_missing_semicolon, &ctx);
  RUN_TEST(test_parse_struct_error_missing_closing_brace, &ctx);
  RUN_TEST(test_parse_typedef_error_missing_name, &ctx);
  RUN_TEST(test_parse_struct_error_missing_field_name, &ctx);
  RUN_TEST(test_parse_error_message_format, &ctx);

  // Parser pointer tests
  RUN_TEST(test_parse_struct_with_pointer, &ctx);
  RUN_TEST(test_parse_struct_with_double_pointer, &ctx);
  RUN_TEST(test_parse_struct_with_triple_pointer, &ctx);
  RUN_TEST(test_parse_struct_mixed_pointers, &ctx);

  // Parser array tests
  RUN_TEST(test_parse_struct_with_fixed_array, &ctx);
  RUN_TEST(test_parse_struct_with_larger_array, &ctx);
  RUN_TEST(test_parse_struct_with_multiple_arrays, &ctx);
  RUN_TEST(test_parse_struct_error_missing_array_size, &ctx);
  RUN_TEST(test_parse_struct_error_missing_closing_bracket, &ctx);

  // Parser comprehensive test
  RUN_TEST(test_parse_struct_comprehensive, &ctx);

  print_test_results();
  return 0;
}
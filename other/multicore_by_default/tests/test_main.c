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
  
  RUN_TEST(test_basic_tokens, &ctx);
  RUN_TEST(test_skip_comments, &ctx);
  RUN_TEST(test_hm_reflect_struct, &ctx);

  RUN_TEST(test_parse_single_struct, &ctx);
  RUN_TEST(test_parse_multiple_structs, &ctx);
  RUN_TEST(test_parse_empty_struct, &ctx);
  RUN_TEST(test_parse_malformed_struct, &ctx);
  RUN_TEST(test_parse_error_message_format, &ctx);
  RUN_TEST(test_enhanced_error_reporting_showcase, &ctx);

  RUN_TEST(test_parse_typedef_struct_simple, &ctx);
  RUN_TEST(test_parse_typedef_struct_with_name, &ctx);
  RUN_TEST(test_parse_mixed_struct_and_typedef, &ctx);
  RUN_TEST(test_parse_typedef_error_missing_name, &ctx);
  RUN_TEST(test_typedef_struct_showcase, &ctx);

  print_test_results();
  return 0;
}
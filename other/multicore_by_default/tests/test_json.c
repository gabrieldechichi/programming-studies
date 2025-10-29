#include "lib/json_parser.h"
#include "lib/json_serializer.h"
#include "lib/memory.h"
#include "lib/string.h"
#include "lib/test.h"

void test_json_serializer_basic(TestContext* ctx) {
  Allocator* allocator = &ctx->allocator;

  JsonSerializer serializer = json_serializer_init(allocator, 256);

  write_object_start(&serializer);
  write_key(&serializer, "name");
  serialize_string_value(&serializer, "test");
  write_comma(&serializer);
  write_key(&serializer, "value");
  serialize_number_value(&serializer, 42.0);
  write_object_end(&serializer);

  char *result = json_serializer_finalize(&serializer);
  assert_str_eq(result, "{\"name\":\"test\",\"value\":42}");
}

void test_json_parser_basic(TestContext* ctx) {
  Allocator* allocator = &ctx->allocator;

  const char *json_input = "{\"name\":\"test\",\"value\":42}";
  JsonParser parser = json_parser_init(json_input, allocator);

  json_expect_object_start(&parser);
  char *key1 = json_expect_key(&parser, "name");
  assert_str_eq(key1, "name");
  json_expect_colon(&parser);
  char *name_value = json_parse_string_value(&parser);
  assert_str_eq(name_value, "test");

  json_expect_comma(&parser);
  char *key2 = json_expect_key(&parser, "value");
  assert_str_eq(key2, "value");
  json_expect_colon(&parser);
  double number_value = json_parse_number_value(&parser);
  assert_eq((u32)number_value, 42);

  json_expect_object_end(&parser);
  assert_true(json_is_at_end(&parser));
}
#include "../meta/parser.h"
#include "../lib/string.h"
#include "../lib/test.h"

// ============================================================================
// Basic Parsing Tests
// ============================================================================

void test_parse_struct_basic(TestContext* ctx) {
  parser_reset_type_id();
  Allocator* allocator = &ctx->allocator;

  const char *source = "struct Point { int x; int y; }";
  Parser parser = parser_create("test.h", source, str_len(source), allocator);

  ReflectedStruct s = {0};
  b32 success = parse_struct(&parser, &s);

  assert_true(success);
  assert_eq(s.type_id, 1);
  assert_str_eq(s.struct_name.value, "Point");
  assert_eq(s.typedef_name.len, 0); // No typedef
  assert_eq(s.fields.len, 2);

  StructField x_field = s.fields.items[0];
  assert_str_eq(x_field.type_name.value, "int");
  assert_str_eq(x_field.field_name.value, "x");
  assert_eq(x_field.attributes.len, 0);

  StructField y_field = s.fields.items[1];
  assert_str_eq(y_field.type_name.value, "int");
  assert_str_eq(y_field.field_name.value, "y");

  parser_destroy(&parser);
}

void test_parse_struct_empty(TestContext* ctx) {
  parser_reset_type_id();
  Allocator* allocator = &ctx->allocator;

  const char *source = "struct Empty {}";
  Parser parser = parser_create("test.h", source, str_len(source), allocator);

  ReflectedStruct s = {0};
  b32 success = parse_struct(&parser, &s);

  assert_true(success);
  assert_str_eq(s.struct_name.value, "Empty");
  assert_eq(s.fields.len, 0);

  parser_destroy(&parser);
}

void test_parse_struct_anonymous(TestContext* ctx) {
  parser_reset_type_id();
  Allocator* allocator = &ctx->allocator;

  const char *source = "struct { int value; }";
  Parser parser = parser_create("test.h", source, str_len(source), allocator);

  ReflectedStruct s = {0};
  b32 success = parse_struct(&parser, &s);

  assert_true(success);
  assert_eq(s.struct_name.len, 0); // Anonymous struct
  assert_eq(s.fields.len, 1);
  assert_str_eq(s.fields.items[0].field_name.value, "value");

  parser_destroy(&parser);
}

// ============================================================================
// Typedef Tests
// ============================================================================

void test_parse_typedef_named_same(TestContext* ctx) {
  parser_reset_type_id();
  Allocator* allocator = &ctx->allocator;

  const char *source = "typedef struct Point { int x; int y; } Point;";
  Parser parser = parser_create("test.h", source, str_len(source), allocator);

  ReflectedStruct s = {0};
  b32 success = parse_struct(&parser, &s);

  assert_true(success);
  assert_str_eq(s.struct_name.value, "Point");
  assert_str_eq(s.typedef_name.value, "Point");
  assert_eq(s.fields.len, 2);

  parser_destroy(&parser);
}

void test_parse_typedef_anonymous(TestContext* ctx) {
  parser_reset_type_id();
  Allocator* allocator = &ctx->allocator;

  const char *source = "typedef struct { float x; float y; } Vector2D;";
  Parser parser = parser_create("test.h", source, str_len(source), allocator);

  ReflectedStruct s = {0};
  b32 success = parse_struct(&parser, &s);

  assert_true(success);
  assert_eq(s.struct_name.len, 0); // Anonymous struct
  assert_str_eq(s.typedef_name.value, "Vector2D");
  assert_eq(s.fields.len, 2);

  parser_destroy(&parser);
}

void test_parse_typedef_different_names(TestContext* ctx) {
  parser_reset_type_id();
  Allocator* allocator = &ctx->allocator;

  const char *source = "typedef struct Point { int x; int y; } Point2D;";
  Parser parser = parser_create("test.h", source, str_len(source), allocator);

  ReflectedStruct s = {0};
  b32 success = parse_struct(&parser, &s);

  assert_true(success);
  assert_str_eq(s.struct_name.value, "Point");
  assert_str_eq(s.typedef_name.value, "Point2D");
  assert_eq(s.fields.len, 2);

  parser_destroy(&parser);
}

// ============================================================================
// Attribute Tests
// ============================================================================

void test_parse_struct_with_struct_attributes(TestContext* ctx) {
  parser_reset_type_id();
  Allocator* allocator = &ctx->allocator;

  const char *source = "HZ_TASK() struct TaskData { u64 value; }";
  Parser parser = parser_create("test.h", source, str_len(source), allocator);

  ReflectedStruct s = {0};
  b32 success = parse_struct(&parser, &s);

  assert_true(success);
  assert_str_eq(s.struct_name.value, "TaskData");
  assert_eq(s.attributes.len, 1);
  assert_str_eq(s.attributes.items[0].name.value, "HZ_TASK");

  parser_destroy(&parser);
}

void test_parse_struct_with_field_attributes(TestContext* ctx) {
  parser_reset_type_id();
  Allocator* allocator = &ctx->allocator;

  const char *source = "struct Data { HZ_READ() u64 input; HZ_WRITE() u64 output; }";
  Parser parser = parser_create("test.h", source, str_len(source), allocator);

  ReflectedStruct s = {0};
  b32 success = parse_struct(&parser, &s);

  assert_true(success);
  assert_eq(s.fields.len, 2);

  StructField input_field = s.fields.items[0];
  assert_str_eq(input_field.field_name.value, "input");
  assert_eq(input_field.attributes.len, 1);
  assert_str_eq(input_field.attributes.items[0].name.value, "HZ_READ");

  StructField output_field = s.fields.items[1];
  assert_str_eq(output_field.field_name.value, "output");
  assert_eq(output_field.attributes.len, 1);
  assert_str_eq(output_field.attributes.items[0].name.value, "HZ_WRITE");

  parser_destroy(&parser);
}

void test_parse_struct_with_multiple_attributes(TestContext* ctx) {
  parser_reset_type_id();
  Allocator* allocator = &ctx->allocator;

  const char *source = "HM_REFLECT() HZ_TASK() typedef struct TaskData { HZ_READ() HZ_ATOMIC() u64 counter; } TaskData;";
  Parser parser = parser_create("test.h", source, str_len(source), allocator);

  ReflectedStruct s = {0};
  b32 success = parse_struct(&parser, &s);

  assert_true(success);
  assert_eq(s.attributes.len, 2);
  assert_str_eq(s.attributes.items[0].name.value, "HM_REFLECT");
  assert_str_eq(s.attributes.items[1].name.value, "HZ_TASK");

  assert_eq(s.fields.len, 1);
  StructField counter_field = s.fields.items[0];
  assert_eq(counter_field.attributes.len, 2);
  assert_str_eq(counter_field.attributes.items[0].name.value, "HZ_READ");
  assert_str_eq(counter_field.attributes.items[1].name.value, "HZ_ATOMIC");

  parser_destroy(&parser);
}

void test_parse_struct_no_attributes(TestContext* ctx) {
  parser_reset_type_id();
  Allocator* allocator = &ctx->allocator;

  const char *source = "struct Plain { int x; }";
  Parser parser = parser_create("test.h", source, str_len(source), allocator);

  ReflectedStruct s = {0};
  b32 success = parse_struct(&parser, &s);

  assert_true(success);
  assert_eq(s.attributes.len, 0);
  assert_eq(s.fields.items[0].attributes.len, 0);

  parser_destroy(&parser);
}

// ============================================================================
// Error Handling Tests
// ============================================================================

void test_parse_struct_error_missing_semicolon(TestContext* ctx) {
  parser_reset_type_id();
  Allocator* allocator = &ctx->allocator;

  const char *source = "struct Broken { int field }";
  Parser parser = parser_create("test.h", source, str_len(source), allocator);

  ReflectedStruct s = {0};
  b32 success = parse_struct(&parser, &s);

  assert_false(success);
  assert_true(parser.has_error);
  assert_true(str_contains(parser.error_message.value, "Expected ';'"));

  parser_destroy(&parser);
}

void test_parse_struct_error_missing_closing_brace(TestContext* ctx) {
  parser_reset_type_id();
  Allocator* allocator = &ctx->allocator;

  const char *source = "struct Broken { int field;";
  Parser parser = parser_create("test.h", source, str_len(source), allocator);

  ReflectedStruct s = {0};
  b32 success = parse_struct(&parser, &s);

  assert_false(success);
  assert_true(parser.has_error);
  assert_true(str_contains(parser.error_message.value, "Expected '}'"));

  parser_destroy(&parser);
}

void test_parse_typedef_error_missing_name(TestContext* ctx) {
  parser_reset_type_id();
  Allocator* allocator = &ctx->allocator;

  const char *source = "typedef struct { int x; };";
  Parser parser = parser_create("test.h", source, str_len(source), allocator);

  ReflectedStruct s = {0};
  b32 success = parse_struct(&parser, &s);

  assert_false(success);
  assert_true(parser.has_error);
  assert_true(str_contains(parser.error_message.value, "Expected typedef name"));

  parser_destroy(&parser);
}

void test_parse_struct_error_missing_field_name(TestContext* ctx) {
  parser_reset_type_id();
  Allocator* allocator = &ctx->allocator;

  const char *source = "struct Broken { int; }";
  Parser parser = parser_create("test.h", source, str_len(source), allocator);

  ReflectedStruct s = {0};
  b32 success = parse_struct(&parser, &s);

  assert_false(success);
  assert_true(parser.has_error);
  assert_true(str_contains(parser.error_message.value, "Expected field name"));

  parser_destroy(&parser);
}

void test_parse_error_message_format(TestContext* ctx) {
  parser_reset_type_id();
  Allocator* allocator = &ctx->allocator;

  const char *source = "struct Broken\n{\n  int field\n}";
  Parser parser = parser_create("multiline.h", source, str_len(source), allocator);

  ReflectedStruct s = {0};
  b32 success = parse_struct(&parser, &s);

  assert_false(success);
  assert_true(parser.has_error);
  assert_true(str_contains(parser.error_message.value, "Error in file"));
  assert_true(str_contains(parser.error_message.value, "multiline.h"));
  assert_true(str_contains(parser.error_message.value, "line"));
  assert_true(str_contains(parser.error_message.value, "column"));
  assert_true(str_contains(parser.error_message.value, "^"));

  parser_destroy(&parser);
}

// ============================================================================
// Complex Integration Tests
// ============================================================================

void test_parse_struct_comprehensive(TestContext* ctx) {
  parser_reset_type_id();
  Allocator* allocator = &ctx->allocator;

  const char *source =
    "HM_REFLECT() HZ_TASK() typedef struct TaskWideSum {\n"
    "    HZ_READ() u64 values_start;\n"
    "    HZ_READ() u64 values_end;\n"
    "    HZ_WRITE() i64 result;\n"
    "} TaskWideSum;";

  Parser parser = parser_create("comprehensive.h", source, str_len(source), allocator);

  ReflectedStruct s = {0};
  b32 success = parse_struct(&parser, &s);

  assert_true(success);

  // Verify struct metadata
  assert_str_eq(s.struct_name.value, "TaskWideSum");
  assert_str_eq(s.typedef_name.value, "TaskWideSum");
  assert_eq(s.type_id, 1);

  // Verify struct attributes
  assert_eq(s.attributes.len, 2);
  assert_str_eq(s.attributes.items[0].name.value, "HM_REFLECT");
  assert_str_eq(s.attributes.items[1].name.value, "HZ_TASK");

  // Verify fields
  assert_eq(s.fields.len, 3);

  StructField field1 = s.fields.items[0];
  assert_str_eq(field1.type_name.value, "u64");
  assert_str_eq(field1.field_name.value, "values_start");
  assert_eq(field1.attributes.len, 1);
  assert_str_eq(field1.attributes.items[0].name.value, "HZ_READ");

  StructField field2 = s.fields.items[1];
  assert_str_eq(field2.type_name.value, "u64");
  assert_str_eq(field2.field_name.value, "values_end");
  assert_eq(field2.attributes.len, 1);
  assert_str_eq(field2.attributes.items[0].name.value, "HZ_READ");

  StructField field3 = s.fields.items[2];
  assert_str_eq(field3.type_name.value, "i64");
  assert_str_eq(field3.field_name.value, "result");
  assert_eq(field3.attributes.len, 1);
  assert_str_eq(field3.attributes.items[0].name.value, "HZ_WRITE");

  parser_destroy(&parser);
}

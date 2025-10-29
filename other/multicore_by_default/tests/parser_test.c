#include "../meta/parser.h"
#include "../lib/string.h"
#include "../lib/test.h"

void test_parse_single_struct(TestContext* ctx) {
  parser_reset_type_id();
  Allocator* allocator = &ctx->allocator;

  const char *source =
      "HM_REFLECT() struct Player { int health; float speed; };";
  Parser parser = parser_create("player.h", source, str_len(source), allocator);

  ReflectedStruct *struct_buffer = ALLOC_ARRAY(allocator, ReflectedStruct, 4);
  ReflectedStruct_DynArray structs;
  structs.items = struct_buffer;
  structs.cap = 4;
  structs.len = 0;

  b32 success = parse_file(&parser, &structs);
  assert_true(success);
  assert_eq(structs.len, 1);

  ReflectedStruct player_struct = structs.items[0];
  assert_eq(player_struct.struct_name.len, 6);
  assert_str_eq(player_struct.struct_name.value, "Player");
  assert_eq(player_struct.type_id, 1);
  assert_eq(player_struct.fields.len, 2);

  StructField health_field = player_struct.fields.items[0];
  assert_str_eq(health_field.type_name.value, "int");
  assert_str_eq(health_field.field_name.value, "health");

  StructField speed_field = player_struct.fields.items[1];
  assert_str_eq(speed_field.type_name.value, "float");
  assert_str_eq(speed_field.field_name.value, "speed");

  parser_destroy(&parser);
}

void test_parse_multiple_structs(TestContext* ctx) {
  parser_reset_type_id();
  Allocator* allocator = &ctx->allocator;

  const char *source =
      "HM_REFLECT() struct Player { int health; };\n"
      "struct NonReflected { int ignored; };\n"
      "HM_REFLECT() struct Enemy { float damage; int level; };";

  Parser parser = parser_create("entities.h", source, str_len(source), allocator);

  ReflectedStruct *struct_buffer = ALLOC_ARRAY(allocator, ReflectedStruct, 4);
  ReflectedStruct_DynArray structs;
  structs.items = struct_buffer;
  structs.cap = 4;
  structs.len = 0;

  b32 success = parse_file(&parser, &structs);
  assert_true(success);
  assert_eq(structs.len, 2);

  ReflectedStruct player_struct = structs.items[0];
  assert_str_eq(player_struct.struct_name.value, "Player");
  assert_eq(player_struct.type_id, 1);
  assert_eq(player_struct.fields.len, 1);

  ReflectedStruct enemy_struct = structs.items[1];
  assert_str_eq(enemy_struct.struct_name.value, "Enemy");
  assert_eq(enemy_struct.type_id, 2);
  assert_eq(enemy_struct.fields.len, 2);

  parser_destroy(&parser);
}

void test_parse_empty_struct(TestContext* ctx) {
  parser_reset_type_id();
  Allocator* allocator = &ctx->allocator;

  const char *source = "HM_REFLECT() struct Empty { };";
  Parser parser = parser_create("empty.h", source, str_len(source), allocator);

  ReflectedStruct *struct_buffer = ALLOC_ARRAY(allocator, ReflectedStruct, 4);
  ReflectedStruct_DynArray structs;
  structs.items = struct_buffer;
  structs.cap = 4;
  structs.len = 0;

  b32 success = parse_file(&parser, &structs);
  assert_true(success);
  assert_eq(structs.len, 1);

  ReflectedStruct empty_struct = structs.items[0];
  assert_str_eq(empty_struct.struct_name.value, "Empty");
  assert_eq(empty_struct.fields.len, 0);

  parser_destroy(&parser);
}

void test_parse_malformed_struct(TestContext* ctx) {
  parser_reset_type_id();
  Allocator* allocator = &ctx->allocator;

  const char *source = "HM_REFLECT() struct Broken { int field }";
  Parser parser = parser_create("broken.h", source, str_len(source), allocator);

  ReflectedStruct *struct_buffer = ALLOC_ARRAY(allocator, ReflectedStruct, 4);
  ReflectedStruct_DynArray structs;
  structs.items = struct_buffer;
  structs.cap = 4;
  structs.len = 0;

  b32 success = parse_file(&parser, &structs);
  assert_false(success);
  assert_true(parser.has_error);

  parser_destroy(&parser);
}

void test_parse_error_message_format(TestContext* ctx) {
  parser_reset_type_id();
  Allocator* allocator = &ctx->allocator;

  const char *source = "HM_REFLECT()\nstruct Broken\n{\n  int field\n}";
  Parser parser = parser_create("multiline.h", source, str_len(source), allocator);

  ReflectedStruct *struct_buffer = ALLOC_ARRAY(allocator, ReflectedStruct, 4);
  ReflectedStruct_DynArray structs;
  structs.items = struct_buffer;
  structs.cap = 4;
  structs.len = 0;

  b32 success = parse_file(&parser, &structs);
  assert_false(success);
  assert_true(parser.has_error);
  assert_true(str_contains(parser.error_message.value, "Error in file"));
  assert_true(str_contains(parser.error_message.value, "multiline.h"));
  assert_true(str_contains(parser.error_message.value, "line"));
  assert_true(str_contains(parser.error_message.value, "column"));
  LOG_INFO(parser.error_message.value);

  parser_destroy(&parser);
}

void test_enhanced_error_reporting_showcase(TestContext* ctx) {
  parser_reset_type_id();
  Allocator* allocator = &ctx->allocator;

  const char *source = 
    "// This file demonstrates enhanced error reporting\n"
    "HM_REFLECT() struct GameEntity {\n"
    "    float position_x;\n"
    "    float position_y  // Missing semicolon here!\n"
    "    int health;\n"
    "};";
    
  Parser parser = parser_create("game_entity.h", source, str_len(source), allocator);

  ReflectedStruct *struct_buffer = ALLOC_ARRAY(allocator, ReflectedStruct, 4);
  ReflectedStruct_DynArray structs;
  structs.items = struct_buffer;
  structs.cap = 4;
  structs.len = 0;

  b32 success = parse_file(&parser, &structs);
  assert_false(success);
  assert_true(parser.has_error);
  
  LOG_INFO("=== Enhanced Error Reporting Showcase ===");
  LOG_INFO(parser.error_message.value);
  LOG_INFO("=== End Showcase ===");
  
  // Verify the enhanced error message contains all expected elements
  assert_true(str_contains(parser.error_message.value, "Error in file 'game_entity.h'"));
  assert_true(str_contains(parser.error_message.value, "line"));
  assert_true(str_contains(parser.error_message.value, "column"));
  assert_true(str_contains(parser.error_message.value, "^"));
  assert_true(str_contains(parser.error_message.value, "Expected"));
  
  parser_destroy(&parser);
}

void test_parse_typedef_struct_simple(TestContext* ctx) {
  parser_reset_type_id();
  Allocator* allocator = &ctx->allocator;

  const char *source = "HM_REFLECT() typedef struct { float x; float y; } Point;";
  Parser parser = parser_create("point.h", source, str_len(source), allocator);

  ReflectedStruct *struct_buffer = ALLOC_ARRAY(allocator, ReflectedStruct, 4);
  ReflectedStruct_DynArray structs;
  structs.items = struct_buffer;
  structs.cap = 4;
  structs.len = 0;

  b32 success = parse_file(&parser, &structs);
  assert_true(success);
  assert_eq(structs.len, 1);

  ReflectedStruct point_struct = structs.items[0];
  assert_str_eq(point_struct.struct_name.value, "Point");
  assert_eq(point_struct.type_id, 1);
  assert_eq(point_struct.fields.len, 2);

  StructField x_field = point_struct.fields.items[0];
  assert_str_eq(x_field.type_name.value, "float");
  assert_str_eq(x_field.field_name.value, "x");

  StructField y_field = point_struct.fields.items[1];
  assert_str_eq(y_field.type_name.value, "float");
  assert_str_eq(y_field.field_name.value, "y");

  parser_destroy(&parser);
}

void test_parse_typedef_struct_with_name(TestContext* ctx) {
  parser_reset_type_id();
  Allocator* allocator = &ctx->allocator;

  const char *source = "HM_REFLECT() typedef struct Vector { float x; float y; float z; } Vector;";
  Parser parser = parser_create("vector.h", source, str_len(source), allocator);

  ReflectedStruct *struct_buffer = ALLOC_ARRAY(allocator, ReflectedStruct, 4);
  ReflectedStruct_DynArray structs;
  structs.items = struct_buffer;
  structs.cap = 4;
  structs.len = 0;

  b32 success = parse_file(&parser, &structs);
  assert_true(success);
  assert_eq(structs.len, 1);

  ReflectedStruct vector_struct = structs.items[0];
  assert_str_eq(vector_struct.struct_name.value, "Vector");
  assert_eq(vector_struct.type_id, 1);
  assert_eq(vector_struct.fields.len, 3);

  parser_destroy(&parser);
}

void test_parse_mixed_struct_and_typedef(TestContext* ctx) {
  parser_reset_type_id();
  Allocator* allocator = &ctx->allocator;

  const char *source = 
    "HM_REFLECT() struct Player { int health; };\n"
    "HM_REFLECT() typedef struct { float x; float y; } Position;\n"
    "HM_REFLECT() typedef struct Enemy { int damage; } Enemy;";

  Parser parser = parser_create("mixed.h", source, str_len(source), allocator);

  ReflectedStruct *struct_buffer = ALLOC_ARRAY(allocator, ReflectedStruct, 4);
  ReflectedStruct_DynArray structs;
  structs.items = struct_buffer;
  structs.cap = 4;
  structs.len = 0;

  b32 success = parse_file(&parser, &structs);
  assert_true(success);
  assert_eq(structs.len, 3);

  // Regular struct
  ReflectedStruct player_struct = structs.items[0];
  assert_str_eq(player_struct.struct_name.value, "Player");
  assert_eq(player_struct.type_id, 1);
  assert_eq(player_struct.fields.len, 1);

  // Typedef without struct name
  ReflectedStruct position_struct = structs.items[1];
  assert_str_eq(position_struct.struct_name.value, "Position");
  assert_eq(position_struct.type_id, 2);
  assert_eq(position_struct.fields.len, 2);

  // Typedef with struct name
  ReflectedStruct enemy_struct = structs.items[2];
  assert_str_eq(enemy_struct.struct_name.value, "Enemy");
  assert_eq(enemy_struct.type_id, 3);
  assert_eq(enemy_struct.fields.len, 1);

  parser_destroy(&parser);
}

void test_parse_typedef_error_missing_name(TestContext* ctx) {
  parser_reset_type_id();
  Allocator* allocator = &ctx->allocator;

  const char *source = "HM_REFLECT() typedef struct { int x; };"; // Missing typedef name
  Parser parser = parser_create("error.h", source, str_len(source), allocator);

  ReflectedStruct *struct_buffer = ALLOC_ARRAY(allocator, ReflectedStruct, 4);
  ReflectedStruct_DynArray structs;
  structs.items = struct_buffer;
  structs.cap = 4;
  structs.len = 0;

  b32 success = parse_file(&parser, &structs);
  assert_false(success);
  assert_true(parser.has_error);
  assert_true(str_contains(parser.error_message.value, "Expected typedef name"));

  parser_destroy(&parser);
}

void test_typedef_struct_showcase(TestContext* ctx) {
  parser_reset_type_id();
  Allocator* allocator = &ctx->allocator;

  const char *source = 
    "// Showcase of both struct patterns supported\n"
    "HM_REFLECT() struct GameEntity {\n"
    "    float x;\n"
    "    float y;\n"
    "    float z;\n"
    "    int health;\n"
    "};\n"
    "\n"
    "HM_REFLECT() typedef struct {\n"
    "    float x;\n"
    "    float y;\n"
    "} Vector2D;\n"
    "\n"
    "HM_REFLECT() typedef struct Transform {\n"
    "    float position_x;\n"
    "    float rotation_w;\n"
    "} Transform;";

  Parser parser = parser_create("showcase.h", source, str_len(source), allocator);

  ReflectedStruct *struct_buffer = ALLOC_ARRAY(allocator, ReflectedStruct, 4);
  ReflectedStruct_DynArray structs;
  structs.items = struct_buffer;
  structs.cap = 4;
  structs.len = 0;

  b32 success = parse_file(&parser, &structs);
  assert_true(success);
  assert_eq(structs.len, 3);

  LOG_INFO("=== Typedef Struct Support Showcase ===");
  for (u32 i = 0; i < structs.len; i++) {
    ReflectedStruct s = structs.items[i];
    LOG_INFO("Found struct: % (ID: %, Fields: %)", 
             FMT_STR(s.struct_name.value),
             FMT_UINT(s.type_id), 
             FMT_UINT(s.fields.len));
  }
  LOG_INFO("=== Successfully parsed regular struct + typedef variants ===");

  // Verify all three structs were parsed correctly
  assert_str_eq(structs.items[0].struct_name.value, "GameEntity");
  assert_eq(structs.items[0].fields.len, 4);
  assert_str_eq(structs.items[1].struct_name.value, "Vector2D"); 
  assert_eq(structs.items[1].fields.len, 2);
  assert_str_eq(structs.items[2].struct_name.value, "Transform");
  assert_eq(structs.items[2].fields.len, 2);

  parser_destroy(&parser);
}
#include "lib/yaml_parser.h"
#include "lib/memory.h"
#include "lib/string.h"
#include "lib/test.h"

// Simple test structure
typedef struct {
  struct {
    double role;
    const char *content;
  } response;
  const char *foo;
} SimpleYamlStruct;

// Type-safe parser for SimpleYamlStruct
internal SimpleYamlStruct yaml_parse_SimpleYamlStruct(const char *yaml_str, Allocator *arena) {
  SimpleYamlStruct result = {0};
  YamlParser parser = yaml_parser_init(yaml_str, arena);
  
  // Parse: response:
  yaml_expect_key(&parser, "response");
  yaml_push_indent(&parser);
  
  // Parse: role: 42
  yaml_expect_key(&parser, "role");
  result.response.role = yaml_parse_number_value(&parser);
  
  // Parse: content: "Hello World"
  yaml_expect_key(&parser, "content");
  result.response.content = yaml_parse_string_value(&parser);
  
  yaml_pop_indent(&parser);
  
  // Parse: foo: bar
  yaml_expect_key(&parser, "foo");
  result.foo = yaml_parse_string_value(&parser);
  
  assert_true(yaml_is_at_end(&parser));
  
  return result;
}

// Shader structure for present_reflection.yaml
typedef struct {
  const char *path;
  bool32 is_binary;
  const char *entry_point;
} ShaderFunc;

typedef struct {
  uint32 slot;
  const char *type;
  const char *base_type;
} ShaderAttr;

typedef struct {
  uint32 slot;
  const char *stage;
  const char *name;
  bool32 multisampled;
  const char *type;
  const char *sample_type;
  uint32 msl_texture_n;
} ShaderTexture;

typedef struct {
  uint32 slot;
  const char *stage;
  const char *name;
  const char *sampler_type;
  uint32 msl_sampler_n;
} ShaderSampler;

typedef struct {
  uint32 slot;
  const char *stage;
  const char *name;
  const char *view_name;
  const char *sampler_name;
  uint32 view_slot;
  uint32 sampler_slot;
} TextureSamplerPair;

typedef struct {
  const char *name;
  ShaderFunc vertex_func;
  ShaderFunc fragment_func;
  ShaderAttr *attrs;
  uint32 attr_count;
  ShaderTexture *views;
  uint32 view_count;
  ShaderSampler *samplers;
  uint32 sampler_count;
  TextureSamplerPair *texture_sampler_pairs;
  uint32 pair_count;
} ShaderProgram;

typedef struct {
  const char *slang;
  ShaderProgram *programs;
  uint32 program_count;
} ShaderConfig;

typedef struct {
  ShaderConfig *shaders;
  uint32 shader_count;
} ShaderRoot;

// Parser for shader function
internal ShaderFunc yaml_parse_ShaderFunc(YamlParser *parser, Allocator *arena) {
  ShaderFunc func = {0};
  
  yaml_push_indent(parser);
  
  yaml_expect_key(parser, "path");
  func.path = yaml_parse_string_value(parser);
  
  yaml_expect_key(parser, "is_binary");
  func.is_binary = yaml_parse_bool_value(parser);
  
  yaml_expect_key(parser, "entry_point");
  func.entry_point = yaml_parse_string_value(parser);
  
  yaml_pop_indent(parser);
  
  return func;
}

// Parser for shader attribute
internal ShaderAttr yaml_parse_ShaderAttr(YamlParser *parser, Allocator *arena) {
  ShaderAttr attr = {0};
  
  yaml_push_indent(parser);
  
  yaml_expect_key(parser, "slot");
  attr.slot = (uint32)yaml_parse_number_value(parser);
  
  yaml_expect_key(parser, "type");
  attr.type = yaml_parse_string_value(parser);
  
  yaml_expect_key(parser, "base_type");
  attr.base_type = yaml_parse_string_value(parser);
  
  yaml_pop_indent(parser);
  
  return attr;
}

// Parser for shader texture view
internal ShaderTexture yaml_parse_ShaderTexture(YamlParser *parser, Allocator *arena) {
  ShaderTexture texture = {0};
  
  yaml_expect_key(parser, "texture");
  yaml_push_indent(parser);
  
  yaml_expect_key(parser, "slot");
  texture.slot = (uint32)yaml_parse_number_value(parser);
  
  yaml_expect_key(parser, "stage");
  texture.stage = yaml_parse_string_value(parser);
  
  yaml_expect_key(parser, "name");
  texture.name = yaml_parse_string_value(parser);
  
  yaml_expect_key(parser, "multisampled");
  texture.multisampled = yaml_parse_bool_value(parser);
  
  yaml_expect_key(parser, "type");
  texture.type = yaml_parse_string_value(parser);
  
  yaml_expect_key(parser, "sample_type");
  texture.sample_type = yaml_parse_string_value(parser);
  
  yaml_expect_key(parser, "msl_texture_n");
  texture.msl_texture_n = (uint32)yaml_parse_number_value(parser);
  
  yaml_pop_indent(parser);
  
  return texture;
}

// Parser for present_reflection.yaml structure (partial)
internal ShaderProgram yaml_parse_ShaderProgram_Partial(YamlParser *parser, Allocator *arena) {
  ShaderProgram program = {0};
  
  yaml_push_indent(parser);
  
  // Parse name
  yaml_expect_key(parser, "name");
  program.name = yaml_parse_string_value(parser);
  
  // Parse vertex_func
  yaml_expect_key(parser, "vertex_func");
  program.vertex_func = yaml_parse_ShaderFunc(parser, arena);
  
  // Parse fragment_func
  yaml_expect_key(parser, "fragment_func");
  program.fragment_func = yaml_parse_ShaderFunc(parser, arena);
  
  // Parse attrs list
  yaml_expect_key(parser, "attrs");
  yaml_push_indent(parser);
  
  // Count attrs (simplified - just parse first one)
  if (yaml_expect_list_item(parser)) {
    program.attr_count = 1;
    program.attrs = ALLOC_ARRAY(arena, ShaderAttr, 1);
    program.attrs[0] = yaml_parse_ShaderAttr(parser, arena);
  }
  
  yaml_pop_indent(parser);
  
  // Parse views list
  yaml_expect_key(parser, "views");
  yaml_push_indent(parser);
  
  // Count views (simplified - just parse first one)
  if (yaml_expect_list_item(parser)) {
    program.view_count = 1;
    program.views = ALLOC_ARRAY(arena, ShaderTexture, 1);
    program.views[0] = yaml_parse_ShaderTexture(parser, arena);
  }
  
  yaml_pop_indent(parser);
  yaml_pop_indent(parser);
  
  return program;
}

// Test functions
void test_simple_yaml(TestContext *ctx) {
  const char *yaml_str = 
    "response:\n"
    "  role: 42\n"
    "  content: \"Hello World\"\n"
    "foo: bar\n";
  
  Allocator *arena = &ctx->allocator;
  SimpleYamlStruct result = yaml_parse_SimpleYamlStruct(yaml_str, arena);
  
  assert_eq(result.response.role, 42);
  assert_str_eq(result.response.content, "Hello World");
  assert_str_eq(result.foo, "bar");
}

void test_yaml_with_lists(TestContext *ctx) {
  const char *yaml_str = 
    "items:\n"
    "  - first\n"
    "  - second\n"
    "  - third\n";
  
  Allocator *arena = &ctx->allocator;
  YamlParser parser = yaml_parser_init(yaml_str, arena);
  
  yaml_expect_key(&parser, "items");
  yaml_push_indent(&parser);
  
  // Parse first item
  assert_true(yaml_expect_list_item(&parser));
  char *item1 = yaml_parse_string_value(&parser);
  assert_str_eq(item1, "first");
  
  // Parse second item
  assert_true(yaml_expect_list_item(&parser));
  char *item2 = yaml_parse_string_value(&parser);
  assert_str_eq(item2, "second");
  
  // Parse third item
  assert_true(yaml_expect_list_item(&parser));
  char *item3 = yaml_parse_string_value(&parser);
  assert_str_eq(item3, "third");
  
  yaml_pop_indent(&parser);
}

void test_yaml_with_booleans(TestContext *ctx) {
  const char *yaml_str = 
    "flag1: true\n"
    "flag2: false\n"
    "flag3: yes\n"
    "flag4: no\n";
  
  Allocator *arena = &ctx->allocator;
  YamlParser parser = yaml_parser_init(yaml_str, arena);
  
  yaml_expect_key(&parser, "flag1");
  assert_true(yaml_parse_bool_value(&parser));
  
  yaml_expect_key(&parser, "flag2");
  assert_false(yaml_parse_bool_value(&parser));
  
  yaml_expect_key(&parser, "flag3");
  assert_true(yaml_parse_bool_value(&parser));
  
  yaml_expect_key(&parser, "flag4");
  assert_false(yaml_parse_bool_value(&parser));
}

void test_yaml_with_comments(TestContext *ctx) {
  const char *yaml_str = 
    "# This is a comment\n"
    "key1: value1\n"
    "# Another comment\n"
    "key2: value2  # Inline comment\n";
  
  Allocator *arena = &ctx->allocator;
  YamlParser parser = yaml_parser_init(yaml_str, arena);
  
  yaml_expect_key(&parser, "key1");
  char *val1 = yaml_parse_string_value(&parser);
  assert_str_eq(val1, "value1");
  
  yaml_expect_key(&parser, "key2");
  char *val2 = yaml_parse_string_value(&parser);
  assert_str_eq(val2, "value2");
}

void test_present_reflection_yaml(TestContext *ctx) {
  // Simplified version of present_reflection.yaml for testing
  const char *yaml_str = 
    "shaders:\n"
    "  -\n"
    "    slang: metal_macos\n"
    "    programs:\n"
    "      -\n"
    "        name: present\n"
    "        vertex_func:\n"
    "          path: shaders/present_vertex.metal\n"
    "          is_binary: false\n"
    "          entry_point: main0\n"
    "        fragment_func:\n"
    "          path: shaders/present_fragment.metal\n"
    "          is_binary: false\n"
    "          entry_point: main0\n"
    "        attrs:\n"
    "          -\n"
    "            slot: 0\n"
    "            type: vec2\n"
    "            base_type: Float\n"
    "        views:\n"
    "          -\n"
    "            texture:\n"
    "              slot: 0\n"
    "              stage: fragment\n"
    "              name: hdrTexture\n"
    "              multisampled: false\n"
    "              type: 2d\n"
    "              sample_type: float\n"
    "              msl_texture_n: 0\n";
  
  Allocator *arena = &ctx->allocator;
  YamlParser parser = yaml_parser_init(yaml_str, arena);
  
  // Parse root
  yaml_expect_key(&parser, "shaders");
  yaml_push_indent(&parser);
  
  // Parse first shader config
  assert_true(yaml_expect_list_item(&parser));
  yaml_push_indent(&parser);
  
  yaml_expect_key(&parser, "slang");
  char *slang = yaml_parse_string_value(&parser);
  assert_str_eq(slang, "metal_macos");
  
  yaml_expect_key(&parser, "programs");
  yaml_push_indent(&parser);
  
  // Parse first program
  assert_true(yaml_expect_list_item(&parser));
  ShaderProgram program = yaml_parse_ShaderProgram_Partial(&parser, arena);
  
  // Verify parsed values
  assert_str_eq(program.name, "present");
  assert_str_eq(program.vertex_func.path, "shaders/present_vertex.metal");
  assert_false(program.vertex_func.is_binary);
  assert_str_eq(program.vertex_func.entry_point, "main0");
  
  assert_eq(program.attr_count, 1);
  assert_eq(program.attrs[0].slot, 0);
  assert_str_eq(program.attrs[0].type, "vec2");
  
  assert_eq(program.view_count, 1);
  assert_eq(program.views[0].slot, 0);
  assert_str_eq(program.views[0].name, "hdrTexture");
  
  yaml_pop_indent(&parser);
  yaml_pop_indent(&parser);
  yaml_pop_indent(&parser);
}
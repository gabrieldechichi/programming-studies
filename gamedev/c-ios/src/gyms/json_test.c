#include "../game.h"
#include "../lib/http.h"
#include "../lib/json_parser.h"
#include "../lib/json_serializer.h"
#include "../lib/memory.h"
#include "../lib/typedefs.h"
#include "platform/platform.h"
#include "../vendor/stb/stb_image.h"

typedef struct {
} GymState;

global GymState *gym_state;

typedef struct {
  struct {
    double role;
    const char *content;
  } response;

  const char *foo;
} MyTestResponse;

// Static assertions to catch struct changes - would be auto-generated
static_assert(sizeof(MyTestResponse) == sizeof(struct {
                struct {
                  double role;
                  const char *content;
                } response;
                const char *foo;
              }),
              "MyTestResponse struct layout changed - regenerate parser");
static_assert(offsetof(MyTestResponse, response) == 0,
              "MyTestResponse.response field moved - regenerate parser");
static_assert(offsetof(MyTestResponse, foo) == sizeof(struct {
                double role;
                const char *content;
              }),
              "MyTestResponse.foo field moved - regenerate parser");
// Nested struct assertions
static_assert(offsetof(MyTestResponse, response.role) == 0,
              "MyTestResponse.response.role field moved - regenerate parser");
static_assert(
    offsetof(MyTestResponse, response.content) == sizeof(double),
    "MyTestResponse.response.content field moved - regenerate parser");

// ====== TYPE-SAFE JSON PARSER FOR MyTestResponse ======
internal MyTestResponse json_parse_MyTestResponse(const char *json_str,
                                                  Allocator *arena) {
  MyTestResponse result = {0};
  JsonParser parser = json_parser_init(json_str, arena);

  // Parse: {
  expect_object_start(&parser);

  // Parse: "response"
  expect_key(&parser, "response");
  expect_colon(&parser);

  // Parse response object: {
  expect_object_start(&parser);

  // Parse: "role"
  expect_key(&parser, "role");
  expect_colon(&parser);
  result.response.role = (uint32)parse_number_value(&parser);

  expect_comma(&parser);

  // Parse: "content"
  expect_key(&parser, "content");
  expect_colon(&parser);
  result.response.content = parse_string_value(&parser);

  // Parse: } (end response object)
  expect_object_end(&parser);

  expect_comma(&parser);

  // Parse: "foo"
  expect_key(&parser, "foo");
  expect_colon(&parser);
  result.foo = parse_string_value(&parser);

  // Parse: } (end main object)
  expect_object_end(&parser);

  // Expect end of input
  assert_msg(is_at_end(&parser), "Expected end of input");

  return result;
}

// ====== TYPE-SAFE JSON SERIALIZER FOR MyTestResponse ======
internal char *json_serialize_MyTestResponse(const MyTestResponse *data,
                                             Allocator *arena) {
  JsonSerializer serializer = json_serializer_init(arena, 256);

  // Serialize: {
  write_object_start(&serializer);

  // Serialize: "response": {
  write_key(&serializer, "response");
  write_object_start(&serializer);

  // Serialize: "role": 42
  write_key(&serializer, "role");
  serialize_number_value(&serializer, (double)data->response.role);

  write_comma(&serializer);

  // Serialize: "content": "Hello World"
  write_key(&serializer, "content");
  serialize_string_value(&serializer, data->response.content);

  // Serialize: } (end response object)
  write_object_end(&serializer);

  write_comma(&serializer);

  // Serialize: "foo": "bar"
  write_key(&serializer, "foo");
  serialize_string_value(&serializer, data->foo);

  // Serialize: } (end main object)
  write_object_end(&serializer);

  return json_serializer_finalize(&serializer);
}

void gym_init(GameMemory *memory) {
  GameContext *ctx = &memory->ctx;

  gym_state = ALLOC(&ctx->allocator, sizeof(GymState));

  const char *test_json = "{\"response\":{\"role\":42,\"content\":\"Hello "
                          "World\"},\"foo\":\"bar\"}";
  LOG_INFO("Testing type-safe parser with: %", FMT_STR(test_json));

  MyTestResponse typed_result =
      json_parse_MyTestResponse(test_json, &ctx->temp_allocator);
  LOG_INFO("Type-safe parser - role: %, content: %, foo: %",
           FMT_UINT(typed_result.response.role),
           FMT_STR(typed_result.response.content), FMT_STR(typed_result.foo));

  // Test serialization
  MyTestResponse test_data = {
      .response = {.role = 1232923940412.23,
                   .content =
                       "Serialized content with \"quotes\" and\nnewlines"},
      .foo = "serialized foo"};

  char *serialized_json =
      json_serialize_MyTestResponse(&test_data, &ctx->temp_allocator);
  LOG_INFO("Type-safe serializer output: %", FMT_STR(serialized_json));

  // Test epsilon-based rounding directly
  char test_buffer[64];
  double_to_str(1232923940412.23, test_buffer);
  LOG_INFO("Direct double_to_str test for 1232923940412.23: %",
           FMT_STR(test_buffer));

  // Test round-trip: serialize then parse
  MyTestResponse round_trip_result =
      json_parse_MyTestResponse(serialized_json, &ctx->temp_allocator);
  LOG_INFO("Round-trip result - role: %, content: %, foo: %",
           FMT_UINT(round_trip_result.response.role),
           FMT_STR(round_trip_result.response.content),
           FMT_STR(round_trip_result.foo));
}

void gym_update_and_render(GameMemory *memory) {}

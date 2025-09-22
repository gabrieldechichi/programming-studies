#include "game.h"
#include "lib/http.h"
#include "config.h"
#include "lib/memory.h"
#include "lib/typedefs.h"
#include "platform/platform.h"
#include "stb/stb_image.h"

typedef struct {
  ArenaAllocator permanent_arena;
  ArenaAllocator temporary_arena;
  GameContext ctx;

  HttpRequest test_http_basic;
  HttpStreamRequest test_http_op;
  bool32 basic_test_done;
} GymState;

global GameContext *g_ctx;
extern GameContext *get_global_ctx() { return g_ctx; }

void gym_init(GameMemory *memory) {
  GymState *gym_state = cast(GymState *) memory->permanent_memory;
  g_ctx = &gym_state->ctx;
  gym_state->permanent_arena =
      arena_from_buffer(memory->permanent_memory + sizeof(GymState),
                        memory->pernament_memory_size - sizeof(GymState));
  gym_state->temporary_arena = arena_from_buffer((u8 *)memory->temporary_memory,
                                                 memory->temporary_memory_size);

  gym_state->ctx.allocator = make_arena_allocator(&gym_state->permanent_arena);
  gym_state->ctx.temp_allocator =
      make_arena_allocator(&gym_state->temporary_arena);
  GameContext *ctx = &gym_state->ctx;

  // First test a simple GET to verify basic HTTP works
  LOG_INFO("Testing basic HTTP GET to httpbin.org/get (time: %)",
           FMT_FLOAT(memory->time.now));
  gym_state->test_http_basic =
      http_get_async("https://httpbin.org/get", &ctx->temp_allocator);
  gym_state->basic_test_done = false;
}

void gym_update_and_render(GameMemory *memory) {
  GymState *gym_state = cast(GymState *) memory->permanent_memory;
  GameContext *ctx = &gym_state->ctx;

  // Test basic HTTP first
  if (!gym_state->basic_test_done) {
    HttpRequest *basic_req = &gym_state->test_http_basic;

    if (http_request_is_complete(basic_req)) {
      HttpResponse response = http_request_get_response(basic_req);

      if (response.success) {
        LOG_INFO("Basic GET SUCCESS! Status: %, Body length: %, Time: %",
                 FMT_INT(response.status_code), FMT_UINT(response.body_len),
                 FMT_FLOAT(memory->time.now));
        // Print first 100 chars
      } else {
        LOG_ERROR("Basic GET FAILED! Error: %",
                  FMT_STR(response.error_message ? response.error_message
                                                 : "Unknown"));
      }

      gym_state->basic_test_done = true;

      // Now start the streaming test
      LOG_INFO("");
      LOG_INFO(
          "Starting streaming test to https://httpbin.org/stream/1 (time: %)",
          FMT_FLOAT(memory->time.now));
      gym_state->test_http_op = http_stream_get_async(
          "https://httpbin.org/stream/100000", &ctx->temp_allocator);
    }
    return;
  }

  // Test streaming
  HttpStreamRequest *req = &gym_state->test_http_op;

  if (http_stream_has_error(req)) {
    LOG_ERROR("Stream error!");
  }

  if (http_stream_has_chunk(req)) {
    HttpStreamChunk resp = http_stream_get_chunk(req);
    LOG_INFO("Stream chunk received! Length: % (time %)",
             FMT_UINT(resp.chunk_len), FMT_FLOAT(memory->time.now));

    if (resp.is_final_chunk) {
      LOG_INFO("Final chunk received!");
    }
  }

  local_persist b32 did_log_stream_complete = false;
  if (http_stream_is_complete(req) && !did_log_stream_complete) {
    did_log_stream_complete = true;
    LOG_INFO("Stream complete! %", FMT_FLOAT(memory->time.now));
  }
}

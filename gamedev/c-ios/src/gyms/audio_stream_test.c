#include "../assets.h"
#include "../game.h"
#include "../lib/audio.h"
#include "../lib/http.h"
#include "../lib/json.h"
#include "../lib/memory.h"
#include "../lib/typedefs.h"
#include "../platform.h"
#include "../vendor/stb/stb_image.h"
#include "../lib/network.h"
#include "../config.h"

typedef struct {
  AudioState audio_system;
  AssetSystem asset_system;
  StreamingAudioClip *audio_stream;
  HttpStreamRequest test_http_op;
  NetworkAudioStreamBuffer net_buffer;
} GymState;

global GymState *gym_state;

void gym_init(GameMemory *memory) {
  GameContext *ctx = &memory->ctx;

  gym_state = ALLOC(&ctx->allocator, sizeof(GymState));
  gym_state->audio_system = audio_init(ctx);
  gym_state->asset_system = asset_system_init(&ctx->allocator, 16);

  AudioState *audio_system = &gym_state->audio_system;
  StreamingAudioClip stream_clip =
      streaming_clip_create(24000, 1, 24000 * sizeof(i16) * 60, ctx);
  audio_play_streaming_clip(audio_system, stream_clip);
  assert(audio_system->streaming_clips.len == 1);
  gym_state->audio_stream = &audio_system->streaming_clips.items[0];
  assert(gym_state->audio_stream);

  gym_state->test_http_op = http_stream_get_async(
      BACKEND_URL "/tomoChat/openai-tts-stream-test",
      &ctx->temp_allocator);

  gym_state->net_buffer =
      network_audio_buffer_init(gym_state->audio_stream, 0.5, ctx);

  LOG_INFO("Send network request at %", FMT_FLOAT(memory->time.now));
}

void gym_update_and_render(GameMemory *memory) {
  GameContext *ctx = &memory->ctx;
  HttpStreamRequest *req = &gym_state->test_http_op;
  local_persist b32 did_load = false;
  local_persist b32 first_chunk = true;
  local_persist b32 first_flush = true;

  AudioState *audio_system = &gym_state->audio_system;
  AssetSystem *asset_system = &gym_state->asset_system;
  asset_system_update(asset_system, ctx);

  if (http_stream_is_complete(req)) {
    LOG_INFO("Final chunk received");
    did_load = true;
  }

  if (!did_load && http_stream_has_chunk(req)) {
    if (first_chunk) {
      LOG_INFO("Just got first chunk at %", FMT_FLOAT(memory->time.now));
      first_chunk = false;
    }
    HttpStreamChunk resp = http_stream_get_chunk(req);

    // Add chunk to buffer
    b32 should_flush = network_audio_buffer_add_chunk(
        &gym_state->net_buffer, (u8 *)resp.chunk_data, resp.chunk_len);

    // Flush when threshold is reached
    if (should_flush) {
      if (first_flush) {
        LOG_INFO("First flush at at %", FMT_FLOAT(memory->time.now));
        first_flush = false;
      }
      network_audio_buffer_flush(&gym_state->net_buffer);
    }

    // Handle final chunk - flush any remaining data
    if (resp.is_final_chunk) {
      network_audio_buffer_flush_remaining(&gym_state->net_buffer);
      LOG_INFO("Final chunk received");
      did_load = true;
    }
  }

  f32 dt = memory->time.dt;
  audio_update(audio_system, dt, ctx);
}

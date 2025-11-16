#include "assets.h"
#include "game.h"
#include "lib/audio.h"
#include "lib/memory.h"
#include "lib/typedefs.h"
#include "platform/platform.h"
#include "vendor/stb/stb_image.h"

typedef struct {
  ArenaAllocator permanent_arena;
  ArenaAllocator temporary_arena;
  GameContext ctx;

  AudioState audio_system;
  AssetSystem asset_system;
  StreamingAudioClip *audio_stream;
  WavFile_Handle wav_file_handle;
  WavFile *wav_file;
  u32 stream_len_per_frame;
  u32 streamed_len;
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

  gym_state->audio_system = audio_init(ctx);
  gym_state->asset_system = asset_system_init(&ctx->allocator, 16);

  gym_state->wav_file_handle = asset_request(WavFile, &gym_state->asset_system,
                                             ctx, "assets/univ0023.wav");

  gym_state->streamed_len = 0;
}

void gym_update_and_render(GameMemory *memory) {
  GymState *gym_state = cast(GymState *) memory->permanent_memory;
  GameContext *ctx = &gym_state->ctx;
  local_persist b32 did_load = false;

  AudioState *audio_system = &gym_state->audio_system;
  AssetSystem *asset_system = &gym_state->asset_system;
  asset_system_update(asset_system, ctx);

  if (!gym_state->audio_stream &&
      asset_is_ready(asset_system, gym_state->wav_file_handle)) {
    WavFile *wav_file =
        asset_get_data(WavFile, asset_system, gym_state->wav_file_handle);
    gym_state->wav_file = wav_file;
    StreamingAudioClip stream_clip = streaming_clip_create(
        wav_file->format.sample_rate, wav_file->format.channels,
        wav_file->format.sample_rate * 20, ctx);
    audio_play_streaming_clip(audio_system, stream_clip);
    gym_state->audio_stream = &audio_system->streaming_clips.items[0];
    assert(gym_state->audio_stream);

    gym_state->stream_len_per_frame = wav_file->format.sample_rate * (1 / 10.0);
  }

  {
    StreamingAudioClip *audio_stream = gym_state->audio_stream;
    WavFile *wav_file = gym_state->wav_file;
    if (!did_load && wav_file && audio_stream) {
      u32 len_to_stream = gym_state->stream_len_per_frame;
      if (gym_state->streamed_len + len_to_stream > wav_file->data_size) {
        len_to_stream = wav_file->data_size - gym_state->streamed_len;
      }
      streaming_clip_write_pcm(
          audio_stream, ((u8 *)wav_file->audio_data) + gym_state->streamed_len,
          len_to_stream);
      gym_state->streamed_len += len_to_stream;

      if (gym_state->streamed_len >= wav_file->data_size) {
        streaming_clip_mark_complete(audio_stream);
        did_load = true;
      }
      LOG_INFO("Loaded % bytes of audio data", FMT_UINT(wav_file->data_size));
    }

    if (audio_stream) {
      LOG_INFO("playing % ", FMT_UINT(audio_stream->is_playing));
    }
  }

  f32 dt = memory->time.dt;
  audio_update(audio_system, ctx, dt);
}

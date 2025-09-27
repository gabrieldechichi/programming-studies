#include "assets.h"
#include "game.h"
#include "lib/audio.h"
#include "lib/fmt.h"
#include "lib/typedefs.h"
#include "platform/platform.h"
#include <stdint.h>

typedef struct {
  ArenaAllocator permanent_arena;
  ArenaAllocator temporary_arena;
  GameContext ctx;

  AssetSystem assets;
  GameInput input;
  AudioState audio_state;
  WavFile_Handle wav_file_handle;
  WavFile *wav_file;
} GymState;

void gym_init(GameMemory *memory) {
  GymState *gym_state = cast(GymState *) memory->permanent_memory;

  gym_state->permanent_arena =
      arena_from_buffer(memory->permanent_memory + sizeof(GymState),
                        memory->pernament_memory_size - sizeof(GymState));
  gym_state->temporary_arena = arena_from_buffer((u8 *)memory->temporary_memory,
                                                 memory->temporary_memory_size);

  gym_state->ctx.allocator = make_arena_allocator(&gym_state->permanent_arena);
  gym_state->ctx.temp_allocator =
      make_arena_allocator(&gym_state->temporary_arena);
  GameContext *ctx = &gym_state->ctx;

  gym_state->assets = asset_system_init(&ctx->allocator, 64);

  gym_state->audio_state = audio_init(ctx);

  gym_state->wav_file_handle =
      asset_request(WavFile, &gym_state->assets, ctx, "assets/univ0023.wav");

  LOG_INFO("Audio WAV test initialized. Sample rate: % Hz",
           FMT_INT(gym_state->audio_state.output_sample_rate));
}

void gym_update_and_render(GameMemory *memory) {
  GymState *gym_state = cast(GymState *) memory->permanent_memory;
  GameContext *ctx = &gym_state->ctx;
  GameTime *time = &memory->time;
  GameInput *input = &gym_state->input;
  f32 dt = memory->time.dt;
  asset_system_update(&gym_state->assets, ctx);
  input_update(input, &memory->input_events, time->now);

  local_persist b32 did_load = false;

  // check if file is loaded
  if (!did_load && asset_system_pending_count(&gym_state->assets) == 0) {
    did_load = true;
    WavFile *wav_file =
        asset_get_data(WavFile, &gym_state->assets, gym_state->wav_file_handle);
    assert(wav_file);
    if (wav_file) {
      LOG_INFO("WAV file loaded: %d Hz, %d channels, %d samples",
               FMT_INT(wav_file->format.sample_rate),
               FMT_INT(wav_file->format.channels),
               FMT_INT(wav_file->total_samples));
    }
    gym_state->wav_file = wav_file;
  }

  if (gym_state->wav_file) {
    // create and play audio clip
    AudioClip clip = {.wav_file = gym_state->wav_file, .loop = false, .volume = 1.0};
    audio_play_clip(&gym_state->audio_state, clip);
  }
  input_end_frame(input);

  // update audio system
  audio_update(&gym_state->audio_state, ctx, dt);
}
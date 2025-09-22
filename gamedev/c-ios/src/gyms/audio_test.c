#include "context.h"
#include "game.h"
#include "lib/typedefs.h"
#include "platform/platform.h"
#include <math.h>

typedef struct {
  ArenaAllocator permanent_arena;
  ArenaAllocator temporary_arena;
  GameContext ctx;

  GameInput input;
  f32 time;
  f32 frequency;
  f32 amplitude;
  i32 sample_rate;
  i32 channels;

  // Persistent audio buffer to avoid temp allocator
  i32 max_samples_per_frame;
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

  gym_state->time = 0.0f;
  gym_state->frequency = 440.0f; // A4 note
  gym_state->amplitude = 0.3f;   // 30% volume to be safe
  gym_state->sample_rate = platform_audio_get_sample_rate();
  gym_state->channels = 2; // stereo

  gym_state->max_samples_per_frame =
      (gym_state->sample_rate / 20); // 1 frame at 20fps worst case
}

void gym_update_and_render(GameMemory *memory) {
  GymState *gym_state = cast(GymState *) memory->permanent_memory;
  GameContext *ctx = &gym_state->ctx;
  GameTime *time = &memory->time;
  GameInput *input = &gym_state->input;

  input_update(input, &memory->input_events, time->now);

  // f32 dt = 1.0f / 60.0f;
  f32 dt = memory->time.dt;
  i32 samples_needed = (i32)(gym_state->sample_rate * dt);

  // Clamp to our buffer size
  if (samples_needed > gym_state->max_samples_per_frame) {
    samples_needed = gym_state->max_samples_per_frame;
  }

  i32 buffer_size = samples_needed * gym_state->channels;
  f32 *audio_samples = ALLOC_ARRAY(&ctx->temp_allocator, f32, buffer_size);

  // Generate sine wave samples
  for (i32 i = 0; i < samples_needed; i++) {
    f32 sample_time = gym_state->time + (f32)i / gym_state->sample_rate;
    f32 sine_value = sinf(2.0f * PI * gym_state->frequency * sample_time);
    f32 sample = sine_value * gym_state->amplitude;
    sample *= 0.2f;

    // if (!input->up.is_pressed) {
    //   sample = 0.0;
    // }

    // Write stereo samples (same value for both channels)
    audio_samples[i * 2] = sample;     // left channel
    audio_samples[i * 2 + 1] = sample; // right channel
  }

  platform_audio_write_samples(audio_samples,
                               samples_needed * gym_state->channels);

  // Update time based on samples generated (not frame time)
  f32 time_increment = (f32)samples_needed / gym_state->sample_rate;
  gym_state->time += time_increment;

  // Keep time reasonable to avoid precision issues
  // if (gym_state->time > 1000.0f) {
  //   gym_state->time = fmodf(gym_state->time, 1.0f);
  // }
  input_end_frame(input);
}
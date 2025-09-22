#include "../game.h"
#include "../gameplay_lib.c"
#include "../lib/array.h"
#include "../lib/audio.h"
#include "../lib/memory.h"
#include "../lib/microphone.h"
#include "../lib/typedefs.h"
#include "../platform.h"
#include "../vendor/stb/stb_image.h"
#include <string.h>

typedef struct {
  MicrophoneState mic_system;
  AudioState audio_system;
  GameInput input_system;

  i16_Slice recording_buffer;
  b32 is_recording;
  b32 was_recording;
} GymState;

global GymState *gym_state;

void gym_init(GameMemory *memory) {
  GameContext *ctx = &memory->ctx;

  gym_state = ALLOC(&ctx->allocator, sizeof(GymState));

  gym_state->mic_system = microphone_init(ctx);
  gym_state->audio_system = audio_init(ctx);
  u32 sample_rate = microphone_get_sample_rate(&gym_state->mic_system);
  gym_state->recording_buffer =
      slice_new_ALLOC(&ctx->allocator, i16, sample_rate * 30);
  gym_state->is_recording = false;
}

void gym_update_and_render(GameMemory *memory) {
  GameContext *ctx = &memory->ctx;
  MicrophoneState *mic_system = &gym_state->mic_system;
  AudioState *audio_system = &gym_state->audio_system;
  GameInput *input_system = &gym_state->input_system;

  input_update(input_system, &memory->input_events, memory->time.now);
  audio_update(audio_system, memory->time.dt, ctx);

  if (input_system->space.pressed_this_frame) {
    if (!gym_state->is_recording) {
      microphone_start_recording(mic_system);
      gym_state->is_recording = true;
      gym_state->was_recording = false;
      gym_state->recording_buffer.len = 0; // Reset buffer
      LOG_INFO("Started recording...");
    }
  }

  if (gym_state->is_recording) {
    u32 available_samples = microphone_get_available_samples(mic_system);
    if (available_samples > 0) {
      i32 space_left = glm_max(
          gym_state->recording_buffer.cap - gym_state->recording_buffer.len, 0);
      u32 samples_to_read =
          available_samples < (u32)space_left ? available_samples : space_left;

      // Read samples to temp buffer first
      i16 *temp_buffer =
          ALLOC_ARRAY(&ctx->temp_allocator, i16, samples_to_read);
      microphone_read_samples(mic_system, temp_buffer, samples_to_read);

      // VAD filtering is now done at the platform level, just record all
      // samples
      i16_Slice *mic_buffer = &gym_state->recording_buffer;
      u32 prev_len = mic_buffer->len;
      slice_increase_len(*mic_buffer, samples_to_read);
      memcpy(mic_buffer->items + prev_len, temp_buffer,
             samples_to_read * sizeof(i16));
      if (space_left <= 0) {
        LOG_WARN("Recording buffer full! Stopping recording.");
        microphone_stop_recording(mic_system);
        gym_state->is_recording = false;
      }
    }
  }

  if (input_system->space.released_this_frame && gym_state->is_recording) {
    gym_state->is_recording = false;
    microphone_stop_recording(mic_system);
  }

  if (!gym_state->is_recording && gym_state->was_recording) {
    if (gym_state->recording_buffer.len > 0) {
      LOG_INFO("Stopped recording. Creating WAV from % samples...",
               FMT_UINT(gym_state->recording_buffer.len));

      // Create WAV file from recorded samples
      WavFile *wav = create_wav_from_samples_alloc(
          gym_state->recording_buffer.items, gym_state->recording_buffer.len,
          microphone_get_sample_rate(mic_system), &ctx->allocator);

      // Create and play audio clip
      AudioClip clip = {0};
      clip.wav_file = wav;
      clip.loop = false;
      clip.volume = 1.0f;

      audio_play_clip(audio_system, clip);
      LOG_INFO("Playing recorded audio...");
    }
  }

  gym_state->was_recording = gym_state->is_recording;

  input_end_frame(input_system);
}

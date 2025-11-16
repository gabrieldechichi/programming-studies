#include "../game.h"
#include "../gameplay_lib.c"
#include "../lib/audio.h"
#include "../stt_system.h"

typedef struct {
  SpeechToTextSystem stt_system;
  GameInput input_system;
  AudioState audio_system;
  WavFile *recorded_wav;
  i16_Slice audio_backup_buffer;
} GymState;

global GymState *gym_state;

void gym_init(GameMemory *memory) {
  GameContext *ctx = &memory->ctx;

  gym_state = ALLOC(&ctx->allocator, sizeof(GymState));

  gym_state->audio_system = audio_init(ctx);

  stt_init(&gym_state->stt_system, ctx);

  u32 sample_rate =
      microphone_get_sample_rate(&gym_state->stt_system.mic_system);
  gym_state->audio_backup_buffer =
      slice_new_ALLOC(&ctx->allocator, i16, sample_rate * 120);

  gym_state->recorded_wav = NULL;

  LOG_INFO(
      "STT Test initialized - speak into microphone to test transcription");
}

void gym_update_and_render(GameMemory *memory) {
  GameContext *ctx = &memory->ctx;
  GameInput *input_system = &gym_state->input_system;

  input_update(input_system, &memory->input_events, memory->time.now);

  SpeechToTextSystem *stt = &gym_state->stt_system;

  // Step 1: Update recording first
  stt_update_recording(stt, memory->time.dt, ctx);

  // Step 2: Check if we will send STT request and backup audio data
  b32 will_send_stt_request =
      (stt->silence_duration >= stt->silence_threshold &&
       stt->is_actively_recording && !stt->has_pending_stt &&
       stt->recording_buffer.len >= stt->recording_buffer_threshold);

  if (will_send_stt_request && stt->recording_buffer.len > 0) {
    LOG_INFO("Backing up % samples before STT request",
             FMT_UINT(stt->recording_buffer.len));

    // Copy recording buffer to backup before it gets cleared
    gym_state->audio_backup_buffer.len = stt->recording_buffer.len;
    memcpy(gym_state->audio_backup_buffer.items, stt->recording_buffer.items,
           stt->recording_buffer.len * sizeof(i16));
  }

  // Step 3: Process STT request (this may clear the recording buffer)
  b32 had_pending_before = stt->has_pending_stt;
  stt_update_request(stt, memory->time.dt, ctx);

  // Step 4: If STT request was just sent, create audio clip from backup
  if (!had_pending_before && stt->has_pending_stt &&
      gym_state->audio_backup_buffer.len > 0) {
    LOG_INFO("Creating WAV from % backed up samples",
             FMT_UINT(gym_state->audio_backup_buffer.len));

    gym_state->recorded_wav = create_wav_from_samples_alloc(
        gym_state->audio_backup_buffer.items,
        gym_state->audio_backup_buffer.len, stt->mic_system.sample_rate,
        &ctx->allocator);
  }

  if (stt->has_new_result) {
    String stt_result = stt_get_result(stt, &ctx->temp_allocator);

    if (stt_result.len > 0) {
      LOG_INFO("STT Result: '%'(%)", FMT_STR(stt_result.value),
               FMT_UINT(stt_result.len));

      if (gym_state->recorded_wav) {
        LOG_INFO("Playing back recorded audio");
        AudioClip clip = {.wav_file = gym_state->recorded_wav,
                          .playback_position = 0.0f,
                          .is_playing = false,
                          .volume = 1.0f,
                          .sample_rate_ratio = 1.0f,
                          .loop = false};
        audio_play_clip(&gym_state->audio_system, clip);
        gym_state->recorded_wav = NULL;
      }
    }
  }

  audio_update(&gym_state->audio_system, ctx, memory->time.dt);

  input_end_frame(input_system);
}

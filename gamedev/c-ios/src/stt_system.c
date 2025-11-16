#include "stt_system.h"
#include "config.h"
#include "lib/audio.h"
#include "lib/math.h"

void stt_init(SpeechToTextSystem *stt, GameContext *ctx) {
  // todo: should stt be initing microphone? Probably not
  stt->mic_system = microphone_init(ctx);
  u32 sample_rate = microphone_get_sample_rate(&stt->mic_system);
  stt->recording_buffer = slice_new_ALLOC(
      &ctx->allocator, i16, sample_rate * 120); // 2 minutes recording max
  stt->stt_result_buffer = slice_new_ALLOC(&ctx->allocator, char, 4096);

  stt->silence_threshold = 0.5f;
  stt->recording_buffer_threshold =
      microphone_get_sample_rate(&stt->mic_system) / 4;
  stt->recording_buffer_threshold_to_halt_tts =
      microphone_get_sample_rate(&stt->mic_system) * 0.4;
  stt->silence_duration = 0.0f;
  stt->is_actively_recording = false;
  stt->has_pending_stt = false;
  stt->has_new_result = false;

  // // start microphone immediately
  // // todo: should tts system be doing this? probably not
  // microphone_start_recording(&stt->mic_system);
}

void stt_update_recording(SpeechToTextSystem *stt, f32 dt, GameContext *ctx) {
  u32 available_samples = microphone_get_available_samples(&stt->mic_system);
  if (available_samples > 0) {
    i32 space_left =
        max(stt->recording_buffer.cap - stt->recording_buffer.len, 0);
    u32 samples_to_read =
        available_samples < (u32)space_left ? available_samples : space_left;

    i16 *temp_buffer = ALLOC_ARRAY(&ctx->temp_allocator, i16, samples_to_read);
    microphone_read_samples(&stt->mic_system, temp_buffer, samples_to_read);

    // User is speaking - add to recording buffer and reset silence timer
    // todo: proper slice function here
    u32 prev_len = stt->recording_buffer.len;
    slice_increase_len(stt->recording_buffer, samples_to_read);
    memcpy(stt->recording_buffer.items + prev_len, temp_buffer,
           samples_to_read * sizeof(i16));

    stt->is_actively_recording = true;
    stt->silence_duration = 0.0f;

    // Handle buffer overflow
    // todo: don't think we should force a stt request if the user is speaking
    // for too long
    if (space_left <= 0) {
      LOG_WARN("STT recording buffer full! Forcing STT request.");
      stt->silence_duration = stt->silence_threshold; // Force STT request
    }
  } else {
    stt->silence_duration += dt;
  }

  stt->should_halt_tts =
      stt->recording_buffer.len > stt->recording_buffer_threshold_to_halt_tts;
}

void stt_update_request(SpeechToTextSystem *stt, f32 dt, GameContext *ctx) {
  // check if we should send STT request
  if (stt->silence_duration >= stt->silence_threshold &&
      stt->is_actively_recording && !stt->has_pending_stt &&
      stt->recording_buffer.len >= stt->recording_buffer_threshold) {

    LOG_INFO("Sending STT request after % seconds of silence (% samples)",
             FMT_FLOAT(stt->silence_duration),
             FMT_UINT(stt->recording_buffer.len));

    WavFile *wav = create_wav_from_samples_alloc(
        stt->recording_buffer.items, stt->recording_buffer.len,
        stt->mic_system.sample_rate, &ctx->temp_allocator);
    u8_Array wav_bytes = wav_write_file_alloc(wav, &ctx->temp_allocator);

    debug_assert(wav_bytes.len > 0);
    if (wav_bytes.len > 0) {
      // send STT request
      const char *headers = "Content-Type: audio/wav";
      stt->stt_stream_req = http_stream_post_binary_async(
          BACKEND_URL "/tomoChat/conversation/stream-stt", headers,
          (const char *)wav_bytes.items, wav_bytes.len, &ctx->temp_allocator);
    }

    // clear recording buffer and update state
    stt->recording_buffer.len = 0;
    memset(stt->recording_buffer.items, 0,
           stt->recording_buffer.cap * sizeof(i16));
    stt->is_actively_recording = false;
    stt->has_pending_stt = true;
    stt->silence_duration = 0.0f;

    // clear previous STT result
    stt->stt_result_buffer.len = 0;
    stt->has_new_result = false;
  }

  // handle STT response
  if (stt->has_pending_stt && http_stream_has_chunk(&stt->stt_stream_req)) {
    HttpStreamChunk resp = http_stream_get_chunk(&stt->stt_stream_req);

    // append chunk to result buffer
    // todo: proper slice function here
    u32 prev_len = stt->stt_result_buffer.len;
    slice_increase_len(stt->stt_result_buffer, resp.chunk_len);
    memcpy(stt->stt_result_buffer.items + prev_len, resp.chunk_data,
           resp.chunk_len);

    if (resp.is_final_chunk) {
      stt->has_pending_stt = false;
      stt->has_new_result = true;
    }
  }
}

void stt_update(SpeechToTextSystem *stt, f32 dt, GameContext *ctx) {
  stt_update_recording(stt, dt, ctx);
  stt_update_request(stt, dt, ctx);
}

String stt_get_result(SpeechToTextSystem *stt, Allocator *allocator) {
  if (!stt->has_new_result) {
    return (String){0};
  }

  String raw_result = str_from_cstr_alloc(
      stt->stt_result_buffer.items, stt->stt_result_buffer.len, allocator);

  // Sanitize string: trim whitespace, spaces, and line breaks
  String result = str_trim(raw_result, allocator);
  result = str_trim_chars(result, "\".#", allocator);

  stt->has_new_result = false;
  return result;
}

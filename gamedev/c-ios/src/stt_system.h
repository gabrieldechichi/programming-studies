#ifndef H_STT_SYSTEM
#define H_STT_SYSTEM

#include "lib/http.h"
#include "lib/microphone.h"
#include "lib/string.h"
typedef struct {
  MicrophoneState mic_system;
  i16_Slice recording_buffer;
  char_Slice stt_result_buffer;
  HttpStreamRequest stt_stream_req;

  f32 silence_duration;
  // used to detect when user stops talking
  f32 silence_threshold;
  // minimum mic recorded samples to send stt request
  u32 recording_buffer_threshold;

  // minimum samples to halt request (e.g if user has started talking again)
  u32 recording_buffer_threshold_to_halt_tts;

  b32 is_actively_recording;
  // todo: better name here?
  b32 should_halt_tts;
  b32 has_pending_stt;
  b32 has_new_result;
} SpeechToTextSystem;

void stt_init(SpeechToTextSystem *stt, GameContext *ctx);

void stt_update(SpeechToTextSystem *stt, f32 dt, GameContext *ctx);

void stt_update_recording(SpeechToTextSystem *stt, f32 dt, GameContext *ctx);
void stt_update_request(SpeechToTextSystem *stt, f32 dt, GameContext *ctx);

String stt_get_result(SpeechToTextSystem *stt, Allocator *allocator);
#endif

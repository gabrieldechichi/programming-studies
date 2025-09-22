#ifndef H_TTS_SYSTEM
#define H_TTS_SYSTEM

#include "game.h"
#include "lib/audio.h"
#include "lib/http.h"
#include "lib/string.h"

typedef enum {
  EMOTION_TAGS_NEUTRAL,
  EMOTION_TAGS_HAPPY,
  EMOTION_TAGS_SAD,
  EMOTION_TAGS_ANGRY,
  EMOTION_TAGS_SURPRISED,
  EMOTION_TAGS_SCARED,
  EMOTION_TAGS_SERIOUS,
  EMOTION_TAGS_SMUG,
  EMOTION_TAGS_MAX
} EMOTION_TAGS;

const char *emotion_tags[] = {"neutral",   "happy",  "sad",     "angry",
                              "surprised", "scared", "serious", "smug"};


typedef struct {
  String text;
  HttpStreamRequest tts_request;
  bool32 tts_started;
  bool32 tts_complete;
  u32 total_audio_data_len;
  u8_Slice pending_audio_data;
  
  // Emotion detection fields
  EMOTION_TAGS detected_emotion;
  bool32 emotion_request_pending;
  HttpRequest emotion_request;
  bool32 emotion_detected;
  bool32 did_play_any_audio;
  f32 predicted_playback_start_time;
} TTSQueueItem;

typedef struct {
  TTSQueueItem *items;
  u32 head;
  u32 tail;
  u32 count;
  u32 capacity;
} TTSQueue;

typedef struct {
  char_Slice tts_text_acc_buffer;
  TTSQueue tts_queue;
  u32 min_phrase_len;
  u32 max_phrase_len;
  b32 audio_play_enabled;
  u32 min_emotion_text_length;
} TextToSpeechSystem;

void tts_init(TextToSpeechSystem *tts_system, GameContext *ctx);

void tts_update(TextToSpeechSystem *tts_system, HttpStreamChunk resp,
                AudioState *audio_system, StreamingAudioClip *main_audio_clip,
                const char *instructions, char_Slice full_context,
                GameContext *ctx);

// Get current emotion from TTS head item (returns EMOTION_TAGS_NEUTRAL if not ready)
EMOTION_TAGS tts_get_current_emotion(TextToSpeechSystem *tts_system);

// Check if current TTS head has detected emotion ready
bool32 tts_current_emotion_ready(TextToSpeechSystem *tts_system);

#endif

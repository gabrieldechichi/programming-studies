#ifndef H_CONVERSATION_SYSTSEM
#define H_CONVERSATION_SYSTSEM

#include "lib/string.h"
#include "stt_system.h"
#include "tts_system.h"

typedef struct {
  String role;
  String content;
} ConversationMessage;
slice_define(ConversationMessage);

typedef ConversationMessage_Slice ConversationHistory;

typedef struct {
  SpeechToTextSystem stt_system;
  TextToSpeechSystem tts_system;

  char_Slice ai_response_buffer;
  HttpStreamRequest conversation_stream_op;
  b32 is_streaming_llm_response;
  StreamingAudioClip *main_audio_clip;

  ConversationHistory history;
} ConversationSystem;

ConversationSystem conversation_system_init(GameContext *ctx,
                                            AudioState *audio_system);

void conversation_system_update(ConversationSystem *conversation,
                                GameContext *ctx, f32 dt,
                                AudioState *audio_system);

void send_conversation_request(ConversationSystem *conversation,
                               GameContext *ctx);

b32 conversation_is_user_speaking(ConversationSystem *conversation);
b32 conversation_is_ai_speaking(ConversationSystem *conversation);
b32 conversation_is_processing(ConversationSystem *conversation);

void conversation_history_add_assistant_message(ConversationHistory *history,
                                                String content,
                                                Allocator *allocator);

#endif // !H_CONVERSATION_SYSTSEM

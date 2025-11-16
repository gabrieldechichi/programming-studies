#include "conversation_system.h"
#include "config.h"
#include "game.h"
#include "gameplay_lib.c"
#include "lib/array.h"
#include "lib/audio.h"
#include "lib/fmt.h"
#include "lib/http.h"
#include "lib/json_serializer.h"
#include "lib/memory.h"
#include "lib/typedefs.h"
#include "platform.h"
#include "stt_system.h"
#include "system_prompt.h"
#include "tts_system.h"
#include "ui_bridge.h"

void halt_all_tts_streaming(ConversationSystem *conversation, GameContext *ctx);

void conversation_history_init(ConversationHistory *history,
                               Allocator *allocator) {
  *history = slice_new_ALLOC(allocator, ConversationMessage, 64);

  // Add system prompt as first message
  const char *system_prompt = (const char *)system_prompt_txt;
  u32 system_prompt_len = system_prompt_txt_len;

  ConversationMessage system_prompt_msg = {0};
  system_prompt_msg.role = str_from_cstr_alloc("system", 6, allocator);
  system_prompt_msg.content =
      str_from_cstr_alloc(system_prompt, system_prompt_len, allocator);
  slice_append(*history, system_prompt_msg);
}

void conversation_history_add_message(ConversationHistory *history,
                                      const char *role, String content,
                                      Allocator *allocator) {
  if (history->len >= history->cap) {
    LOG_WARN("Conversation history full, cannot add message");
    return;
  }

  ConversationMessage msg = {0};
  u32 role_len = str_len(role);
  msg.role = str_from_cstr_alloc(role, role_len, allocator);
  msg.content = str_from_cstr_alloc(content.value, content.len, allocator);
  slice_append(*history, msg);
}

void conversation_history_add_user_message(ConversationHistory *history,
                                           String content,
                                           Allocator *allocator) {
  conversation_history_add_message(history, "user", content, allocator);
}

void conversation_history_add_assistant_message(ConversationHistory *history,
                                                String content,
                                                Allocator *allocator) {
  conversation_history_add_message(history, "assistant", content, allocator);
}

char *conversation_history_to_json(ConversationHistory *history,
                                   Allocator *allocator) {
  // todo: need to calculate proper json size
  u32 estimated_size = 512;
  for (u32 i = 0; i < history->len; i++) {
    estimated_size +=
        history->items[i].content.len + history->items[i].role.len + 64;
  }

  JsonSerializer serializer = json_serializer_init(allocator, estimated_size);
  write_object_start(&serializer);
  write_key(&serializer, "messages");
  write_array_start(&serializer);

  for (u32 i = 0; i < history->len; i++) {
    if (i > 0) {
      write_comma(&serializer);
    }

    ConversationMessage *msg = &history->items[i];
    write_object_start(&serializer);
    write_key(&serializer, "role");
    serialize_string_value(&serializer, msg->role.value);
    write_comma(&serializer);
    write_key(&serializer, "content");
    serialize_string_value(&serializer, msg->content.value);
    write_object_end(&serializer);
  }

  write_array_end(&serializer);
  write_object_end(&serializer);
  return json_serializer_finalize(&serializer);
}

void send_conversation_request(ConversationSystem *conversation,
                               GameContext *ctx) {
  char *json_body = conversation_history_to_json(&conversation->history,
                                                 &ctx->temp_allocator);

  LOG_INFO("Sending conversation request with JSON: %", FMT_STR(json_body));

  // todo: backend configuration needed instead of hardcoded url
  const char *headers = "Content-Type: application/json";
  conversation->conversation_stream_op =
      http_stream_post_async(BACKEND_URL "/tomoChat/conversation/stream-text",
                             headers, json_body, &ctx->temp_allocator);
  conversation->is_streaming_llm_response = true;
}

void halt_all_tts_streaming(ConversationSystem *conversation,
                            GameContext *ctx) {
  LOG_INFO("Halting all streaming");
  TextToSpeechSystem *tts_system = &conversation->tts_system;
  // make sur to add ai response to history anyway
  // todo: we should do that at the last moment possible (when we receive
  // another AI message)
  if (conversation->ai_response_buffer.len > 0) {
    String ai_response = str_from_cstr_alloc(
        conversation->ai_response_buffer.items,
        conversation->ai_response_buffer.len, &ctx->allocator);
    conversation_history_add_assistant_message(&conversation->history,
                                               ai_response, &ctx->allocator);
    conversation->ai_response_buffer.len = 0;
    LOG_INFO("Added AI response to history (% chars)",
             FMT_UINT(ai_response.len));
  }
  conversation->ai_response_buffer.len = 0;
  conversation->conversation_stream_op = (HttpStreamRequest){0};
  conversation->is_streaming_llm_response = false;

  tts_system->tts_text_acc_buffer.len = 0;
  for (u32 i = 0; i < tts_system->tts_queue.capacity; i++) {
    tts_system->tts_queue.items[i].total_audio_data_len = 0;
    tts_system->tts_queue.items[i].pending_audio_data.len = 0;
    tts_system->tts_queue.items[i].tts_started = false;
    tts_system->tts_queue.items[i].tts_complete = true;
    tts_system->tts_queue.items[i].tts_request = (HttpStreamRequest){0};
  }
  tts_system->tts_queue.head = 0;
  tts_system->tts_queue.tail = 0;
  tts_system->tts_queue.count = 0;
  streaming_clip_reset(conversation->main_audio_clip);
}

ConversationSystem conversation_system_init(GameContext *ctx,
                                            AudioState *audio_system) {
  ConversationSystem conversation = {0};

  stt_init(&conversation.stt_system, ctx);

  tts_init(&conversation.tts_system, ctx);

  conversation.ai_response_buffer =
      slice_new_ALLOC(&ctx->allocator, char, 4096);

  // Initialize conversation history with system prompt
  conversation_history_init(&conversation.history, &ctx->allocator);

  // Create single main streaming audio clip that will never stop
  u32 sample_rate = 24000;
  StreamingAudioClip main_clip = streaming_clip_create(
      sample_rate, 1, sample_rate * sizeof(i16) * 60, ctx);
  conversation.main_audio_clip =
      audio_play_streaming_clip(audio_system, main_clip);
  return conversation;
}

void conversation_system_update(ConversationSystem *conversation,
                                GameContext *ctx, f32 dt,
                                AudioState *audio_system) {
  SpeechToTextSystem *stt_system = &conversation->stt_system;
  stt_update(stt_system, dt, ctx);

  if (stt_system->should_halt_tts) {
    halt_all_tts_streaming(conversation, ctx);
  }

  // check for new STT results and trigger conversation
  if (stt_system->has_new_result) {
    String user_message = stt_get_result(stt_system, &ctx->temp_allocator);

    if (user_message.len > 0) {
      // clean up conversation req
      halt_all_tts_streaming(conversation, ctx);

      LOG_INFO("STT transcribed: '%'", FMT_STR(user_message.value));

      ui_show_last_message(user_message.value);

      conversation_history_add_user_message(&conversation->history,
                                            user_message, &ctx->allocator);

      send_conversation_request(conversation, ctx);
    }
  }

  // check for UI chat messages
  if (ui_has_chat_messages()) {
    String message = ui_chat_message_pop(&ctx->temp_allocator);

    if (message.len > 0) {

      // clean up conversation req
      halt_all_tts_streaming(conversation, ctx);

      LOG_INFO("Received chat message: '%'", FMT_STR(message.value));

      // Show the user's message in the UI for 3 seconds
      ui_show_last_message(message.value);

      // Add user message to conversation history
      conversation_history_add_user_message(&conversation->history, message,
                                            &ctx->allocator);

      // Send conversation request with full history
      send_conversation_request(conversation, ctx);
    }
  }

  // Handle conversation text streaming
  HttpStreamRequest *conversation_req = &conversation->conversation_stream_op;
  HttpStreamChunk resp = {0};
  if (http_stream_has_chunk(conversation_req)) {
    resp = http_stream_get_chunk(conversation_req);

    u32 prev_ai_len = conversation->ai_response_buffer.len;
    slice_increase_len(conversation->ai_response_buffer, resp.chunk_len);
    memcpy(conversation->ai_response_buffer.items + prev_ai_len,
           resp.chunk_data, resp.chunk_len);

    if (resp.is_final_chunk && conversation->ai_response_buffer.len > 0) {
      String ai_response = str_from_cstr_alloc(
          conversation->ai_response_buffer.items,
          conversation->ai_response_buffer.len, &ctx->allocator);
      conversation_history_add_assistant_message(&conversation->history,
                                                 ai_response, &ctx->allocator);
      conversation->ai_response_buffer.len = 0;
      conversation->is_streaming_llm_response = false;
      LOG_INFO("Added AI response to history (% chars)",
               FMT_UINT(ai_response.len));
    }
  } else if (http_stream_is_complete(conversation_req)) {
    conversation->ai_response_buffer.len = 0;
    conversation->is_streaming_llm_response = false;
  }

  const char *tts_instructions =
      "You are Anya Forger, a 6-year-old telepathic girl from Spy x Family.";
  tts_update(&conversation->tts_system, resp, audio_system,
             conversation->main_audio_clip, tts_instructions,
             conversation->ai_response_buffer, ctx);
}

b32 conversation_is_user_speaking(ConversationSystem *conversation) {
  SpeechToTextSystem *stt = &conversation->stt_system;
  return stt->is_actively_recording;
}

b32 conversation_is_ai_speaking(ConversationSystem *conversation) {
  return streaming_clip_has_audio_content(conversation->main_audio_clip);
}

b32 conversation_is_processing(ConversationSystem *conversation) {
  b32 is_processing = conversation->is_streaming_llm_response;
  is_processing |=
      http_stream_is_ready(&conversation->conversation_stream_op) &&
      !http_stream_is_complete(&conversation->conversation_stream_op) &&
      !http_stream_has_error(&conversation->conversation_stream_op);

  TextToSpeechSystem *tts = &conversation->tts_system;

  is_processing |= tts->tts_queue.count > 0;

  return is_processing;
}

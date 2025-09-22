#include "tts_system.h"
#include "config.h"
#include "lib/audio.h"
#include "lib/json_serializer.h"
#include "lib/string.h"

bool32 is_punctuation(char c) { return c == '.' || c == '!' || c == '?'; }

b32 is_space(char c) { return c == ' ' || c == '\n'; }

void tts_send_emotion_detection_request(TTSQueueItem *item, GameContext *ctx) {
  u32 estimated_size = 256 + item->text.len;
  JsonSerializer serializer =
      json_serializer_init(&ctx->temp_allocator, estimated_size);

  write_object_start(&serializer);
  write_key(&serializer, "text");
  serialize_string_value(&serializer, item->text.value);
  write_object_end(&serializer);

  char *json_body = json_serializer_finalize(&serializer);

  const char *headers = "Content-Type: application/json";
  item->emotion_request =
      http_post_async(BACKEND_URL "/tomoChat/conversation/detect-emotion",
                      headers, json_body, &ctx->temp_allocator);
  item->emotion_request_pending = true;

  LOG_INFO("Sent emotion detection request for TTS text: %",
           FMT_STR(item->text.value));
}

void tts_process_emotion_response(TTSQueueItem *item) {
  if (!item->emotion_request_pending ||
      !http_request_is_complete(&item->emotion_request)) {
    return;
  }

  HttpResponse response = http_request_get_response(&item->emotion_request);

  if (!response.success) {
    LOG_WARN("Emotion detection request failed: %",
             FMT_STR(response.error_message ? response.error_message
                                            : "Unknown error"));
    item->detected_emotion = EMOTION_TAGS_NEUTRAL;
  } else {
    LOG_INFO("Emotion detection response: %", FMT_STR(response.body));

    EMOTION_TAGS detected_emotion = EMOTION_TAGS_NEUTRAL;
    for (u32 i = 0; i < EMOTION_TAGS_MAX; i++) {
      if (str_contains(response.body, emotion_tags[i])) {
        detected_emotion = (EMOTION_TAGS)i;
        break;
      }
    }
    item->detected_emotion = detected_emotion;
    LOG_INFO("Detected emotion: %", FMT_STR(emotion_tags[detected_emotion]));
  }

  item->emotion_detected = true;
  item->emotion_request_pending = false;
  item->emotion_request = (HttpRequest){0};
}

void tts_queue_add_phrase(TTSQueue *queue, const char *text, u32 text_len,
                          const char *instructions, char_Slice full_context,
                          GameContext *ctx) {
  if (queue->count >= queue->capacity) {
    LOG_WARN("TTS queue is full, dropping phrase");
    return;
  }

  TTSQueueItem *item = &queue->items[queue->tail];

  item->tts_started = true;
  item->tts_complete = false;

  item->text = str_from_cstr_alloc(text, text_len, &ctx->allocator);

  // create TTS request using JSON serializer
  // todo: separate function here, emulating code gen
  u32 estimated_size = text_len + 512 + full_context.len;
  JsonSerializer serializer =
      json_serializer_init(&ctx->temp_allocator, estimated_size);
  write_object_start(&serializer);
  write_key(&serializer, "text");
  serialize_string_value(&serializer, item->text.value);
  if (instructions) {
    write_comma(&serializer);
    write_key(&serializer, "instructions");
    serialize_string_value(&serializer, instructions);
  }
  if (full_context.len > 0) {
    write_comma(&serializer);
    write_key(&serializer, "context");
    serialize_string_value_len(&serializer, full_context.items,
                               full_context.len);
  }
  write_object_end(&serializer);
  char *json_body = json_serializer_finalize(&serializer);

  const char *headers = "Content-Type: application/json";
  item->tts_request =
      http_stream_post_async(BACKEND_URL "/tomoChat/conversation/stream-tts",
                             headers, json_body, &ctx->temp_allocator);

  item->pending_audio_data.len = 0;

  // Initialize emotion detection fields
  item->detected_emotion = EMOTION_TAGS_NEUTRAL;
  item->emotion_request_pending = false;
  item->emotion_request = (HttpRequest){0};
  item->emotion_detected = false;
  item->did_play_any_audio = false;
  item->predicted_playback_start_time = 0.0f;

  // Send emotion detection request immediately
  tts_send_emotion_detection_request(item, ctx);

  queue->tail = (queue->tail + 1) % queue->capacity;
  queue->count++;

  LOG_INFO("Added phrase to TTS queue: '%'", FMT_STR(item->text.value));
}

void tts_queue_update(TTSQueue *queue, AudioState *audio_system,
                      StreamingAudioClip *main_audio_clip,
                      b32 audio_play_enabled) {

  // accumulate tts streams to audio buffers
  for (u32 i = 0; i < queue->count; i++) {
    u32 idx = (queue->head + i) % queue->capacity;
    TTSQueueItem *item = &queue->items[idx];

    if (item->tts_started && !item->tts_complete) {
      HttpStreamRequest *req = &item->tts_request;
      b32 is_ready = http_stream_is_ready(req);
      if (!is_ready) {
        continue;
      }

      // check for HTTP errors
      if (req->has_error || (req->stream_complete && req->status_code >= 400)) {
        LOG_WARN(
            "TTS request failed for phrase '%s': status %d, error: %s",
            FMT_STR(item->text.value), FMT_INT(req->status_code),
            FMT_STR(req->error_message ? req->error_message : "Unknown error"));
        item->tts_complete = true;
        // don't process any audio from failed request
        item->pending_audio_data.len = 0;
        item->total_audio_data_len = 0;
        continue;
      }

      if (http_stream_has_chunk(req)) {
        HttpStreamChunk resp = http_stream_get_chunk(req);

        item->total_audio_data_len += resp.chunk_len;
        u32 prev_len = item->pending_audio_data.len;
        // todo: proper slice function here
        slice_increase_len(item->pending_audio_data, resp.chunk_len);
        memcpy(item->pending_audio_data.items + prev_len, resp.chunk_data,
               resp.chunk_len);

        if (resp.is_final_chunk) {
          item->tts_complete = true;
          // LOG_INFO("TTS complete for phrase: '%s'", FMT_STR(item->text));
        }
      }

      // note: makes double sure tts is marked as complete if stream is complete
      if (req->stream_complete) {
        item->tts_complete = true;
      }
    }
  }

  // Process emotion detection responses for all queue items
  for (u32 i = 0; i < queue->count; i++) {
    u32 idx = (queue->head + i) % queue->capacity;
    TTSQueueItem *item = &queue->items[idx];

    if (item->emotion_request_pending && !item->emotion_detected) {
      tts_process_emotion_response(item);
    }
  }

  // process head item - write to main streaming clip if space available
  if (queue->count > 0) {
    TTSQueueItem *head_item = &queue->items[queue->head];

    // if we've written all data and TTS is complete, remove this item
    // note: we do this here in the beginning so there is one frame delay
    // between finish processing a queue item and going to the next one
    if (head_item->pending_audio_data.len == 0 && head_item->tts_complete) {
      head_item->total_audio_data_len = 0;
      queue->head = (queue->head + 1) % queue->capacity;
      queue->count--;
    }

    const u32 min_audio_frames_to_start_playing = 30;
    if (audio_play_enabled && head_item->pending_audio_data.len > 0 &&
        head_item->total_audio_data_len >
            audio_system->sample_buffer_len *
                min_audio_frames_to_start_playing &&
        head_item->emotion_detected) {
      u32 available_space =
          streaming_buffer_available_space(&main_audio_clip->pcm_buffer);

      if (available_space > 0) {
        if (!head_item->did_play_any_audio) {
          u32 buffered_data_bytes =
              streaming_buffer_available_data_len(&main_audio_clip->pcm_buffer);
          u32 bytes_per_sample = 2 * main_audio_clip->channels;
          head_item->predicted_playback_start_time =
              (f32)buffered_data_bytes /
              (bytes_per_sample * main_audio_clip->source_sample_rate);
        }
        head_item->did_play_any_audio = true;
        u32 bytes_to_write =
            (head_item->pending_audio_data.len < available_space)
                ? head_item->pending_audio_data.len
                : available_space;

        streaming_clip_write_pcm(main_audio_clip,
                                 head_item->pending_audio_data.items,
                                 bytes_to_write);

        // remove written data from pending buffer
        // todo: proper slice function here
        head_item->pending_audio_data.len -= bytes_to_write;
        if (head_item->pending_audio_data.len > 0) {
          memmove(head_item->pending_audio_data.items,
                  head_item->pending_audio_data.items + bytes_to_write,
                  head_item->pending_audio_data.len);
        }
      }
    }
  }
}

void tts_init(TextToSpeechSystem *tts_system, GameContext *ctx) {
  tts_system->tts_text_acc_buffer =
      slice_new_ALLOC(&ctx->allocator, char, 4096);
  tts_system->max_phrase_len = 1024;
  tts_system->min_phrase_len = 30;

  tts_system->tts_queue.capacity = 16;
  tts_system->tts_queue.items = ALLOC_ARRAY(&ctx->allocator, TTSQueueItem,
                                            tts_system->tts_queue.capacity);
  tts_system->audio_play_enabled = true;
  tts_system->min_emotion_text_length = 10;

  // initialize buffers for all items
  for (u32 i = 0; i < tts_system->tts_queue.capacity; i++) {
    TTSQueueItem *item = &tts_system->tts_queue.items[i];
    item->pending_audio_data = slice_new_ALLOC(&ctx->allocator, u8, MB(4));
  }
}

void tts_update(TextToSpeechSystem *tts_system, HttpStreamChunk resp,
                AudioState *audio_system, StreamingAudioClip *main_audio_clip,
                const char *instructions, char_Slice full_context,
                GameContext *ctx) {
  b32 should_trigger_tts = false;
  i32 phrase_end = -1;
  if (resp.chunk_len > 0) {
    // todo: proper slice_append_multi macro or something
    u32 prev_len = tts_system->tts_text_acc_buffer.len;
    slice_increase_len(tts_system->tts_text_acc_buffer, resp.chunk_len);
    memcpy(tts_system->tts_text_acc_buffer.items + prev_len, resp.chunk_data,
           resp.chunk_len);

    // check if we should trigger tts
    const char *text = tts_system->tts_text_acc_buffer.items;
    u32 len = tts_system->tts_text_acc_buffer.len;
    if (len < tts_system->min_phrase_len) {
      should_trigger_tts = false;
    }
    // force break if too long
    else if (len >= tts_system->max_phrase_len) {
      should_trigger_tts = true;
      phrase_end = tts_system->max_phrase_len - 1;
    } else {
      // look for punctuation followed by space or end
      for (u32 i = tts_system->min_phrase_len; i < len; i++) {
        if (is_punctuation(text[i])) {
          if (i == len - 1 || is_space(text[i + 1])) {
            should_trigger_tts = true;
            phrase_end = i + 1;
            break;
          }
        }
      }
    }
  }

  if (should_trigger_tts && phrase_end > 0) {
    tts_queue_add_phrase(&tts_system->tts_queue,
                         tts_system->tts_text_acc_buffer.items, phrase_end,
                         instructions, full_context, ctx);

    // Remove processed text from buffer
    // todo: proper slice function here?
    u32 remaining_len = tts_system->tts_text_acc_buffer.len - phrase_end;
    memmove(tts_system->tts_text_acc_buffer.items,
            tts_system->tts_text_acc_buffer.items + phrase_end, remaining_len);
    tts_system->tts_text_acc_buffer.len = remaining_len;
  }

  if (resp.is_final_chunk) {
    // LOG_INFO("Final text chunk received");
    // Process any remaining text
    if (tts_system->tts_text_acc_buffer.len > 0) {
      tts_queue_add_phrase(
          &tts_system->tts_queue, tts_system->tts_text_acc_buffer.items,
          tts_system->tts_text_acc_buffer.len, instructions, full_context, ctx);
      tts_system->tts_text_acc_buffer.len = 0;
    }
  }

  tts_queue_update(&tts_system->tts_queue, audio_system, main_audio_clip,
                   tts_system->audio_play_enabled);
}

EMOTION_TAGS tts_get_current_emotion(TextToSpeechSystem *tts_system) {
  if (tts_system->tts_queue.count == 0) {
    return EMOTION_TAGS_NEUTRAL;
  }

  TTSQueueItem *head_item =
      &tts_system->tts_queue.items[tts_system->tts_queue.head];
  if (!head_item->emotion_detected) {
    return EMOTION_TAGS_NEUTRAL;
  }

  return head_item->detected_emotion;
}

bool32 tts_current_emotion_ready(TextToSpeechSystem *tts_system) {
  if (tts_system->tts_queue.count == 0) {
    return false;
  }

  TTSQueueItem *head_item =
      &tts_system->tts_queue.items[tts_system->tts_queue.head];
  return head_item->emotion_detected && head_item->did_play_any_audio;
}

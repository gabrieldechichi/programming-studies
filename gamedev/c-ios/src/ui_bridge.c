#include "ui_bridge.h"
#include "game.h"
#include "stats.h"

#if defined(__wasm__)

// External declarations for WASM imports
extern bool32 _platform_ui_has_chat_messages();
extern int32 _platform_ui_chat_message_pop(char *buffer, int32 buffer_len);
extern void _platform_ui_show_last_message(const char *message);
extern void _platform_ui_hide_last_message();
extern void _platform_ui_set_stats(UIStats *stats);

bool32 ui_has_chat_messages() { return _platform_ui_has_chat_messages(); }

String ui_chat_message_pop(Allocator *allocator) {
  // Use a temporary buffer to get the message from JS
  // todo: receive context and use temp allocator
  char temp_buffer[512];
  int32 message_len =
      _platform_ui_chat_message_pop(temp_buffer, sizeof(temp_buffer));

  if (message_len <= 0) {
    // Return empty string if no message or error
    return (String){.value = NULL, .len = 0};
  }

  // Allocate and copy to a proper String
  return str_from_cstr_alloc(temp_buffer, message_len, allocator);
}

void ui_show_last_message(const char *message) {
  _platform_ui_show_last_message(message);
}

void ui_set_stats(GameStats *game_stats) {
  UIStats out_stats = {0};
  out_stats.dt_avg = game_stats->dt_avg;
  out_stats.temp_memory_used = game_stats->temp_memory_used;
  out_stats.temp_memory_total = game_stats->temp_memory_total;
  out_stats.memory_used = game_stats->memory_used;
  out_stats.memory_total = game_stats->memory_total;
  _platform_ui_set_stats(&out_stats);
}

#else

// Stub implementations for non-WASM builds
bool32 ui_has_chat_messages() { return false; }

String ui_chat_message_pop(Allocator *allocator) {
  return (String){.value = NULL, .len = 0};
}

void ui_show_last_message(const char *message) {
  // Stub - do nothing
}

void ui_hide_last_message() {
  // Stub - do nothing
}

void ui_get_stats(GameStats *game_stats, GameContext *ctx, UIStats *out_stats) {
  // Stub - do nothing
}

#endif
#ifndef H_UI_BRIDGE
#define H_UI_BRIDGE

#include "game.h"
#include "lib/memory.h"
#include "lib/string.h"
#include "lib/typedefs.h"
#include "stats.h"

/*!
 * UI Bridge - Communication from JavaScript UI to C code
 */

bool32 ui_has_chat_messages();

String ui_chat_message_pop(Allocator *allocator);

void ui_show_last_message(const char *message);

typedef struct {
  f32 dt_avg;
  u32 temp_memory_used;
  u32 temp_memory_total;
  u32 memory_used;
  u32 memory_total;
} UIStats;

void ui_set_stats(GameStats *game_stats);

#endif
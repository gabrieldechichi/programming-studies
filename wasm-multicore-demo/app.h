#ifndef H_APP
#define H_APP
#include "input.h"
#include "lib/memory.h"

typedef struct {
  f32 dt;
  f32 total_time;
  f32 canvas_width;
  f32 canvas_height;
  f32 dpr;

  AppInputEvents input_events;

  u8 *heap;
  size_t heap_size;
} AppMemory;

void app_init(AppMemory* memory);
void app_update_and_render(AppMemory* memory);
#endif

#include "context.h"
#include "lib/array.h"
#include "lib/multicore_runtime.h"
#include "lib/thread_context.h"
#include "lib/typedefs.h"
#include "os/os.h"
#include "lib/math.h"
#include "cube.h"
#include "renderer.h"
#include "camera.h"
#include "app.h"

arr_define(i64);

#define NUMBERS_COUNT 10000000

typedef struct {
  i64_Array numbers;
  u64 values_start;
} TaskWideSumInitData;

typedef struct {
  i64_Array numbers;
  i64 lane_sum;
} TaskWideSumExecData;

void task_wide_sum_init(void *arg) {
  TaskWideSumInitData *data = (TaskWideSumInitData *)arg;
  for (u32 i = 0; i < data->numbers.len; i++) {
    data->numbers.items[i] = data->values_start + i;
  }
}

void task_wide_sum_exec(void *arg) {
  TaskWideSumExecData *data = (TaskWideSumExecData *)arg;
  i64 sum = 0;
  arr_foreach(data->numbers, i64, value) { sum += value; }
  data->lane_sum = sum;
}

void app_init(AppMemory *memory) {
  UNUSED(memory);

  ThreadContext *tctx = tctx_current();
  AppContext *app_ctx = app_ctx_current();

  local_shared MCRTaskQueue mcr_queue = {0};
  local_shared u64 array_size = NUMBERS_COUNT;
  local_shared i64 *array = NULL;
  local_shared TaskWideSumExecData *sum_lane_data = NULL;

  if (is_main_thread()) {
    array = ARENA_ALLOC_ARRAY(&app_ctx->arena, i64, NUMBERS_COUNT);
    sum_lane_data =
        ARENA_ALLOC_ARRAY(&app_ctx->arena, TaskWideSumExecData, tctx->thread_count);
  }
  lane_sync();

  Range_u64 range = lane_range(array_size);
  i64_Array numbers = {
      .items = array + range.min,
      .len = (u32)(range.max - range.min),
  };

  TaskWideSumInitData *init_data =
      ARENA_ALLOC(&tctx->temp_arena, TaskWideSumInitData);
  *init_data = (TaskWideSumInitData){
      .numbers = numbers,
      .values_start = range.min + 1,
  };

  MCRTaskHandle init_handle =
      mcr_queue_append(&mcr_queue, task_wide_sum_init, init_data, NULL, 0, NULL, 0);

  sum_lane_data[tctx->thread_idx] = (TaskWideSumExecData){
      .numbers = numbers,
      .lane_sum = 0,
  };

  MCRTaskHandle deps[1] = {init_handle};
  mcr_queue_append(&mcr_queue, task_wide_sum_exec,
                   &sum_lane_data[tctx->thread_idx], NULL, 0, deps, 1);

  mcr_queue_process(&mcr_queue);

  if (is_main_thread()) {
    i64 sum = 0;
    for (u8 i = 0; i < tctx->thread_count; i++) {
      sum += sum_lane_data[i].lane_sum;
    }
    LOG_INFO("Sum result: %", FMT_UINT(sum));
  }
}

void app_update_and_render(AppMemory *memory) {
  UNUSED(memory);
}

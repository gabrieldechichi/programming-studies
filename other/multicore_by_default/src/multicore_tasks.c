#include "lib/array.h"
#include "lib/thread_context.h"
#include "lib/typedefs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lib/task.h"
#include "multicore_tasks_generated.h"

#define NUMBERS_COUNT 1000000000
// #define NUMBERS_COUNT 1000000

void TaskWideSumInit_Exec(TaskWideSumInit *data) {
  for (u64 i = 0; i < data->numbers.len; i++) {
    data->numbers.items[i] = data->values_start + i;
  }

  // note: just to test race conditions
  // if (tctx_current()->thread_idx == 2 || tctx_current()->thread_idx == 3) {
  //   for (u64 i = 0; i < 1000000000; i++) {
  //     cpu_pause();
  //   }
  // }
}

void TaskWideSumExec_Exec(TaskWideSumExec *data) {
  i64 sum = 0;

  arr_foreach(data->numbers, i64, value) { sum += value; }
  data->lane_sum = sum;
}

void entrypoint() {
  local_shared TaskQueue task_queue = {0};
  local_shared u64 array_size = NUMBERS_COUNT;
  local_shared i64 *array = NULL;
  local_shared TaskWideSumExec *sum_lane_data = NULL;

  ThreadContext *tctx = tctx_current();

  if (is_main_thread()) {
    array = malloc(NUMBERS_COUNT * sizeof(i64));
    sum_lane_data = calloc(1, tctx->thread_count * sizeof(TaskWideSumExec));
  }
  lane_sync();

  // create array views (shared)
  Range_u64 range = lane_range(array_size);
  i64_Array numbers = arr_view_from_range(i64, array, range);

  //todo: pass struct and move arena copy elsewhere (schedule?)
  //for sharing data you should add an attribute
  TaskWideSumInit *init_data =
      arena_alloc(&tctx->temp_arena, sizeof(TaskWideSumInit));
  *init_data = (TaskWideSumInit){
      .numbers = numbers,
      .values_start = range.min + 1,
  };

  printf("schedule init task %d\n", tctx_current()->thread_idx);
  TaskHandle init_task_handle =
      _TaskWideSumInit_Schedule(&task_queue, init_data, NULL, 0);

  sum_lane_data[tctx->thread_idx] = (TaskWideSumExec){
      .numbers = numbers,
      .lane_sum = 0,
  };

  printf("schedule sum task %d\n", tctx_current()->thread_idx);
  _TaskWideSumExec_Schedule(&task_queue, &sum_lane_data[tctx->thread_idx],
                            &init_task_handle, 1);

  task_queue_process(&task_queue);

  if (is_main_thread()) {
    u64 sum = 0;
    for (u8 i = 0; i < tctx->thread_count; i++) {
      sum += sum_lane_data[i].lane_sum;
    }
    printf("sum %lld", sum);
  }
}

void *entrypoint_internal(void *arg) {
  ThreadContext *ctx = (ThreadContext *)arg;
  tctx_set_current(ctx);

  entrypoint();
  return NULL;
}

int main(void) {
  const u8 thread_mult = 1;
  i8 core_count = os_core_count();
  u8 thread_count = core_count * thread_mult;

  printf("Core count %d Thread count %d\n", core_count, thread_count);

  Thread *threads = malloc(thread_count * sizeof(Thread));
  ThreadContext *thread_ctx_arr = malloc(thread_count * sizeof(ThreadContext));
  Barrier barrier;
  barrier_init(&barrier, thread_count);

  u64 broadcast_memory = 0;
  for (u8 i = 0; i < thread_count; i++) {
    thread_ctx_arr[i] = (ThreadContext){
        .thread_idx = i,
        .thread_count = thread_count,
        .barrier = &barrier,
        .broadcast_memory = &broadcast_memory,
        .temp_arena = arena_from_buffer(calloc(1, MB(8)), MB(8)),
    };
    threads[i] = thread_create(entrypoint_internal, &thread_ctx_arr[i]);
  }

  for (u8 i = 0; i < thread_count; i++) {
    thread_join(threads[i]);
  }

  return 0;
}

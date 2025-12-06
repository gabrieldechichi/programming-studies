#include "lib/array.h"
#include "lib/thread_context.h"
#include "lib/typedefs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lib/multicore_runtime.h"
#include "multicore_tasks_generated.h"

#define NUMBERS_COUNT 1000000000
// #define NUMBERS_COUNT 1000000

void TaskWideSumInit_Exec(TaskWideSumInit *data)
{
  for (u64 i = 0; i < data->numbers.len; i++)
  {
    data->numbers.items[i] = data->values_start + i;
  }

  // note: just to test race conditions
  // if (tctx_current()->thread_idx == 2 || tctx_current()->thread_idx == 3) {
  //   for (u64 i = 0; i < 1000000000; i++) {
  //     cpu_pause();
  //   }
  // }
}

void TaskWideSumExec_Exec(TaskWideSumExec *data)
{
  i64 sum = 0;

  arr_foreach(data->numbers, i64, value) { sum += value; }
  data->lane_sum = sum;
}

void entrypoint()
{
  local_shared MCRTaskQueue mcr_queue = {0};
  local_shared u64 array_size = NUMBERS_COUNT;
  local_shared i64 *array = NULL;
  local_shared TaskWideSumExec *sum_lane_data = NULL;

  ThreadContext *tctx = tctx_current();

  if (is_main_thread())
  {
    array = malloc(NUMBERS_COUNT * sizeof(i64));
    sum_lane_data = calloc(1, tctx->thread_count * sizeof(TaskWideSumExec));
  }
  lane_sync();

  // create array views (shared)
  Range_u64 range = lane_range(array_size);
  i64_Array numbers = arr_view_from_range(i64, array, range);

  // todo: pass struct and move arena copy elsewhere (schedule?)
  // for sharing data you should add an attribute
  TaskWideSumInit *init_data =
      arena_alloc(&tctx->temp_arena, sizeof(TaskWideSumInit));
  *init_data = (TaskWideSumInit){
      .numbers = numbers,
      .values_start = range.min + 1,
  };

  MCRTaskHandle init_mcr_handle =
      TaskWideSumInit_Schedule(&mcr_queue, init_data);

  sum_lane_data[tctx->thread_idx] = (TaskWideSumExec){
      .numbers = numbers,
      .lane_sum = 0,
  };
  MCRTaskHandle first_sum_handle = TaskWideSumExec_Schedule(
      &mcr_queue, &sum_lane_data[tctx->thread_idx], init_mcr_handle);

  TaskWideSumExec_Schedule(&mcr_queue, &sum_lane_data[tctx->thread_idx],
                           init_mcr_handle, first_sum_handle);

  mcr_queue_process(&mcr_queue);

  if (is_main_thread())
  {
    u64 sum = 0;
    for (u8 i = 0; i < tctx->thread_count; i++)
    {
      sum += sum_lane_data[i].lane_sum;
    }
    printf("sum %lld", sum);
  }
}

void *entrypoint_internal(void *arg)
{
  ThreadContext *ctx = (ThreadContext *)arg;
  tctx_set_current(ctx);

  entrypoint();
  return NULL;
}

int main(void)
{
  const u8 thread_mult = 1;
  os_init();
  i8 core_count = os_core_count();
  u8 thread_count = core_count * thread_mult;

  printf("Core count %d Thread count %d\n", core_count, thread_count);
  mcr_run(thread_count, MB(8), entrypoint);

  return 0;
}

#include "thread_context.h"
#include "typedefs.h"
#include <stdio.h>
#include <stdlib.h>

#define ARRAY_SIZE 1000000000
// #define ARRAY_SIZE 1000000

typedef void (*TaskFunc)(void *);

typedef struct {
  TaskFunc task_func;
  void *user_data;
} Task;

typedef struct {
  i64 *array;
  Range_u64 range;
} TaskWideSumInitData;

typedef struct {
  i64 *array;
  Range_u64 range;
  i64 lane_sum;
} TaskWideSumData;

void task_sum_init(void *_data) {
  TaskWideSumInitData *data = (TaskWideSumInitData *)_data;
  for (u64 i = data->range.min; i < data->range.max; i++) {
    data->array[i] = i + 1;
  }
}

void task_sum_exec(void *_data) {
  TaskWideSumData *data = (TaskWideSumData *)_data;
  i64 sum = 0;

  for (u64 i = data->range.min; i < data->range.max; i++) {
    sum += data->array[i];
  }
  data->lane_sum = sum;
}

typedef struct {
  Task tasks_ptr[128];
  u8 task_counter;
  u8 tasks_count;
} TaskQueue;

void entrypoint() {
  static TaskQueue task_queue = {0};
  static TaskWideSumInitData *init_data = NULL;
  u64 array_size = ARRAY_SIZE;
  static i64 *array = NULL;

  if (is_main_thread()) {
    init_data =
        calloc(1, tctx_current()->thread_count * sizeof(TaskWideSumInitData));
    array = malloc(ARRAY_SIZE * sizeof(i64));
  }
  lane_sync_u64(0, &init_data);
  lane_sync_u64(0, &array);

  {
    init_data[tctx_current()->thread_idx] = (TaskWideSumInitData){
        .range = lane_range(array_size),
        .array = array,
    };
    u8 next_task_id = atomic_inc_eval(&task_queue.tasks_count) - 1;
    task_queue.tasks_ptr[next_task_id] = (Task){
        .task_func = task_sum_init,
        .user_data = &init_data[tctx_current()->thread_idx],
    };
  }

  task_queue.task_counter = 0;
  lane_sync();
  for (;;) {
    u64 task_idx = atomic_inc_eval(&task_queue.task_counter) - 1;
    if (task_idx >= task_queue.tasks_count) {
      break;
    }
    printf("Init: Thread %d executing task %lld\n", tctx_current()->thread_idx,
           task_idx);

    Task task = task_queue.tasks_ptr[task_idx];
    task.task_func(task.user_data);
  }

  task_queue.task_counter = 0;
  task_queue.tasks_count = 0;
  lane_sync();

  static TaskWideSumData *sum_lane_data = NULL;
  if (is_main_thread()) {
    sum_lane_data =
        calloc(1, tctx_current()->thread_count * sizeof(TaskWideSumData));
  }
  lane_sync();

  sum_lane_data[tctx_current()->thread_idx] = (TaskWideSumData){
      .array = array,
      .range = lane_range(array_size),
      .lane_sum = 0,
  };

  {
    u8 next_task_id = atomic_inc_eval(&task_queue.tasks_count) - 1;
    task_queue.tasks_ptr[next_task_id] = (Task){
        .task_func = task_sum_exec,
        .user_data = &sum_lane_data[tctx_current()->thread_idx],
    };
  }

  task_queue.task_counter = 0;
  lane_sync();

  for (;;) {
    u64 task_idx = atomic_inc_eval(&task_queue.task_counter) - 1;
    if (task_idx >= task_queue.tasks_count) {
      break;
    }
    printf("exec: Thread %d executing task %lld\n", tctx_current()->thread_idx,
           task_idx);

    Task task = task_queue.tasks_ptr[task_idx];
    task.task_func(task.user_data);
  }
  task_queue.task_counter = 0;
  task_queue.tasks_count = 0;
  lane_sync();

  if (is_main_thread()) {
    u64 sum = 0;
    for (u8 i = 0; i < tctx_current()->thread_count; i++) {
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

  pthread_t *threads = malloc(thread_count * sizeof(pthread_t));
  ThreadContext *thread_ctx_arr = malloc(thread_count * sizeof(ThreadContext));
  pthread_barrier_t barrier;
  pthread_barrier_init(&barrier, NULL, thread_count);

  u64 broadcast_memory = 0;
  for (u8 i = 0; i < thread_count; i++) {
    thread_ctx_arr[i] = (ThreadContext){
        .thread_idx = i,
        .thread_count = thread_count,
        .barrier = &barrier,
        .broadcast_memory = &broadcast_memory,
    };
    pthread_create(&threads[i], NULL, entrypoint_internal, &thread_ctx_arr[i]);
  }

  for (u8 i = 0; i < thread_count; i++) {
    pthread_join(threads[i], NULL);
  }

  return 0;
}

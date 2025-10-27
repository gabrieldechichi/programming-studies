#include "thread_context.h"
#include "typedefs.h"
#include <stdio.h>
#include <stdlib.h>

#define ARRAY_SIZE 1000000

typedef void (*TaskFunc)(void *);

typedef struct {
  TaskFunc task_func;
  void *user_data;
} Task;

typedef struct {
  i64 *array;
  u64 array_size;
} TaskWideSumInitData;

typedef struct {
  i64 *array;
  Range_u64 range;
  i64 sum;
} TaskWideSumData;

void task_sum_init(void *_data) {
  TaskWideSumInitData *data = (TaskWideSumInitData *)_data;
  data->array_size = ARRAY_SIZE;
  data->array = malloc(data->array_size * sizeof(i64));
  for (u64 i = 0; i < data->array_size; i++) {
    data->array[i] = i + 1; // 1, 2, 3, ...
  }
}

void task_sum_exec(void *_data) {
  TaskWideSumData *data = (TaskWideSumData *)_data;
  i64 sum = 0;

  for (u64 i = data->range.min; i < data->range.max; i++) {
    sum += data->array[i];
  }
  data->sum = sum;
}

void entrypoint() {
  Task *tasks_ptr = NULL;
  u8 tasks_count = 0;
  u64 *task_counter_ptr = NULL;
  TaskWideSumInitData *init_data = NULL;

  if (is_main_thread()) {
    tasks_ptr = calloc(128, sizeof(Task));
    task_counter_ptr = calloc(1, sizeof(u64));
    *task_counter_ptr = 0;

    init_data = calloc(1, sizeof(TaskWideSumInitData));

    tasks_ptr[tasks_count++] = (Task){
        .task_func = task_sum_init,
        .user_data = init_data,
    };
  }

  // Broadcast pointers
  lane_sync_u64(0, &tasks_ptr);
  lane_sync_u64(0, &tasks_count);
  lane_sync_u64(0, &task_counter_ptr);
  lane_sync_u64(0, &init_data);

  // Now all threads can access the same tasks array
  for (;;) {
    u64 task_idx = atomic_add(task_counter_ptr, 1);
    if (task_idx >= tasks_count) {
      break;
    }
    printf("Thread %d executing task %lld\n", tctx_current()->thread_idx, task_idx);

    Task task = tasks_ptr[task_idx];
    task.task_func(task.user_data);
  }
  lane_sync();

  printf("Thread %d %lld %lld\n", tctx_current()->thread_idx,
         init_data->array_size, init_data->array[0]);
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

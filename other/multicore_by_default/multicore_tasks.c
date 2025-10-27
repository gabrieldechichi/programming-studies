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

static inline void cpu_pause() {
#if defined(__x86_64__) || defined(__i386__)
  __asm__ __volatile__("pause" ::: "memory");
#elif defined(__aarch64__) || defined(__arm64__)
  __asm__ __volatile__("yield" ::: "memory"); // ARM equivalent
#else
  __asm__ __volatile__("" ::: "memory"); // Fallback: just a barrier
#endif
}

void task_queue_append(TaskQueue *queue, Task task) {
  u8 next_task_id = atomic_inc_eval(&queue->tasks_count) - 1;
  queue->tasks_ptr[next_task_id] = task;
}

void task_queue_process(TaskQueue *queue) {
  queue->task_counter = 0;
  lane_sync();
  ThreadContext *tctx = tctx_current();

  for (;;) {
    u64 task_idx = atomic_inc_eval(&queue->task_counter) - 1;
    if (task_idx >= queue->tasks_count) {
      break;
    }
    printf("exec: Thread %d executing task %lld\n", tctx->thread_idx, task_idx);

    Task task = queue->tasks_ptr[task_idx];
    task.task_func(task.user_data);
  }
  queue->task_counter = 0;
  queue->tasks_count = 0;
  lane_sync();
}

void entrypoint() {
  local_shared TaskQueue task_queue = {0};
  local_shared TaskWideSumInitData *init_data = NULL;
  local_shared u64 array_size = ARRAY_SIZE;
  local_shared i64 *array = NULL;

  ThreadContext *tctx = tctx_current();

  if (is_main_thread()) {
    init_data = calloc(1, tctx->thread_count * sizeof(TaskWideSumInitData));
    array = malloc(ARRAY_SIZE * sizeof(i64));
  }
  lane_sync();

  // todo: use arena here for shared memory
  init_data[tctx->thread_idx] = (TaskWideSumInitData){
      .range = lane_range(array_size),
      .array = array,
  };
  task_queue_append(&task_queue, (Task){
                                     .task_func = task_sum_init,
                                     .user_data = &init_data[tctx->thread_idx],
                                 });

  task_queue_process(&task_queue);

  local_shared TaskWideSumData *sum_lane_data = NULL;
  if (is_main_thread()) {
    sum_lane_data = calloc(1, tctx->thread_count * sizeof(TaskWideSumData));
  }
  lane_sync();

  sum_lane_data[tctx->thread_idx] = (TaskWideSumData){
      .array = array,
      .range = lane_range(array_size),
      .lane_sum = 0,
  };

  task_queue_append(&task_queue,
                    (Task){
                        .task_func = task_sum_exec,
                        .user_data = &sum_lane_data[tctx->thread_idx],
                    });

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

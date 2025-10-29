#include "lib/array.h"
#include "lib/thread_context.h"
#include "lib/typedefs.h"
#include "lib/assert.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define NUMBERS_COUNT 1000000000
// #define NUMBERS_COUNT 1000000

static inline void cpu_pause() {
#if defined(__x86_64__) || defined(__i386__)
  __asm__ __volatile__("pause" ::: "memory");
#elif defined(__aarch64__) || defined(__arm64__)
  __asm__ __volatile__("yield" ::: "memory"); // ARM equivalent
#else
  __asm__ __volatile__("" ::: "memory"); // Fallback: just a barrier
#endif
}

arr_define(i64);

#define arr_view_from_min_max(type, _items, min, max)                          \
  ((type##_Array){                                                             \
      .items = (_items) + (min),                                               \
      .len = (max) - (min),                                                    \
  })

#define arr_view_from_range(type, _items, range)                               \
  arr_view_from_min_max(type, _items, (range).min, (range).max)

typedef enum {
  TASK_RESOURCE_TYPE_READ,
  TASK_RESOURCE_TYPE_WRITE,
} TaskResourceAccessType;

typedef struct {
  TaskResourceAccessType access_mode;
  void *ptr;
  u64 size;
} TaskResourceAccess;

TaskResourceAccess task_resource_access_create(TaskResourceAccessType type,
                                               void *ptr, u64 size) {
  TaskResourceAccess resource_access = {
      .access_mode = type, .ptr = ptr, .size = size};
  return resource_access;
}

#define TASK_ACCESS_READ(ptr, size)                                            \
  task_resource_access_create(TASK_RESOURCE_TYPE_READ, (void *)(ptr), (size))

#define TASK_ACCESS_WRITE(ptr, size)                                           \
  task_resource_access_create(TASK_RESOURCE_TYPE_WRITE, (void *)(ptr), (size))

typedef void (*TaskFunc)(void *);

typedef struct {
  u8 h[1];
} TaskHandle;

typedef struct {
  TaskFunc task_func;
  void *user_data;

  // Dependencies: how many dependencies I'm waiting on
  i8 dependency_count_remaining;

  // Dependents: who's waiting for me
  TaskHandle dependent_task_ids[32];
  u8 dependents_count;

#if DEBUG
  TaskResourceAccess resources[16];
  u8 resources_count;
#endif
} Task;

typedef struct {
  Task tasks_ptr[128];
  u8 tasks_count;

  TaskHandle ready_queue[128];
  u8 ready_count;
  u8 ready_counter;
} TaskQueue;

TaskHandle _task_queue_append(TaskQueue *queue, TaskFunc fn, void *data,
                              TaskResourceAccess *resources, u8 resources_count,
                              TaskHandle *deps, u8 dep_count) {
  u8 next_task_id = atomic_inc_eval(&queue->tasks_count) - 1;

  Task task = (Task){.task_func = (TaskFunc)(fn), .user_data = data};
  task.dependency_count_remaining = dep_count;

  queue->tasks_ptr[next_task_id] = task;

  TaskHandle this_task_handle = {
      next_task_id,
  };
  // if we have dependencies, find dependent tasks and add this task to it's
  // dependencies
  if (dep_count > 0) {
    for (u8 i = 0; i < dep_count; i++) {
      TaskHandle dependency_task_handle = deps[i];
      Task *dependency_task = &queue->tasks_ptr[dependency_task_handle.h[0]];
      u8 next_dependent_id =
          atomic_inc_eval(&dependency_task->dependents_count) - 1;
      dependency_task->dependent_task_ids[next_dependent_id] = this_task_handle;
    }
  } else {
    // if we don't have dependencies add ourselves to the ready queue directly
    u8 next_ready_id = atomic_inc_eval(&queue->ready_count) - 1;
    queue->ready_queue[next_ready_id] = this_task_handle;
  }

#if DEBUG
  if (resources == NULL) {
    resources_count = 0;
  }

  // store resources for race detection
  task.resources_count = resources_count;
  memcpy(task.resources, resources,
         sizeof(TaskResourceAccess) * resources_count);

  // re-assign task
  queue->tasks_ptr[next_task_id] = task;

  // Check for data race conditions against all existing tasks
  for (u8 other_task_idx = 0; other_task_idx < next_task_id; other_task_idx++) {
    Task *other_task = &queue->tasks_ptr[other_task_idx];

    // Check each resource in this task against each resource in other task
    for (u8 i = 0; i < resources_count; i++) {
      TaskResourceAccess *my_resource = &task.resources[i];

      for (u8 j = 0; j < other_task->resources_count; j++) {
        TaskResourceAccess *other_resource = &other_task->resources[j];

        // Check if memory regions overlap
        u64 my_start = (u64)my_resource->ptr;
        u64 my_end = my_start + my_resource->size;
        u64 other_start = (u64)other_resource->ptr;
        u64 other_end = other_start + other_resource->size;

        b32 overlaps = (my_start < other_end) && (other_start < my_end);

        if (overlaps) {
          // Check if there's a conflict (at least one WRITE)
          b32 has_conflict =
              (my_resource->access_mode == TASK_RESOURCE_TYPE_WRITE) ||
              (other_resource->access_mode == TASK_RESOURCE_TYPE_WRITE);

          if (has_conflict) {
            // Verify the other task is in our dependency chain
            b32 is_dependency = 0;
            for (u8 d = 0; d < dep_count; d++) {
              if (deps[d].h[0] == other_task_idx) {
                is_dependency = 1;
                break;
              }
            }

            if (!is_dependency) {
              printf("RACE CONDITION DETECTED:\n"
                     "  Task %d conflicts with Task %d\n"
                     "  Memory region: [%p - %p] overlaps [%p - %p]\n"
                     "  Access modes: Task %d = %s, Task %d = %s\n"
                     "  Task %d should depend on Task %d\n",
                     next_task_id, other_task_idx, (void *)my_start,
                     (void *)my_end, (void *)other_start, (void *)other_end,
                     next_task_id,
                     my_resource->access_mode == TASK_RESOURCE_TYPE_WRITE
                         ? "WRITE"
                         : "READ",
                     other_task_idx,
                     other_resource->access_mode == TASK_RESOURCE_TYPE_WRITE
                         ? "WRITE"
                         : "READ",
                     next_task_id, other_task_idx);
              exit(1);
              // Note: not aborting here, just warning
            }
          }
        }
      }
    }
  }
#endif

  return this_task_handle;
}

#define task_queue_append(queue, fn, data, resources, resources_count, deps,   \
                          deps_count)                                          \
  _task_queue_append((queue), (TaskFunc)(fn), (void *)(data), resources,       \
                     resources_count, deps, deps_count)

void task_queue_process(TaskQueue *queue) {
  ThreadContext *tctx = tctx_current();
  queue->ready_counter = 0;
  lane_sync();
  printf("thread %d: start processing queue\n", tctx->thread_idx);

  for (;;) {
    u8 ready_idx = atomic_inc_eval(&queue->ready_counter) - 1;
    // we have ready tasks
    if (ready_idx < queue->ready_count) {
      TaskHandle ready_task_handle = queue->ready_queue[ready_idx];
      printf("thread %d: executing task handle %d (%d)\n", tctx->thread_idx,
             ready_idx, ready_task_handle.h[0]);
      // note: should we assume valid task here?
      Task task = queue->tasks_ptr[ready_task_handle.h[0]];
      task.task_func(task.user_data);

      for (u8 i = 0; i < task.dependents_count; i++) {
        TaskHandle dependent_handle = task.dependent_task_ids[i];
        Task *dependent = &queue->tasks_ptr[dependent_handle.h[0]];
        // todo: what if this wraps to max u8?
        i8 dependency_count_remaining =
            atomic_dec_eval(&dependent->dependency_count_remaining);
        // add to the ready queue
        if (dependency_count_remaining <= 0) {
          printf("thread %d: adding task %d to ready queue\n", tctx->thread_idx,
                 dependent_handle.h[0]);
          // todo: fix repeated code (thread safe array?)
          u8 next_ready_id = atomic_inc_eval(&queue->ready_count) - 1;
          queue->ready_queue[next_ready_id] = dependent_handle;
        }
      }
      printf("thread %d: done executing task %d\n", tctx->thread_idx,
             ready_task_handle.h[0]);
    } else if (queue->ready_counter >= queue->tasks_count) {
      // done
      break;
    } else {
      // no ready tasks, put the id back and try again
      atomic_dec_eval(&queue->ready_counter);
      cpu_pause();
    }
  }

  printf("thread %d: done processing queue\n", tctx->thread_idx);

  queue->ready_counter = 0;
  queue->ready_count = 0;
  queue->tasks_count = 0;
  lane_sync();
}

#define HZ_TASK()
// BEGIN USER CODE

HZ_TASK()
typedef struct {
  u64 values_start;
  i64_Array numbers;
} TaskWideSumInit;

void task_sum_init(TaskWideSumInit *data) {
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

static void _task_sum_init(void *_data) {
  TaskWideSumInit *data = (TaskWideSumInit *)_data;
  task_sum_init(data);
}

TaskHandle _TaskWideSumInit_schedule(TaskQueue *queue,
                                         TaskWideSumInit *data,
                                         TaskHandle *deps, u8 deps_count) {
  assert(data);
  // build resource access
  // todo: generate
  TaskResourceAccess resource_access[32];
  u8 resource_count = 0;
  resource_access[resource_count++] =
      TASK_ACCESS_WRITE(data->numbers.items, data->numbers.len);

  return _task_queue_append(queue, _task_sum_init, data, resource_access,
                            resource_count, deps, deps_count);
}

#define TaskWideSumInit_schedule(queue, data, ...)                         \
  _TaskWideSumInit_schedule(queue, data,                                   \
                                ARGS_ARRAY(TaskHandle, __VA_ARGS__),           \
                                ARGS_COUNT(TaskHandle, __VA_ARGS__))

typedef struct {
  i64_Array numbers;
  i64 lane_sum;
} TaskWideSumExec;

void task_sum_exec(TaskWideSumExec *data) {
  i64 sum = 0;

  arr_foreach(data->numbers, i64, value) { sum += value; }
  data->lane_sum = sum;
}

static void _task_sum_exec(void *_data) {
  TaskWideSumExec *data = (TaskWideSumExec *)_data;
  task_sum_exec(data);
}

TaskHandle _TaskWideSumExec_schedule(TaskQueue *queue, TaskWideSumExec *data,
                                     TaskHandle *deps, u8 deps_count) {
  assert(data);
  // build resource access
  // todo: generate
  TaskResourceAccess resource_access[32];
  u8 resource_count = 0;
  resource_access[resource_count++] =
      TASK_ACCESS_READ(data->numbers.items, data->numbers.len);
  // todo: add write to thing

  return _task_queue_append(queue, _task_sum_exec, data, resource_access,
                            resource_count, deps, deps_count);
}

#define TaskWideSumExec_schedule(queue, data, ...)                             \
  _TaskWideSumExec_schedule(queue, data, ARGS_ARRAY(TaskHandle, __VA_ARGS__),  \
                            ARGS_COUNT(TaskHandle, __VA_ARGS__))

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

  TaskWideSumInit *init_data =
      arena_alloc(&tctx->temp_arena, sizeof(TaskWideSumInit));
  *init_data = (TaskWideSumInit){
      .numbers = numbers,
      .values_start = range.min + 1,
  };

  TaskHandle init_task_handle =
      TaskWideSumInit_schedule(&task_queue, init_data);

  sum_lane_data[tctx->thread_idx] = (TaskWideSumExec){
      .numbers = numbers,
      .lane_sum = 0,
  };

  TaskWideSumExec_schedule(&task_queue, &sum_lane_data[tctx->thread_idx],
                           init_task_handle);

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
        .temp_arena = arena_from_buffer(calloc(1, MB(8)), MB(8)),
    };
    pthread_create(&threads[i], NULL, entrypoint_internal, &thread_ctx_arr[i]);
  }

  for (u8 i = 0; i < thread_count; i++) {
    pthread_join(threads[i], NULL);
  }

  return 0;
}

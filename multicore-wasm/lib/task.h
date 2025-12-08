#ifndef H_TASK
#define H_TASK

#include "lib/typedefs.h"
#include "lib/thread.h"
#include "lib/memory.h"
#include "lib/thread_context.h"
#include "os/os.h"
typedef enum {
  TASK_RESOURCE_TYPE_READ,
  TASK_RESOURCE_TYPE_WRITE,
} TaskResourceAccessType;

typedef struct {
  TaskResourceAccessType access_mode;
  void *ptr;
  u64 size;
} TaskResourceAccess;

typedef void (*TaskFunc)(void *);

typedef struct {
  u8 h[1];
} TaskHandle;

typedef struct {
  TaskFunc task_func;
  void *user_data;

  // Dependencies: how many dependencies I'm waiting on
  i32 dependency_count_remaining;

  // Dependents: who's waiting for me
  TaskHandle dependent_task_ids[32];
  u32 dependents_count;

#if DEBUG
  TaskResourceAccess resources[16];
  u32 resources_count;
#endif
} Task;

force_inline TaskResourceAccess
task_resource_access_create(TaskResourceAccessType type, void *ptr, u64 size) {
  TaskResourceAccess resource_access = {
      .access_mode = type, .ptr = ptr, .size = size};
  return resource_access;
}

#define MAX_TASKS 256

typedef struct {
  Task* tasks_ptr;
  u64 tasks_count;

  TaskHandle* ready_queue;
  u64 ready_write_idx;
  u64 ready_count;
  u64 ready_counter;

  TaskHandle* next_ready_queue;
  u64 next_ready_write_idx;
  u64 next_ready_count;
} TaskQueue;

typedef struct TaskSystem {
  Thread *workers;
  ThreadContext *worker_contexts;
  ThreadContext main_thread_context;
  u32 worker_count;

  Barrier barrier;
  u64 broadcast_memory;

  TaskQueue queue;

  Semaphore work_semaphore;
  volatile u32 tasks_in_flight;
  volatile b32 should_quit;
  volatile b32 processing;
} TaskSystem;

#define TASK_ACCESS_READ(ptr, size)                                            \
  task_resource_access_create(TASK_RESOURCE_TYPE_READ, (void *)(ptr), (size))

#define TASK_ACCESS_WRITE(ptr, size)                                           \
  task_resource_access_create(TASK_RESOURCE_TYPE_WRITE, (void *)(ptr), (size))

TaskHandle _task_queue_append(TaskQueue *queue, TaskFunc fn, void *data,
                              TaskResourceAccess *resources, u8 resources_count,
                              TaskHandle *deps, u8 dep_count);


#define task_queue_append(queue, fn, data, resources, resources_count, deps,   \
                          deps_count)                                          \
  _task_queue_append((queue), (TaskFunc)(fn), (void *)(data), resources,       \
                     resources_count, deps, deps_count)

void task_queue_process(TaskQueue *queue);

TaskSystem task_system_init(u32 worker_count, Allocator *allocator);
void task_system_start_workers(TaskSystem *sys);
void task_system_shutdown(TaskSystem *sys);

TaskHandle task_schedule(TaskSystem *sys, TaskFunc fn, void *data);
TaskHandle task_schedule_after(TaskSystem *sys, TaskFunc fn, void *data,
                               TaskHandle *deps, u8 dep_count);

void task_queue_wait(TaskSystem *sys);
void task_queue_flush(TaskSystem *sys);
void task_queue_reset(TaskSystem *sys);

#endif

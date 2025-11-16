#ifndef H_TASK
#define H_TASK

#include "lib/typedefs.h"
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
  i8 dependency_count_remaining;

  // Dependents: who's waiting for me
  TaskHandle dependent_task_ids[32];
  u8 dependents_count;

#if DEBUG
  TaskResourceAccess resources[16];
  u8 resources_count;
#endif
} Task;

force_inline TaskResourceAccess
task_resource_access_create(TaskResourceAccessType type, void *ptr, u64 size) {
  TaskResourceAccess resource_access = {
      .access_mode = type, .ptr = ptr, .size = size};
  return resource_access;
}

typedef struct {
  Task tasks_ptr[128];
  u8 tasks_count;

  TaskHandle ready_queue[128];
  u8 ready_count;
  u8 ready_counter;
} TaskQueue;

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

#endif

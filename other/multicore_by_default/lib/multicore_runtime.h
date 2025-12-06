#ifndef H_TASK
#define H_TASK

#include "lib/typedefs.h"
typedef enum
{
  MCR_RESOURCE_TYPE_READ,
  MCR_RESOURCE_TYPE_WRITE,
} MCRResourceAccessType;

typedef struct
{
  MCRResourceAccessType access_mode;
  void *ptr;
  u64 size;
} MCRResourceAccess;

typedef void (*MCRFunc)(void *);

typedef struct
{
  u8 h[1];
} MCRHandle;

typedef struct
{
  MCRFunc mcr_func;
  void *user_data;

  // Dependencies: how many dependencies I'm waiting on
  i32 dependency_count_remaining;

  // Dependents: who's waiting for me
  MCRHandle dependent_mcr_ids[32];
  u32 dependents_count;

#if DEBUG
  MCRResourceAccess resources[16];
  u32 resources_count;
#endif
} Task;

force_inline MCRResourceAccess
mcr_resource_access_create(MCRResourceAccessType type, void *ptr, u64 size)
{
  MCRResourceAccess resource_access = {
      .access_mode = type, .ptr = ptr, .size = size};
  return resource_access;
}

typedef struct
{
  Task tasks_ptr[128];
  u64 tasks_count;

  MCRHandle ready_queue[128];
  u64 ready_count;
  u64 ready_counter;

  MCRHandle next_ready_queue[128];
  u64 next_ready_count;
} MCRQueue;

#define MCR_ACCESS_READ(ptr, size) \
  mcr_resource_access_create(MCR_RESOURCE_TYPE_READ, (void *)(ptr), (size))

#define MCR_ACCESS_WRITE(ptr, size) \
  mcr_resource_access_create(MCR_RESOURCE_TYPE_WRITE, (void *)(ptr), (size))

MCRHandle _mcr_queue_append(MCRQueue *queue, MCRFunc fn, void *data,
                            MCRResourceAccess *resources, u8 resources_count,
                            MCRHandle *deps, u8 dep_count);

#define mcr_queue_append(queue, fn, data, resources, resources_count, deps, \
                         deps_count)                                        \
  _mcr_queue_append((queue), (MCRFunc)(fn), (void *)(data), resources,      \
                    resources_count, deps, deps_count)

void mcr_queue_process(MCRQueue *queue);

#endif

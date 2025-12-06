#ifndef H_MULTICORE_RUNTIME
#define H_MULTICORE_RUNTIME

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

typedef void (*MCRTaskFunc)(void *);

typedef struct
{
  u8 h[1];
} MCRTaskHandle;

typedef struct
{
  MCRTaskFunc mcr_func;
  void *user_data;

  // Dependencies: how many dependencies I'm waiting on
  i32 dependency_count_remaining;

  // Dependents: who's waiting for me
  MCRTaskHandle dependent_mcr_ids[32];
  u32 dependents_count;

#if DEBUG
  MCRResourceAccess resources[16];
  u32 resources_count;
#endif
} MCRTask;

force_inline MCRResourceAccess
mcr_resource_access_create(MCRResourceAccessType type, void *ptr, u64 size)
{
  MCRResourceAccess resource_access = {
      .access_mode = type, .ptr = ptr, .size = size};
  return resource_access;
}

typedef struct
{
  MCRTask tasks_ptr[128];
  u64 tasks_count;

  MCRTaskHandle ready_queue[128];
  u64 ready_count;
  u64 ready_counter;

  MCRTaskHandle next_ready_queue[128];
  u64 next_ready_count;
} MCRTaskQueue;

#define MCR_ACCESS_READ(ptr, size) \
  mcr_resource_access_create(MCR_RESOURCE_TYPE_READ, (void *)(ptr), (size))

#define MCR_ACCESS_WRITE(ptr, size) \
  mcr_resource_access_create(MCR_RESOURCE_TYPE_WRITE, (void *)(ptr), (size))

MCRTaskHandle _mcr_queue_append(MCRTaskQueue *queue, MCRTaskFunc fn, void *data,
                                MCRResourceAccess *resources, u8 resources_count,
                                MCRTaskHandle *deps, u8 dep_count);

#define mcr_queue_append(queue, fn, data, resources, resources_count, deps, \
                         deps_count)                                        \
  _mcr_queue_append((queue), (MCRTaskFunc)(fn), (void *)(data), resources,  \
                    resources_count, deps, deps_count)

void mcr_queue_process(MCRTaskQueue *queue);

#endif

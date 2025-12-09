#include "multicore_runtime.h"
#include "lib/string_builder.h"
#include "lib/thread_context.h"
#include "os/os.h"
#include "thread.h"

typedef struct {
  ThreadContext *ctx;
  MCREntrypointFunc func;
} MCREntrypointFnData;

internal void _mcr_entrypoint_internal(void *arg) {
  MCREntrypointFnData *ctx = (MCREntrypointFnData *)arg;
  tctx_set_current(ctx->ctx);

  ctx->func();
}

void mcr_run(u8 thread_count, size_t temp_arena_size, MCREntrypointFunc func,
             ArenaAllocator *arena) {
  Thread *threads = ARENA_ALLOC_ARRAY(arena, Thread, thread_count);
  ThreadContext *thread_ctx_arr =
      ARENA_ALLOC_ARRAY(arena, ThreadContext, thread_count);
  MCREntrypointFnData *entrypoints =
      ARENA_ALLOC_ARRAY(arena, MCREntrypointFnData, thread_count);
  Barrier barrier = barrier_alloc(thread_count);

  u64 broadcast_memory = 0;
  for (u8 i = 0; i < thread_count; i++) {
    thread_ctx_arr[i] = (ThreadContext){
        .thread_idx = i,
        .thread_count = thread_count,
        .barrier = &barrier,
        .broadcast_memory = &broadcast_memory,
        .temp_arena = arena_from_buffer(
            ARENA_ALLOC_ARRAY(arena, u8, temp_arena_size), temp_arena_size),
    };
    entrypoints[i] =
        (MCREntrypointFnData){.ctx = &thread_ctx_arr[i], .func = func};
    threads[i] = thread_launch(_mcr_entrypoint_internal, &entrypoints[i]);

    if (i == 0) {
      thread_set_name(threads[i], "MCR Main");
    } else {
      char thread_name[256];
      StringBuilder sb;
      sb_init(&sb, thread_name, 256);
      sb_append_format(&sb, "MCR Thread %", FMT_UINT(i));
      thread_set_name(threads[i], sb_get(&sb));
    }
  }

  for (u8 i = 0; i < thread_count; i++) {
    thread_join(threads[i], 0);
  }
}

MCRTaskHandle _mcr_queue_append(MCRTaskQueue *queue, MCRTaskFunc fn, void *data,
                                MCRResourceAccess *resources,
                                u8 resources_count, MCRTaskHandle *deps,
                                u8 dep_count) {
  u64 next_mcr_id = ins_atomic_u64_inc_eval(&queue->tasks_count) - 1;

  MCRTask task = (MCRTask){.mcr_func = (MCRTaskFunc)(fn), .user_data = data};
  task.dependency_count_remaining = dep_count;

  queue->tasks_ptr[next_mcr_id] = task;

  MCRTaskHandle this_mcr_handle = {
      next_mcr_id,
  };
  // if we have dependencies, find dependent tasks and add this task to it's
  // dependencies
  if (dep_count > 0) {
    for (u8 i = 0; i < dep_count; i++) {
      MCRTaskHandle dependency_mcr_handle = deps[i];
      MCRTask *dependency_task = &queue->tasks_ptr[dependency_mcr_handle.h[0]];
      u32 next_dependent_id =
          ins_atomic_u32_inc_eval(&dependency_task->dependents_count) - 1;
      dependency_task->dependent_mcr_ids[next_dependent_id] = this_mcr_handle;
    }
  } else {
    // if we don't have dependencies add ourselves to the ready queue directly
    u64 next_ready_id = ins_atomic_u64_inc_eval(&queue->ready_count) - 1;
    queue->ready_queue[next_ready_id] = this_mcr_handle;
  }

#if DEBUG
  if (resources == NULL) {
    resources_count = 0;
  }

  // store resources for race detection
  task.resources_count = resources_count;
  memcpy(task.resources, resources,
         sizeof(MCRResourceAccess) * resources_count);

  // re-assign task
  queue->tasks_ptr[next_mcr_id] = task;

  // Check for data race conditions against all existing tasks
  for (u8 other_mcr_idx = 0; other_mcr_idx < next_mcr_id; other_mcr_idx++) {
    MCRTask *other_task = &queue->tasks_ptr[other_mcr_idx];

    // Check each resource in this task against each resource in other task
    for (u8 i = 0; i < resources_count; i++) {
      MCRResourceAccess *my_resource = &task.resources[i];

      for (u8 j = 0; j < other_task->resources_count; j++) {
        MCRResourceAccess *other_resource = &other_task->resources[j];

        // Check if memory regions overlap
        u64 my_start = (u64)my_resource->ptr;
        u64 my_end = my_start + my_resource->size;
        u64 other_start = (u64)other_resource->ptr;
        u64 other_end = other_start + other_resource->size;

        b32 overlaps = (my_start < other_end) && (other_start < my_end);

        if (overlaps) {
          // Check if there's a conflict (at least one WRITE)
          b32 has_conflict =
              (my_resource->access_mode == MCR_RESOURCE_TYPE_WRITE) ||
              (other_resource->access_mode == MCR_RESOURCE_TYPE_WRITE);

          if (has_conflict) {
            // Verify the other task is in our dependency chain
            b32 is_dependency = 0;
            for (u8 d = 0; d < dep_count; d++) {
              if (deps[d].h[0] == other_mcr_idx) {
                is_dependency = 1;
                break;
              }
            }

            if (!is_dependency) {
              LOG_INFO(
                  "RACE CONDITION DETECTED:\n"
                  "  Task %d conflicts with Task %d\n"
                  "  Memory region: [%p - %p] overlaps [%p - %p]\n"
                  "  Access modes: Task %d = %s, Task %d = %s\n"
                  "  Task %d should depend on Task %d\n",
                  FMT_UINT(next_mcr_id), FMT_UINT(other_mcr_idx),
                  FMT_UINT(my_start), FMT_UINT(my_end), FMT_UINT(other_start),
                  FMT_UINT(other_end), FMT_UINT(next_mcr_id),
                  FMT_STR(my_resource->access_mode == MCR_RESOURCE_TYPE_WRITE
                              ? "WRITE"
                              : "READ"),
                  FMT_UINT(other_mcr_idx),
                  FMT_STR(other_resource->access_mode == MCR_RESOURCE_TYPE_WRITE
                              ? "WRITE"
                              : "READ"),
                  FMT_UINT(next_mcr_id), FMT_UINT(other_mcr_idx));
#ifndef WASM
              exit(1);
#endif
            }
          }
        }
      }
    }
  }
#endif

  return this_mcr_handle;
}

void mcr_queue_process(MCRTaskQueue *queue) {
  ThreadContext *tctx = tctx_current();
  queue->ready_counter = 0;
  queue->next_ready_count = 0;
  lane_sync();
  // todo: change to verbose
  //  printf("thread %d: start processing queue\n", tctx->thread_idx);

process_queue_loop:
  for (;;) {
    u64 ready_idx = ins_atomic_u64_inc_eval(&queue->ready_counter) - 1;
    // we have ready tasks
    if (ready_idx < queue->ready_count) {
      MCRTaskHandle ready_mcr_handle = queue->ready_queue[ready_idx];
      // todo: change to verbose
      //  printf("thread %d: executing task handle %d (%d)\n", tctx->thread_idx,
      //         ready_idx, ready_mcr_handle.h[0]);

      // note: should we assume valid task here?
      MCRTask *task = &queue->tasks_ptr[ready_mcr_handle.h[0]];
      task->mcr_func(task->user_data);

      for (u32 i = 0; i < task->dependents_count; i++) {
        MCRTaskHandle dependent_handle = task->dependent_mcr_ids[i];
        MCRTask *dependent = &queue->tasks_ptr[dependent_handle.h[0]];
        // todo: what if this wraps to max u32?
        i32 dependency_count_remaining =
            ins_atomic_u32_dec_eval(&dependent->dependency_count_remaining);
        // add to the ready queue
        if (dependency_count_remaining == 0) {
          // todo: change to verbose
          // printf("thread %d: adding task %d to ready queue\n",
          // tctx->thread_idx,
          //        dependent_handle.h[0]);

          // todo: fix repeated code (thread safe array?)
          u64 next_ready_id =
              ins_atomic_u64_inc_eval(&queue->next_ready_count) - 1;
          queue->next_ready_queue[next_ready_id] = dependent_handle;
        }
      }
      // todo: change to verbose
      //  printf("thread %d: done executing task %d\n", tctx->thread_idx,
      //         ready_mcr_handle.h[0]);
    } else {
      // done
      break;
    }
  }

  // we need sync here to make a sure a thread doesn't early exit before next
  // ready queue has been appended
  lane_sync();
  // printf("thread %d: finished processing ready queue, checking for next ready
  // "
  //        "queue. count %d\n",
  //        tctx->thread_idx, queue->next_ready_count);

  if (queue->next_ready_count > 0) {
    // sync here to prevent main thread from gettin ghere to fast and seting
    // next_ready_count = 0
    lane_sync();
    if (is_main_thread()) {
      // todo: too big
      memcpy(queue->ready_queue, queue->next_ready_queue,
             sizeof(MCRTaskHandle) * queue->next_ready_count);
      queue->ready_count = queue->next_ready_count;
      queue->ready_counter = 0;
      queue->next_ready_count = 0;
    }
    // sync so every thread has shared memory
    lane_sync();
    goto process_queue_loop;
  }

  // todo: change to verbose
  //  printf("thread %d: done processing queue\n", tctx->thread_idx);

  queue->ready_counter = 0;
  queue->ready_count = 0;
  queue->tasks_count = 0;
  queue->next_ready_count = 0;
  lane_sync();
}

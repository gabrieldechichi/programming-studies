#include "task.h"
#include "lib/memory.h"
#include "lib/thread_context.h"
#include "os/os.h"
#include <stdlib.h>
#include <string.h>

TaskHandle _task_queue_append(TaskQueue *queue, TaskFunc fn, void *data,
                              TaskResourceAccess *resources, u8 resources_count,
                              TaskHandle *deps, u8 dep_count) {
#if !DEBUG
  (void)resources;
  (void)resources_count;
#endif
  u64 next_task_id = ins_atomic_u64_inc_eval(&queue->tasks_count) - 1;

  Task task = (Task){.task_func = (TaskFunc)(fn), .user_data = data};
  task.dependency_count_remaining = dep_count;

  queue->tasks_ptr[next_task_id] = task;

  TaskHandle this_task_handle = {
      (u8)next_task_id,
  };
  // if we have dependencies, find dependent tasks and add this task to it's
  // dependencies
  if (dep_count > 0) {
    for (u8 i = 0; i < dep_count; i++) {
      TaskHandle dependency_task_handle = deps[i];
      Task *dependency_task = &queue->tasks_ptr[dependency_task_handle.h[0]];
      u32 next_dependent_id =
          ins_atomic_u32_inc_eval(&dependency_task->dependents_count) - 1;
      dependency_task->dependent_task_ids[next_dependent_id] = this_task_handle;
    }
  } else {
    u64 slot = ins_atomic_u64_inc_eval(&queue->ready_write_idx) - 1;
    queue->ready_queue[slot] = this_task_handle;
    while ((u64)ins_atomic_load_acquire64(&queue->ready_count) < slot) {
      cpu_pause();
    }
    ins_atomic_store_release64(&queue->ready_count, slot + 1);
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
              LOG_ERROR("RACE CONDITION DETECTED: Task % conflicts with Task %",
                        FMT_UINT(next_task_id), FMT_UINT(other_task_idx));
              exit(1);
            }
          }
        }
      }
    }
  }
#endif

  return this_task_handle;
}

void task_queue_process(TaskQueue *queue) {
  ThreadContext *tctx = tctx_current();
  queue->ready_counter = 0;
  queue->next_ready_count = 0;
  lane_sync();
  LOG_INFO("thread %: start processing queue", FMT_UINT(tctx->thread_idx));

process_queue_loop:
  for (;;) {
    u64 ready_idx = ins_atomic_u64_inc_eval(&queue->ready_counter) - 1;
    // we have ready tasks
    if (ready_idx < queue->ready_count) {
      TaskHandle ready_task_handle = queue->ready_queue[ready_idx];
      LOG_INFO("thread %: executing task handle % (%)",
               FMT_UINT(tctx->thread_idx), FMT_UINT(ready_idx),
               FMT_UINT(ready_task_handle.h[0]));
      // note: should we assume valid task here?
      Task *task = &queue->tasks_ptr[ready_task_handle.h[0]];
      task->task_func(task->user_data);

      for (u32 i = 0; i < task->dependents_count; i++) {
        TaskHandle dependent_handle = task->dependent_task_ids[i];
        Task *dependent = &queue->tasks_ptr[dependent_handle.h[0]];
        // todo: what if this wraps to max u32?
        i32 dependency_count_remaining =
            ins_atomic_u32_dec_eval(&dependent->dependency_count_remaining);
        // add to the ready queue
        if (dependency_count_remaining == 0) {
          LOG_INFO("thread %: adding task % to ready queue",
                   FMT_UINT(tctx->thread_idx), FMT_UINT(dependent_handle.h[0]));
          // todo: fix repeated code (thread safe array?)
          u64 next_ready_id =
              ins_atomic_u64_inc_eval(&queue->next_ready_count) - 1;
          queue->next_ready_queue[next_ready_id] = dependent_handle;
        }
      }
      LOG_INFO("thread %: done executing task %", FMT_UINT(tctx->thread_idx),
               FMT_UINT(ready_task_handle.h[0]));
    } else {
      // done
      break;
    }
  }

  // we need sync here to make a sure a thread doesn't early exit before next
  // ready queue has been appended
  lane_sync();
  LOG_INFO("thread %: finished processing ready queue, checking for next ready "
           "queue. count %",
           FMT_UINT(tctx->thread_idx), FMT_UINT(queue->next_ready_count));

  if (queue->next_ready_count > 0) {
    // sync here to prevent main thread from gettin ghere to fast and seting
    // next_ready_count = 0
    lane_sync();
    if (is_main_thread()) {
      // todo: too big
      memcpy(queue->ready_queue, queue->next_ready_queue,
             sizeof(TaskHandle) * queue->next_ready_count);
      queue->ready_count = queue->next_ready_count;
      queue->ready_counter = 0;
      queue->next_ready_count = 0;
    }
    // sync so every thread has shared memory
    lane_sync();
    goto process_queue_loop;
  }

  LOG_INFO("thread %: done processing queue", FMT_UINT(tctx->thread_idx));

  queue->ready_counter = 0;
  queue->ready_count = 0;
  queue->tasks_count = 0;
  queue->next_ready_count = 0;
  lane_sync();
}

internal void task_execute_one(TaskSystem *sys, TaskHandle handle) {
  TaskQueue *q = &sys->queue;
  Task *task = &q->tasks_ptr[handle.h[0]];
  task->task_func(task->user_data);

  for (u32 i = 0; i < task->dependents_count; i++) {
    TaskHandle dependent_handle = task->dependent_task_ids[i];
    Task *dependent = &q->tasks_ptr[dependent_handle.h[0]];
    i32 remaining =
        ins_atomic_u32_dec_eval(&dependent->dependency_count_remaining);
    if (remaining == 0) {
      u64 slot = ins_atomic_u64_inc_eval(&q->next_ready_write_idx) - 1;
      q->next_ready_queue[slot] = dependent_handle;
      while ((u64)ins_atomic_load_acquire64(&q->next_ready_count) < slot) {
        cpu_pause();
      }
      ins_atomic_store_release64(&q->next_ready_count, slot + 1);
      ins_atomic_u32_inc_eval(&sys->tasks_in_flight);
      semaphore_drop(sys->work_semaphore);
    }
  }
}

internal void task_worker_proc(void *arg) {
  ThreadContext *ctx = (ThreadContext *)arg;
  tctx_set_current(ctx);
  TaskSystem *sys = ctx->task_system;

  while (!sys->should_quit) {
    semaphore_take(sys->work_semaphore);

    if (sys->should_quit)
      break;

    TaskQueue *q = &sys->queue;

    u64 idx = ins_atomic_u64_inc_eval(&q->ready_counter) - 1;
    u64 count = ins_atomic_load_acquire64(&q->ready_count);
    if (idx < count) {
      TaskHandle handle = q->ready_queue[idx];
      task_execute_one(sys, handle);
      ins_atomic_u32_dec_eval(&sys->tasks_in_flight);
    }
  }
}

#define WORKER_ARENA_SIZE MB(8)

TaskSystem task_system_init(u32 worker_count, Allocator *allocator) {
  TaskSystem sys = {0};
  sys.worker_count = worker_count;
  sys.should_quit = false;
  sys.tasks_in_flight = 0;
  sys.processing = false;

  sys.queue.tasks_ptr = ALLOC_ARRAY(allocator, Task, MAX_TASKS);
  sys.queue.ready_queue = ALLOC_ARRAY(allocator, TaskHandle, MAX_TASKS);
  sys.queue.next_ready_queue = ALLOC_ARRAY(allocator, TaskHandle, MAX_TASKS);

  sys.work_semaphore = semaphore_alloc(0);
  sys.barrier = barrier_alloc(worker_count + 1);

  sys.workers = ALLOC_ARRAY(allocator, Thread, worker_count);
  sys.worker_contexts = ALLOC_ARRAY(allocator, ThreadContext, worker_count);

  for (u32 i = 0; i < worker_count; i++) {
    void *arena_mem = ALLOC_ARRAY(allocator, u8, WORKER_ARENA_SIZE);
    sys.worker_contexts[i] = (ThreadContext){
        .thread_idx = (u8)(i + 1),
        .thread_count = (u8)(worker_count + 1),
        .broadcast_memory = &sys.broadcast_memory,
        .barrier = &sys.barrier,
        .temp_arena = arena_from_buffer(arena_mem, WORKER_ARENA_SIZE),
        .task_system = &sys,
    };
  }

  void *main_arena_mem = ALLOC_ARRAY(allocator, u8, WORKER_ARENA_SIZE);
  sys.main_thread_context = (ThreadContext){
      .thread_idx = 0,
      .thread_count = (u8)(worker_count + 1),
      .broadcast_memory = &sys.broadcast_memory,
      .barrier = &sys.barrier,
      .temp_arena = arena_from_buffer(main_arena_mem, WORKER_ARENA_SIZE),
      .task_system = &sys,
  };

  return sys;
}

void task_system_start_workers(TaskSystem *sys) {
  sys->main_thread_context.task_system = sys;
  tctx_set_current(&sys->main_thread_context);

  for (u32 i = 0; i < sys->worker_count; i++) {
    sys->worker_contexts[i].task_system = sys;
    sys->workers[i] = thread_launch(task_worker_proc, &sys->worker_contexts[i]);
  }
}

void task_system_shutdown(TaskSystem *sys) {
  sys->should_quit = true;

  for (u32 i = 0; i < sys->worker_count; i++) {
    semaphore_drop(sys->work_semaphore);
  }

  for (u32 i = 0; i < sys->worker_count; i++) {
    thread_join(sys->workers[i], 0);
  }

  semaphore_release(sys->work_semaphore);
  barrier_release(sys->barrier);
}

TaskHandle task_schedule(TaskSystem *sys, TaskFunc fn, void *data) {
  return task_schedule_after(sys, fn, data, NULL, 0);
}

TaskHandle task_schedule_after(TaskSystem *sys, TaskFunc fn, void *data,
                               TaskHandle *deps, u8 dep_count) {
  TaskQueue *q = &sys->queue;

  TaskHandle handle = _task_queue_append(q, fn, data, NULL, 0, deps, dep_count);

  if (dep_count == 0) {
    ins_atomic_u32_inc_eval(&sys->tasks_in_flight);
    semaphore_drop(sys->work_semaphore);
  }

  return handle;
}

void task_queue_wait(TaskSystem *sys) {
  // Wake all workers to help
  for (u32 i = 0; i < sys->worker_count; i++) {
    semaphore_drop(sys->work_semaphore);
  }
  while (sys->tasks_in_flight > 0) {
    cpu_pause();
  }
  task_queue_reset(sys);
}

void task_queue_flush(TaskSystem *sys) {
  TaskQueue *q = &sys->queue;

  while (sys->tasks_in_flight > 0) {
    u64 idx = ins_atomic_u64_inc_eval(&q->ready_counter) - 1;
    u64 count = ins_atomic_load_acquire64(&q->ready_count);
    if (idx < count) {
      TaskHandle handle = q->ready_queue[idx];
      task_execute_one(sys, handle);
      ins_atomic_u32_dec_eval(&sys->tasks_in_flight);
    } else {
      cpu_pause();
    }
  }

  task_queue_reset(sys);
}

void task_queue_reset(TaskSystem *sys) {
  TaskQueue *q = &sys->queue;
  q->tasks_count = 0;
  q->ready_write_idx = 0;
  q->ready_count = 0;
  q->ready_counter = 0;
  q->next_ready_write_idx = 0;
  q->next_ready_count = 0;
  sys->tasks_in_flight = 0;
}

#include "task.h"
#include "lib/thread_context.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

TaskHandle _task_queue_append(TaskQueue *queue, TaskFunc fn, void *data,
                              TaskResourceAccess *resources, u8 resources_count,
                              TaskHandle *deps, u8 dep_count) {
  u64 next_task_id = ins_atomic_u64_inc_eval(&queue->tasks_count) - 1;

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
      u32 next_dependent_id =
          ins_atomic_u32_inc_eval(&dependency_task->dependents_count) - 1;
      dependency_task->dependent_task_ids[next_dependent_id] = this_task_handle;
    }
  } else {
    // if we don't have dependencies add ourselves to the ready queue directly
    u64 next_ready_id = ins_atomic_u64_inc_eval(&queue->ready_count) - 1;
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
  printf("thread %d: start processing queue\n", tctx->thread_idx);

process_queue_loop:
  for (;;) {
    u64 ready_idx = ins_atomic_u64_inc_eval(&queue->ready_counter) - 1;
    // we have ready tasks
    if (ready_idx < queue->ready_count) {
      TaskHandle ready_task_handle = queue->ready_queue[ready_idx];
      printf("thread %d: executing task handle %d (%d)\n", tctx->thread_idx,
             ready_idx, ready_task_handle.h[0]);
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
          printf("thread %d: adding task %d to ready queue\n", tctx->thread_idx,
                 dependent_handle.h[0]);
          // todo: fix repeated code (thread safe array?)
          u64 next_ready_id =
              ins_atomic_u64_inc_eval(&queue->next_ready_count) - 1;
          queue->next_ready_queue[next_ready_id] = dependent_handle;
        }
      }
      printf("thread %d: done executing task %d\n", tctx->thread_idx,
             ready_task_handle.h[0]);
    } else {
      // done
      break;
    }
  }

  // we need sync here to make a sure a thread doesn't early exit before next
  // ready queue has been appended
  lane_sync();
  printf("thread %d: finished processing ready queue, checking for next ready "
         "queue. count %d\n",
         tctx->thread_idx, queue->next_ready_count);

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

  printf("thread %d: done processing queue\n", tctx->thread_idx);

  queue->ready_counter = 0;
  queue->ready_count = 0;
  queue->tasks_count = 0;
  queue->next_ready_count = 0;
  lane_sync();
}

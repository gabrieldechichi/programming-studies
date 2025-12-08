#include "thread_context.h"
#include <string.h>

#ifdef _WIN32
// Windows doesn't have unistd.h
#else
#include <unistd.h>
#endif

thread_static ThreadContext *tctx_thread_local;

i8 os_core_count() {
#ifdef _WIN32
  return (i8)os_get_processor_count();
#else
  return (i8)sysconf(_SC_NPROCESSORS_ONLN);
#endif
}

b32 is_main_thread() {
  ThreadContext *ctx = tctx_current();
  return ctx && ctx->thread_idx == 0;
}

ThreadContext *tctx_current() { return tctx_thread_local; }
void tctx_set_current(ThreadContext *ctx) { tctx_thread_local = ctx; }

void _lane_sync_u64(ThreadContext *ctx, u32 broadcast_thread_idx,
                    u64 *value_ptr) {
  if (value_ptr && ctx->thread_idx == broadcast_thread_idx) {
    memcpy(ctx->broadcast_memory, value_ptr, sizeof(u64));
  }
  barrier_wait(*ctx->barrier);

  if (value_ptr && ctx->thread_idx != broadcast_thread_idx) {
    memcpy(value_ptr, ctx->broadcast_memory, sizeof(u64));
  }
  barrier_wait(*ctx->barrier);
}

void _lane_sync(ThreadContext *ctx) { barrier_wait(*ctx->barrier); }

Range_u64 _lane_range(ThreadContext *ctx, u64 values_count) {
  u32 thread_count = ctx->thread_count;
  u32 thread_idx = ctx->thread_idx;

  u64 values_per_thread = values_count / thread_count;
  u64 leftover_values_count = values_count % thread_count;
  b32 thread_has_leftover = thread_idx < leftover_values_count;
  u64 leftover_count_before_this_thread =
      thread_has_leftover ? thread_idx : leftover_values_count;

  u64 thread_first_value_idx =
      (values_per_thread * thread_idx + leftover_count_before_this_thread);
  u64 thread_opl_value_idx =
      thread_first_value_idx + values_per_thread + !!thread_has_leftover;

  Range_u64 range = {
      .min = thread_first_value_idx,
      .max = thread_opl_value_idx,
  };
  return range;
}

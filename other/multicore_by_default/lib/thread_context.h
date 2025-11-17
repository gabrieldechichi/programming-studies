#ifndef H_THREAD
#define H_THREAD

#include "memory.h"
#include "thread.h"
#include "typedefs.h"

#define thread_static __thread
#define atomic_add(ptr, value) __sync_fetch_and_add((u64 *)(ptr), (value))
#define atomic_inc_eval(ptr) __sync_add_and_fetch((ptr), 1)
#define atomic_dec_eval(ptr) __sync_add_and_fetch((ptr), -1)
#define atomic_set(ptr, value) __sync_lock_test_and_set((ptr), (value))
// todo: fix weird macros in typedef.h
//  #define atomic_load(ptr) __atomic_load_n((ptr), __ATOMIC_SEQ_CST)

force_inline void cpu_pause() {
#if defined(__x86_64__) || defined(__i386__)
  __asm__ __volatile__("pause" ::: "memory");
#elif defined(__aarch64__) || defined(__arm64__)
  __asm__ __volatile__("yield" ::: "memory"); // ARM equivalent
#else
  __asm__ __volatile__("" ::: "memory"); // Fallback: just a barrier
#endif
}

typedef struct ThreadContext {
  u8 thread_idx;
  u8 thread_count;
  u64 *broadcast_memory;
  Barrier *barrier;
  ArenaAllocator temp_arena;
} ThreadContext;

i8 os_core_count();

ThreadContext *tctx_current();
void tctx_set_current(ThreadContext *ctx);

b32 is_main_thread();

void _lane_sync_u64(ThreadContext *ctx, u32 broadcast_thread_idx,
                    u64 *value_ptr);

void _lane_sync(ThreadContext *ctx);

Range_u64 _lane_range(ThreadContext *ctx, u64 values_count);

#define lane_sync_u64(broadcast_thread_idx, value_ptr)                         \
  _lane_sync_u64(tctx_current(), (broadcast_thread_idx), (u64 *)(value_ptr))
#define lane_sync() _lane_sync(tctx_current())
#define lane_range(values_count) _lane_range(tctx_current(), (values_count))

#endif

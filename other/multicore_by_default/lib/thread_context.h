#ifndef H_THREAD
#define H_THREAD

#if defined(COMPILER_MSVC)
#include <intrin.h>
#include <windows.h>
#endif

#include "memory.h"
#include "thread.h"
#include "typedefs.h"

#ifdef _WIN32
#define ins_compiler_barrier() _ReadWriteBarrier()
#else
#define ins_compiler_barrier() asm volatile("" ::: "memory")
#endif
 #ifdef _WIN32
  #define memory_fence() MemoryBarrier()
  #else
  #define memory_fence() __atomic_thread_fence(__ATOMIC_SEQ_CST)
  #endif
// todo: fix weird macros in typedef.h
//  #define atomic_load(ptr) __atomic_load_n((ptr), __ATOMIC_SEQ_CST)

#if defined(COMPILER_MSVC)
#define thread_static __declspec(thread)
#define ins_atomic_u64_inc_eval(x)              InterlockedIncrement64((__int64 *)(x))
#define ins_atomic_u64_dec_eval(x)              InterlockedDecrement64((__int64 *)(x))
#define ins_atomic_u64_add_eval(x,c)            InterlockedAdd64((__int64 *)(x), c)
#define ins_atomic_u64_eval_assign(x,c)         InterlockedExchange64((__int64 *)(x),(c))
#define ins_atomic_u32_inc_eval(x)              InterlockedIncrement((LONG *)(x))
#define ins_atomic_u32_dec_eval(x)              InterlockedDecrement((LONG *)(x))
#define ins_atomic_u32_add_eval(x,c)            InterlockedAdd((LONG *)(x), (c))
#define ins_atomic_u32_eval_assign(x,c)         InterlockedExchange((LONG *)(x),(c))
#define ins_atomic_load_acquire(x)              _InterlockedOr((volatile LONG *)(x), 0)
#define ins_atomic_store_release(x,v)           _InterlockedExchange((volatile LONG *)(x), (LONG)(v))
#define ins_atomic_load_acquire64(x)            _InterlockedOr64((volatile __int64 *)(x), 0)
#define ins_atomic_store_release64(x,v)         _InterlockedExchange64((volatile __int64 *)(x), (__int64)(v))
#else
#define thread_static __thread
#define ins_atomic_u64_inc_eval(x)              (__atomic_fetch_add((u64 *)(x), 1, __ATOMIC_SEQ_CST) + 1)
#define ins_atomic_u64_dec_eval(x)              (__atomic_fetch_sub((u64 *)(x), 1, __ATOMIC_SEQ_CST) - 1)
#define ins_atomic_u64_add_eval(x,c)            (__atomic_fetch_add((u64 *)(x), c, __ATOMIC_SEQ_CST) + (c))
#define ins_atomic_u64_eval_assign(x,c)         __atomic_exchange_n(x, c, __ATOMIC_SEQ_CST)
#define ins_atomic_u32_inc_eval(x)              (__atomic_fetch_add((u32 *)(x), 1, __ATOMIC_SEQ_CST) + 1)
#define ins_atomic_u32_dec_eval(x)              (__atomic_fetch_sub((u32 *)(x), 1, __ATOMIC_SEQ_CST) - 1)
#define ins_atomic_u32_add_eval(x,c)            (__atomic_fetch_add((u32 *)(x), c, __ATOMIC_SEQ_CST) + (c))
#define ins_atomic_u32_eval_assign(x,c)         __atomic_exchange_n((x), (c), __ATOMIC_SEQ_CST)
#define ins_atomic_load_acquire(x)              __atomic_load_n((x), __ATOMIC_ACQUIRE)
#define ins_atomic_store_release(x,v)           __atomic_store_n((x), (v), __ATOMIC_RELEASE)
#define ins_atomic_load_acquire64(x)            __atomic_load_n((x), __ATOMIC_ACQUIRE)
#define ins_atomic_store_release64(x,v)         __atomic_store_n((x), (v), __ATOMIC_RELEASE)
#endif

force_inline void cpu_pause() {
#if defined(COMPILER_MSVC)
  #if defined(_M_X64) || defined(_M_IX86)
    _mm_pause();
  #elif defined(_M_ARM64) || defined(_M_ARM)
    __yield();
  #else
    _ReadWriteBarrier();
  #endif
#elif defined(__x86_64__) || defined(__i386__)
  __asm__ __volatile__("pause" ::: "memory");
#elif defined(__aarch64__) || defined(__arm64__)
  __asm__ __volatile__("yield" ::: "memory"); // ARM equivalent
#else
  __asm__ __volatile__("" ::: "memory"); // Fallback: just a barrier
#endif
}

typedef struct TaskSystem TaskSystem;

typedef struct ThreadContext {
  u8 thread_idx;
  u8 thread_count;
  u64 *broadcast_memory;
  Barrier *barrier;
  ArenaAllocator temp_arena;
  TaskSystem *task_system;
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

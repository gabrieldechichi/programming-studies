#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <string.h>
#include "pthread_barrier.h"
#include "unistd.h"
#include "pthread_barrier.c"

#define ARRAY_SIZE 1000000000
// #define ARRAY_SIZE 1000000

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef u32 b32;

#if COMPILER_MSVC
#define thread_static __declspec(thread)
#elif COMPILER_CLANG || COMPILER_GCC
#define thread_static __thread
#else
#error thread_static not defined for this compiler.
#endif

typedef struct ThreadContext {
  u8 thread_idx;
  u8 thread_count;
  u64 *broadcast_memory;
  pthread_barrier_t *barrier;
} ThreadContext;

void lane_sync_u64(ThreadContext *ctx, u32 broadcast_thread_idx,
                   u64 *value_ptr) {
  // broadcast from source thread -> other threads
  if (value_ptr && ctx->thread_idx == broadcast_thread_idx) {
    memcpy(ctx->broadcast_memory, value_ptr, sizeof(u64));
  }
  pthread_barrier_wait(ctx->barrier);

  // receive value <- from broadcast thread
  if (value_ptr && ctx->thread_idx != broadcast_thread_idx) {
    memcpy(value_ptr, ctx->broadcast_memory, sizeof(u64));
  }
  pthread_barrier_wait(ctx->barrier);
}

#define RANGE_DEFINE(type)                                                     \
  typedef struct Range_##type {                                                \
    type min;                                                                  \
    type max;                                                                  \
  } Range_##type;

RANGE_DEFINE(u64);

thread_static ThreadContext *tctx_thread_local;

ThreadContext *tctx_current() { return tctx_thread_local; }
void tctx_set_current(ThreadContext *ctx) { tctx_thread_local = ctx; }

void entrypoint() {
  ThreadContext *ctx = tctx_current();
  u64 array_size = 0;
  i64 *array = NULL;

  if (ctx->thread_idx == 0) {
    array_size = ARRAY_SIZE;
    array = malloc(array_size * sizeof(i64));
    for (u64 i = 0; i < array_size; i++) {
      array[i] = i + 1; // 1, 2, 3, ...
    }
  }

  lane_sync_u64(ctx, 0, &array_size);
  lane_sync_u64(ctx, 0, (u64 *)&array);

  u64 values_count = array_size;
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

  printf("Thread %d start %lld end %lld leftovers %d\n", ctx->thread_idx,
         range.min, range.max, thread_has_leftover);

  // Compute sum
  i64 sum = 0;
  u64 *sum_ptr = (u64 *)&sum;
  lane_sync_u64(ctx, 0, sum_ptr);

  i64 lane_sum = 0;
  for (u64 i = range.min; i < range.max; i++) {
    lane_sum += array[i];
  }

  __sync_fetch_and_add(sum_ptr, lane_sum);

  lane_sync_u64(ctx, 0, sum_ptr);

  if (ctx->thread_idx == 0) {
    printf("Sum: %lld\n", sum);
  }
}

void *entrypoint_internal(void *arg) {
  ThreadContext *ctx = (ThreadContext *)arg;
  tctx_set_current(ctx);

  entrypoint();
  return NULL;
}

i8 os_core_count() { return (i8)sysconf(_SC_NPROCESSORS_ONLN); }

int main(void) {
  const u8 thread_mult = 1;
  i8 core_count = os_core_count();
  u8 thread_count = core_count * thread_mult;

  printf("Core count %d Thread count %d\n", core_count, thread_count);

  pthread_t *threads = malloc(thread_count * sizeof(pthread_t));
  ThreadContext *ctx_arr = malloc(thread_count * sizeof(ThreadContext));
  pthread_barrier_t barrier;
  pthread_barrier_init(&barrier, NULL, thread_count);

  u64 broadcast_memory = 0;
  for (u8 i = 0; i < thread_count; i++) {
    ctx_arr[i] = (ThreadContext){
        .thread_idx = i,
        .thread_count = thread_count,
        .barrier = &barrier,
        .broadcast_memory = &broadcast_memory,
    };
    pthread_create(&threads[i], NULL, entrypoint_internal, &ctx_arr[i]);
  }

  for (u8 i = 0; i < thread_count; i++) {
    pthread_join(threads[i], NULL);
  }

  return 0;
}

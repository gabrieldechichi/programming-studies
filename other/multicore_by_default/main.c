#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include "pthread_barrier.h"
#include "unistd.h"
#include "pthread_barrier.c"

// #define ARRAY_SIZE 1000000000
#define ARRAY_SIZE 1000000

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
  pthread_barrier_t *barrier;
} ThreadContext;

typedef struct AppContext {
  u64 array_size;
  i64 *array;
  u8 thread_count;
  u64 *thread_sums;
} AppContext;

static AppContext *app_ctx = NULL;

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

  if (ctx->thread_idx == 0) {
    app_ctx->array_size = ARRAY_SIZE;
    app_ctx->array = malloc(app_ctx->array_size * sizeof(i64));
    for (u64 i = 0; i < app_ctx->array_size; i++) {
      app_ctx->array[i] = i + 1; // 1, 2, 3, ...
    }
  }
  pthread_barrier_wait(ctx->barrier);

  u64 values_count = app_ctx->array_size;
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
  {
    i64 sum = 0;
    for (u64 i = range.min; i < range.max; i++) {
      sum += app_ctx->array[i];
    }
    app_ctx->thread_sums[ctx->thread_idx] = sum;
  }

  pthread_barrier_wait(ctx->barrier);

  if (ctx->thread_idx == 0) {
    i64 sum = 0;
    for (u64 i = 0; i < app_ctx->thread_count; i++) {
      sum += app_ctx->thread_sums[i];
    }
    printf("Sum: %lld\n", (u64)sum);
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

  app_ctx = calloc(1, sizeof(AppContext));
  app_ctx->thread_count = thread_count;
  app_ctx->thread_sums = calloc(1, sizeof(u64) * thread_count);

  for (u8 i = 0; i < thread_count; i++) {
    ctx_arr[i] = (ThreadContext){
        .thread_idx = i,
        .thread_count = thread_count,
        .barrier = &barrier,
    };
    pthread_create(&threads[i], NULL, entrypoint_internal, &ctx_arr[i]);
  }

  for (u8 i = 0; i < thread_count; i++) {
    pthread_join(threads[i], NULL);
  }

  return 0;
}

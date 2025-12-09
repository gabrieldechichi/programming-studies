// Demo 7: Atomics
// Tests: atomic increment, load, store operations

#include "os/os.h"
#include "lib/thread_context.h"

#define NUM_THREADS 4
#define ITERATIONS 10000

static u32 atomic_counter = 0;
static i32 regular_counter = 0;

typedef struct {
    i32 id;
} AtomicsThreadArg;

void thread_func(void* arg) {
    AtomicsThreadArg* targ = (AtomicsThreadArg*)arg;
    i32 id = targ->id;

    // Test atomic increment
    for (i32 i = 0; i < ITERATIONS; i++) {
        ins_atomic_u32_inc_eval(&atomic_counter);
        regular_counter++;  // For comparison (will have races)
    }

    LOG_INFO("Thread %: completed", FMT_INT(id));
}

int demo_main(void) {
    LOG_INFO("=== Demo: Atomics ===");

    i32 expected = NUM_THREADS * ITERATIONS;
    LOG_INFO("Testing atomic operations with % threads x % iterations", FMT_INT(NUM_THREADS), FMT_INT(ITERATIONS));

    Thread threads[NUM_THREADS];
    AtomicsThreadArg thread_args[NUM_THREADS];

    // Create threads
    for (i32 i = 0; i < NUM_THREADS; i++) {
        thread_args[i].id = i;
        threads[i] = thread_launch(thread_func, &thread_args[i]);
    }

    // Join threads
    for (i32 i = 0; i < NUM_THREADS; i++) {
        thread_join(threads[i], 0);
    }

    // Results
    i32 atomic_result = (i32)atomic_counter;

    LOG_INFO("Results:");
    LOG_INFO("  atomic counter: % (expected %)", FMT_INT(atomic_result), FMT_INT(expected));
    LOG_INFO("  regular counter: % (expected %)", FMT_INT(regular_counter), FMT_INT(expected));

    if (regular_counter != expected) {
        LOG_INFO("    ^ Lost % increments due to races", FMT_INT(expected - regular_counter));
    }

    // Verify atomic counter
    if (atomic_result != expected) {
        LOG_ERROR("[FAIL] Atomic counter has wrong value!");
        return 1;
    }

    LOG_INFO("[PASS] Atomic operations work correctly!");
    LOG_INFO("  - atomic increment provides race-free increments");

    return 0;
}

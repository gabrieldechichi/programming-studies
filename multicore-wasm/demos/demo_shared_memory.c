// Demo 3: Shared Memory
// Tests: Multiple threads reading/writing same global memory
// Demonstrates that threads CAN see each other's writes (unlike TLS)
// Also shows race conditions when no synchronization is used

#include "os/os.h"

#define NUM_THREADS 4
#define ITERATIONS 10000

// Shared memory - all threads access the same variable
static i32 shared_counter = 0;
static i32 shared_array[NUM_THREADS] = {0};

typedef struct {
    i32 id;
} SharedMemThreadArg;

void thread_func(void* arg) {
    SharedMemThreadArg* targ = (SharedMemThreadArg*)arg;
    i32 id = targ->id;

    // Write to our slot in the shared array (no race - each thread owns a slot)
    shared_array[id] = id * 100;
    LOG_INFO("Thread %: wrote % to shared_array[%]", FMT_INT(id), FMT_INT(shared_array[id]), FMT_INT(id));

    // Increment shared counter (WILL HAVE RACES - intentional!)
    for (i32 i = 0; i < ITERATIONS; i++) {
        shared_counter++;  // This is NOT atomic!
    }
}

int demo_main(void) {
    LOG_INFO("=== Demo: Shared Memory ===");

    Thread threads[NUM_THREADS];
    SharedMemThreadArg thread_args[NUM_THREADS];

    LOG_INFO("Initial shared_counter = %", FMT_INT(shared_counter));
    LOG_INFO("Expected final value (if no races) = %", FMT_INT(NUM_THREADS * ITERATIONS));

    // Create threads
    for (i32 i = 0; i < NUM_THREADS; i++) {
        thread_args[i].id = i;
        threads[i] = thread_launch(thread_func, &thread_args[i]);
    }

    // Join threads
    for (i32 i = 0; i < NUM_THREADS; i++) {
        thread_join(threads[i], 0);
    }

    // Check shared array (should be correct - no races)
    LOG_INFO("Shared array contents:");
    b32 array_correct = true;
    for (i32 i = 0; i < NUM_THREADS; i++) {
        i32 expected = i * 100;
        if (shared_array[i] == expected) {
            LOG_INFO("  shared_array[%] = % (expected %) [OK]",
                     FMT_INT(i), FMT_INT(shared_array[i]), FMT_INT(expected));
        } else {
            LOG_ERROR("  shared_array[%] = % (expected %) [WRONG]",
                      FMT_INT(i), FMT_INT(shared_array[i]), FMT_INT(expected));
            array_correct = false;
        }
    }

    // Check shared counter (likely has races)
    i32 expected = NUM_THREADS * ITERATIONS;
    LOG_INFO("Shared counter = % (expected %)", FMT_INT(shared_counter), FMT_INT(expected));

    if (shared_counter == expected) {
        LOG_INFO("  Note: Counter matches expected! (got lucky or single-core execution)");
    } else {
        i32 lost = expected - shared_counter;
        LOG_INFO("  Lost % increments due to race conditions", FMT_INT(lost));
    }

    if (array_correct) {
        LOG_INFO("[PASS] Shared memory is accessible from all threads!");
        LOG_INFO("  - Non-overlapping writes work correctly");
        LOG_INFO("  - Race conditions occur with concurrent modifications");
        LOG_INFO("  - Use mutexes or atomics to fix races (see other demos)");
    } else {
        LOG_ERROR("[FAIL] Shared array was corrupted!");
        return 1;
    }

    return 0;
}

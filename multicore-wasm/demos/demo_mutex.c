// Demo 4: Mutex
// Tests: mutex_take/mutex_drop - protecting shared counter

#include "os/os.h"

#define NUM_THREADS 4
#define ITERATIONS 10000

typedef struct {
    i32 id;
    Mutex *mutex;
    i32 *protected_counter;
    i32 *unprotected_counter;
} ThreadArg;

void thread_func(void* arg) {
    ThreadArg *targ = (ThreadArg *)arg;

    for (i32 i = 0; i < ITERATIONS; i++) {
        // Protected increment (with mutex)
        mutex_take(*targ->mutex);
        (*targ->protected_counter)++;
        mutex_drop(*targ->mutex);

        // Unprotected increment (for comparison)
        (*targ->unprotected_counter)++;
    }

    LOG_INFO("Thread %: completed % iterations", FMT_INT(targ->id), FMT_INT(ITERATIONS));
}

int demo_main(void) {
    LOG_INFO("=== Demo: Mutex ===");

    Mutex mutex = mutex_alloc();
    i32 protected_counter = 0;
    i32 unprotected_counter = 0;

    Thread threads[NUM_THREADS];
    ThreadArg thread_args[NUM_THREADS];

    i32 expected = NUM_THREADS * ITERATIONS;
    LOG_INFO("Each of % threads will increment counters % times", FMT_INT(NUM_THREADS), FMT_INT(ITERATIONS));
    LOG_INFO("Expected final value: %", FMT_INT(expected));

    // Create threads
    for (i32 i = 0; i < NUM_THREADS; i++) {
        thread_args[i].id = i;
        thread_args[i].mutex = &mutex;
        thread_args[i].protected_counter = &protected_counter;
        thread_args[i].unprotected_counter = &unprotected_counter;
        threads[i] = thread_launch(thread_func, &thread_args[i]);
    }

    // Join threads
    for (i32 i = 0; i < NUM_THREADS; i++) {
        thread_join(threads[i], 0);
    }

    // Results
    LOG_INFO("Results:");
    LOG_INFO("  Protected counter:   % (expected %)", FMT_INT(protected_counter), FMT_INT(expected));
    LOG_INFO("  Unprotected counter: % (expected %)", FMT_INT(unprotected_counter), FMT_INT(expected));

    if (protected_counter != expected) {
        LOG_ERROR("[FAIL] Mutex did not protect the counter!");
        mutex_release(mutex);
        return 1;
    }

    if (unprotected_counter == expected) {
        LOG_INFO("  Note: Unprotected counter also correct (got lucky or single-core)");
    } else {
        i32 lost = expected - unprotected_counter;
        LOG_INFO("  Unprotected counter lost % increments", FMT_INT(lost));
    }

    LOG_INFO("[PASS] Mutex correctly protects shared data!");

    // Cleanup
    mutex_release(mutex);

    return 0;
}

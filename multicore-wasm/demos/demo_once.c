// Demo 12: Once Initialization
// Tests: pthread_once - one-time initialization across threads

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdatomic.h>

#define NUM_THREADS 8

static pthread_once_t once_control = PTHREAD_ONCE_INIT;
static atomic_int init_call_count = 0;
static int initialized_value = 0;
static int initializer_thread_id = -1;

// This function should only be called ONCE, regardless of how many threads try
void init_function(void) {
    int count = atomic_fetch_add(&init_call_count, 1) + 1;
    printf("init_function: called (count=%d)\n", count);

    // Simulate expensive initialization
    for (volatile int i = 0; i < 100000; i++) {}

    initialized_value = 42;
    printf("init_function: initialization complete, value=%d\n", initialized_value);
}

void* thread_func(void* arg) {
    int id = *(int*)arg;
    printf("Thread %d: calling pthread_once...\n", id);

    // All threads call pthread_once, but init_function runs only once
    int ret = pthread_once(&once_control, init_function);

    if (ret != 0) {
        printf("Thread %d: ERROR - pthread_once failed (error %d)\n", id, ret);
        return (void*)1;
    }

    printf("Thread %d: pthread_once returned, value=%d\n", id, initialized_value);

    // Verify the value is correctly initialized
    if (initialized_value != 42) {
        printf("Thread %d: ERROR - wrong initialized value!\n", id);
        return (void*)1;
    }

    return (void*)0;
}

int demo_main(void) {
    printf("=== Demo: Once Initialization ===\n\n");

    printf("%d threads will all call pthread_once simultaneously\n", NUM_THREADS);
    printf("init_function should only execute ONCE\n\n");

    pthread_t threads[NUM_THREADS];
    int thread_ids[NUM_THREADS];

    // Create all threads
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_ids[i] = i;
        int ret = pthread_create(&threads[i], NULL, thread_func, &thread_ids[i]);
        if (ret != 0) {
            printf("ERROR: pthread_create failed (error %d)\n", ret);
            return 1;
        }
    }

    // Join all threads
    int failures = 0;
    for (int i = 0; i < NUM_THREADS; i++) {
        void* result;
        pthread_join(threads[i], &result);
        if ((intptr_t)result != 0) {
            failures++;
        }
    }

    // Results
    int calls = atomic_load(&init_call_count);
    printf("\nResults:\n");
    printf("  init_function was called: %d time(s)\n", calls);
    printf("  Final initialized_value: %d (expected 42)\n", initialized_value);
    printf("  Thread failures: %d\n", failures);

    if (calls != 1) {
        printf("\n[FAIL] init_function was called more than once!\n");
        return 1;
    }

    if (initialized_value != 42) {
        printf("\n[FAIL] Initialization produced wrong value!\n");
        return 1;
    }

    if (failures > 0) {
        printf("\n[FAIL] Some threads reported errors!\n");
        return 1;
    }

    printf("\n[PASS] pthread_once works correctly!\n");
    printf("  - init_function executed exactly once\n");
    printf("  - All %d threads saw the initialized value\n", NUM_THREADS);
    printf("  - Thread-safe one-time initialization achieved\n");

    return 0;
}

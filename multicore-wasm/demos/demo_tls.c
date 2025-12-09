// Demo 2: Thread Local Storage
// Tests: thread_local / _Thread_local variables - each thread has its own copy

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define NUM_THREADS 4
#define ITERATIONS 1000

// Thread-local variable - each thread gets its own copy
thread_local int tls_counter = 0;
thread_local int tls_thread_id = -1;

// Shared variable for comparison
int shared_counter = 0;

void* thread_func(void* arg) {
    int id = *(int*)arg;
    tls_thread_id = id;

    printf("Thread %d: TLS counter initial value = %d\n", id, tls_counter);

    // Increment thread-local counter
    for (int i = 0; i < ITERATIONS; i++) {
        tls_counter++;
    }

    printf("Thread %d: TLS counter final value = %d (expected %d)\n",
           id, tls_counter, ITERATIONS);

    // Verify TLS isolation
    if (tls_counter != ITERATIONS) {
        printf("Thread %d: [FAIL] TLS counter corrupted!\n", id);
        return (void*)1;
    }

    if (tls_thread_id != id) {
        printf("Thread %d: [FAIL] TLS thread_id corrupted! Got %d\n", id, tls_thread_id);
        return (void*)1;
    }

    return (void*)0;
}

int demo_main(void) {
    printf("=== Demo: Thread Local Storage ===\n\n");

    pthread_t threads[NUM_THREADS];
    int thread_ids[NUM_THREADS];

    // Verify main thread TLS
    tls_counter = 999;
    tls_thread_id = -999;
    printf("Main: Set TLS counter to %d, thread_id to %d\n", tls_counter, tls_thread_id);

    // Create threads
    printf("\nCreating %d threads...\n", NUM_THREADS);
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_ids[i] = i;
        int ret = pthread_create(&threads[i], NULL, thread_func, &thread_ids[i]);
        if (ret != 0) {
            printf("ERROR: pthread_create failed (error %d)\n", ret);
            return 1;
        }
    }

    // Join threads
    int failures = 0;
    for (int i = 0; i < NUM_THREADS; i++) {
        void* result;
        pthread_join(threads[i], &result);
        if ((intptr_t)result != 0) {
            failures++;
        }
    }

    // Verify main thread TLS wasn't affected
    printf("\nMain: TLS counter = %d (expected 999)\n", tls_counter);
    printf("Main: TLS thread_id = %d (expected -999)\n", tls_thread_id);

    if (tls_counter != 999 || tls_thread_id != -999) {
        printf("\n[FAIL] Main thread TLS was corrupted by worker threads!\n");
        return 1;
    }

    if (failures == 0) {
        printf("\n[PASS] Thread Local Storage works correctly!\n");
        printf("  - Each thread had isolated TLS variables\n");
        printf("  - Main thread TLS was not affected\n");
    } else {
        printf("\n[FAIL] %d threads had TLS issues!\n", failures);
        return 1;
    }

    return 0;
}

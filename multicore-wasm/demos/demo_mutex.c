// Demo 4: Mutex
// Tests: pthread_mutex_t - lock/unlock, protecting shared counter

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define NUM_THREADS 4
#define ITERATIONS 10000

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static int protected_counter = 0;
static int unprotected_counter = 0;

void* thread_func(void* arg) {
    int id = *(int*)arg;

    for (int i = 0; i < ITERATIONS; i++) {
        // Protected increment (with mutex)
        pthread_mutex_lock(&mutex);
        protected_counter++;
        pthread_mutex_unlock(&mutex);

        // Unprotected increment (for comparison)
        unprotected_counter++;
    }

    printf("Thread %d: completed %d iterations\n", id, ITERATIONS);
    return NULL;
}

int demo_main(void) {
    printf("=== Demo: Mutex ===\n\n");

    pthread_t threads[NUM_THREADS];
    int thread_ids[NUM_THREADS];

    int expected = NUM_THREADS * ITERATIONS;
    printf("Each of %d threads will increment counters %d times\n", NUM_THREADS, ITERATIONS);
    printf("Expected final value: %d\n\n", expected);

    // Create threads
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_ids[i] = i;
        int ret = pthread_create(&threads[i], NULL, thread_func, &thread_ids[i]);
        if (ret != 0) {
            printf("ERROR: pthread_create failed (error %d)\n", ret);
            return 1;
        }
    }

    // Join threads
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    // Results
    printf("\nResults:\n");
    printf("  Protected counter:   %d (expected %d)\n", protected_counter, expected);
    printf("  Unprotected counter: %d (expected %d)\n", unprotected_counter, expected);

    if (protected_counter != expected) {
        printf("\n[FAIL] Mutex did not protect the counter!\n");
        return 1;
    }

    if (unprotected_counter == expected) {
        printf("\n  Note: Unprotected counter also correct (single-core or lucky timing)\n");
    } else {
        int lost = expected - unprotected_counter;
        printf("\n  Unprotected counter lost %d increments (%.1f%% loss)\n",
               lost, (lost * 100.0) / expected);
    }

    printf("\n[PASS] Mutex correctly protects shared data!\n");

    // Cleanup
    pthread_mutex_destroy(&mutex);

    return 0;
}

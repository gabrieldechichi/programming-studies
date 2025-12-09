// Demo 3: Shared Memory
// Tests: Multiple threads reading/writing same global memory
// Demonstrates that threads CAN see each other's writes (unlike TLS)
// Also shows race conditions when no synchronization is used

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define NUM_THREADS 4
#define ITERATIONS 10000

// Shared memory - all threads access the same variable
static int shared_counter = 0;
static int shared_array[NUM_THREADS] = {0};

void* thread_func(void* arg) {
    int id = *(int*)arg;

    // Write to our slot in the shared array (no race - each thread owns a slot)
    shared_array[id] = id * 100;
    printf("Thread %d: wrote %d to shared_array[%d]\n", id, shared_array[id], id);

    // Increment shared counter (WILL HAVE RACES - intentional!)
    for (int i = 0; i < ITERATIONS; i++) {
        shared_counter++;  // This is NOT atomic!
    }

    return NULL;
}

int demo_main(void) {
    printf("=== Demo: Shared Memory ===\n\n");

    pthread_t threads[NUM_THREADS];
    int thread_ids[NUM_THREADS];

    printf("Initial shared_counter = %d\n", shared_counter);
    printf("Expected final value (if no races) = %d\n\n", NUM_THREADS * ITERATIONS);

    // Create threads
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_ids[i] = i;
        pthread_create(&threads[i], NULL, thread_func, &thread_ids[i]);
    }

    // Join threads
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    // Check shared array (should be correct - no races)
    printf("\nShared array contents:\n");
    int array_correct = 1;
    for (int i = 0; i < NUM_THREADS; i++) {
        int expected = i * 100;
        printf("  shared_array[%d] = %d (expected %d) %s\n",
               i, shared_array[i], expected,
               shared_array[i] == expected ? "[OK]" : "[WRONG]");
        if (shared_array[i] != expected) {
            array_correct = 0;
        }
    }

    // Check shared counter (likely has races)
    int expected = NUM_THREADS * ITERATIONS;
    printf("\nShared counter = %d (expected %d)\n", shared_counter, expected);

    if (shared_counter == expected) {
        printf("  Note: Counter matches expected! (got lucky or single-core execution)\n");
    } else {
        int lost = expected - shared_counter;
        printf("  Lost %d increments due to race conditions (%.1f%% loss)\n",
               lost, (lost * 100.0) / expected);
    }

    printf("\n");
    if (array_correct) {
        printf("[PASS] Shared memory is accessible from all threads!\n");
        printf("  - Non-overlapping writes work correctly\n");
        printf("  - Race conditions occur with concurrent modifications\n");
        printf("  - Use mutexes or atomics to fix races (see other demos)\n");
    } else {
        printf("[FAIL] Shared array was corrupted!\n");
        return 1;
    }

    return 0;
}

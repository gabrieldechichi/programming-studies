// Demo 5: Barrier
// Tests: pthread_barrier_t - synchronize N threads at a rendezvous point

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define NUM_THREADS 4
#define NUM_PHASES 3

static pthread_barrier_t barrier;
static int phase_results[NUM_PHASES][NUM_THREADS] = {0};

void* thread_func(void* arg) {
    int id = *(int*)arg;

    for (int phase = 0; phase < NUM_PHASES; phase++) {
        // Do some "work" for this phase
        phase_results[phase][id] = (phase + 1) * (id + 1);
        printf("Thread %d: completed phase %d (result=%d)\n",
               id, phase, phase_results[phase][id]);

        // Wait for all threads to complete this phase
        int ret = pthread_barrier_wait(&barrier);

        // One thread gets PTHREAD_BARRIER_SERIAL_THREAD, others get 0
        if (ret == PTHREAD_BARRIER_SERIAL_THREAD) {
            printf("--- Thread %d: all threads reached barrier (phase %d complete) ---\n",
                   id, phase);
        } else if (ret != 0) {
            printf("Thread %d: ERROR - barrier_wait returned %d\n", id, ret);
        }
    }

    return NULL;
}

int demo_main(void) {
    printf("=== Demo: Barrier ===\n\n");

    printf("%d threads will synchronize at barriers through %d phases\n\n",
           NUM_THREADS, NUM_PHASES);

    // Initialize barrier for NUM_THREADS
    int ret = pthread_barrier_init(&barrier, NULL, NUM_THREADS);
    if (ret != 0) {
        printf("ERROR: pthread_barrier_init failed (error %d)\n", ret);
        return 1;
    }

    pthread_t threads[NUM_THREADS];
    int thread_ids[NUM_THREADS];

    // Create threads
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_ids[i] = i;
        ret = pthread_create(&threads[i], NULL, thread_func, &thread_ids[i]);
        if (ret != 0) {
            printf("ERROR: pthread_create failed (error %d)\n", ret);
            return 1;
        }
    }

    // Join threads
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    // Verify all phase results
    printf("\nVerifying phase results...\n");
    int errors = 0;
    for (int phase = 0; phase < NUM_PHASES; phase++) {
        printf("Phase %d: ", phase);
        for (int t = 0; t < NUM_THREADS; t++) {
            int expected = (phase + 1) * (t + 1);
            int got = phase_results[phase][t];
            printf("[%d]=%d ", t, got);
            if (got != expected) {
                errors++;
            }
        }
        printf("\n");
    }

    if (errors == 0) {
        printf("\n[PASS] Barrier synchronization works correctly!\n");
        printf("  - All threads waited for each other at each phase\n");
        printf("  - Results from all phases are correct\n");
    } else {
        printf("\n[FAIL] %d errors in phase results!\n", errors);
        return 1;
    }

    // Cleanup
    pthread_barrier_destroy(&barrier);

    return 0;
}

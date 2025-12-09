// Demo 1: Basic Thread Creation
// Tests: pthread_create, pthread_join, passing arguments, return values

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define NUM_THREADS 4

typedef struct {
    int thread_id;
    int input_value;
} ThreadArg;

typedef struct {
    int thread_id;
    int result;
} ThreadResult;

void* thread_func(void* arg) {
    ThreadArg* targ = (ThreadArg*)arg;

    printf("Thread %d: started with input value %d\n", targ->thread_id, targ->input_value);

    // Allocate result on heap (caller will free)
    ThreadResult* result = malloc(sizeof(ThreadResult));
    result->thread_id = targ->thread_id;
    result->result = targ->input_value * 2;

    printf("Thread %d: computed result %d\n", targ->thread_id, result->result);

    return result;
}

int demo_main(void) {
    printf("=== Demo: Thread Create/Join ===\n\n");

    pthread_t threads[NUM_THREADS];
    ThreadArg args[NUM_THREADS];

    // Create threads
    printf("Creating %d threads...\n", NUM_THREADS);
    for (int i = 0; i < NUM_THREADS; i++) {
        args[i].thread_id = i;
        args[i].input_value = (i + 1) * 10;

        int ret = pthread_create(&threads[i], NULL, thread_func, &args[i]);
        if (ret != 0) {
            printf("ERROR: pthread_create failed for thread %d (error %d)\n", i, ret);
            return 1;
        }
    }

    // Join threads and collect results
    printf("\nJoining threads and collecting results...\n");
    int total = 0;
    for (int i = 0; i < NUM_THREADS; i++) {
        ThreadResult* result;
        int ret = pthread_join(threads[i], (void**)&result);
        if (ret != 0) {
            printf("ERROR: pthread_join failed for thread %d (error %d)\n", i, ret);
            return 1;
        }

        printf("Main: Thread %d returned result %d\n", result->thread_id, result->result);
        total += result->result;
        free(result);
    }

    printf("\nTotal sum of all results: %d\n", total);
    printf("Expected: %d\n", (10 + 20 + 30 + 40) * 2);

    if (total == 200) {
        printf("\n[PASS] Thread create/join works correctly!\n");
    } else {
        printf("\n[FAIL] Unexpected result!\n");
        return 1;
    }

    return 0;
}

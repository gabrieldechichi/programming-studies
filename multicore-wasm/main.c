#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <emscripten/threading.h>

#include "lib/typedefs.h"

#define ARRAY_SIZE 12000

// Shared state across all threads (file-scope statics)
local_shared int* shared_array = NULL;
local_shared int num_cores = 0;
local_shared pthread_barrier_t barrier;

void* thread_entry(void* arg) {
    int idx = *(int*)arg;

    // Thread 0 allocates the shared array
    if (idx == 0) {
        shared_array = malloc(ARRAY_SIZE * sizeof(int));
        printf("Thread 0 allocated shared array at %p\n", (void*)shared_array);
    }

    // Wait for allocation to complete
    pthread_barrier_wait(&barrier);

    // Calculate this thread's range
    int chunk = ARRAY_SIZE / num_cores;
    int start = idx * chunk;
    int end = (idx == num_cores - 1) ? ARRAY_SIZE : start + chunk;

    // Fill the range with sequential values
    for (int i = start; i < end; i++) {
        shared_array[i] = i;
    }

    printf("Thread %d filled [%d, %d)\n", idx, start, end);

    // Wait for all threads to finish filling
    pthread_barrier_wait(&barrier);

    return NULL;
}

#define MIN_THREADS 16

int main() {
    num_cores = emscripten_num_logical_cores();
    if (num_cores < MIN_THREADS) { num_cores = MIN_THREADS; }
    printf("Detected %d cores\n", num_cores);

    pthread_barrier_init(&barrier, NULL, num_cores);

    pthread_t* threads = malloc(num_cores * sizeof(pthread_t));
    int* thread_indices = malloc(num_cores * sizeof(int));

    // Create threads
    for (int i = 0; i < num_cores; i++) {
        thread_indices[i] = i;
        pthread_create(&threads[i], NULL, thread_entry, &thread_indices[i]);
    }

    // Wait for all threads to complete
    for (int i = 0; i < num_cores; i++) {
        pthread_join(threads[i], NULL);
    }

    // Verify the array was filled correctly
    printf("Verifying array...\n");
    int errors = 0;
    for (int i = 0; i < ARRAY_SIZE; i++) {
        if (shared_array[i] != i) {
            printf("Error at index %d: expected %d, got %d\n", i, i, shared_array[i]);
            errors++;
            if (errors > 10) {
                printf("Too many errors, stopping verification\n");
                break;
            }
        }
    }

    if (errors == 0) {
        printf("All %d values verified correctly!\n", ARRAY_SIZE);
    }

    // Cleanup
    free(shared_array);
    free(threads);
    free(thread_indices);

    printf("Done!\n");
    return 0;
}

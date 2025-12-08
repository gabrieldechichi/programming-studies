#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <emscripten/threading.h>

#include "lib/typedefs.h"
#include "lib/thread_context.h"
#include "lib/array.h"

#define ARRAY_SIZE 12000

arr_define_concurrent(int);

// Shared state across all threads (file-scope statics)
local_shared ConcurrentArray(int) shared_array = {0};
local_shared int num_cores = 0;
local_shared pthread_barrier_t barrier;

void* thread_entry(void* arg) {
    int idx = *(int*)arg;

    // Thread 0 allocates the shared array buffer
    if (idx == 0) {
        shared_array.items = malloc(ARRAY_SIZE * sizeof(int));
        shared_array.cap = ARRAY_SIZE;
        shared_array.len_atomic = 0;
        printf("Thread 0 allocated shared array at %p\n", (void*)shared_array.items);
    }

    // Wait for allocation to complete
    pthread_barrier_wait(&barrier);

    // Calculate this thread's range
    int chunk = ARRAY_SIZE / num_cores;
    int start = idx * chunk;
    int end = (idx == num_cores - 1) ? ARRAY_SIZE : start + chunk;

    // Each thread appends its values using atomic operations
    for (int i = start; i < end; i++) {
        concurrent_arr_append(shared_array, i);
    }

    printf("Thread %d appended %d values\n", idx, end - start);

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
    u32 len = concurrent_arr_len(shared_array);
    printf("Verifying array (len=%u)...\n", len);

    // With concurrent append, values won't be in order - just verify all values 0..ARRAY_SIZE-1 exist
    int* seen = calloc(ARRAY_SIZE, sizeof(int));
    int errors = 0;
    for (u32 i = 0; i < len; i++) {
        int val = concurrent_arr_get(shared_array, i);
        if (val < 0 || val >= ARRAY_SIZE) {
            printf("Error: value %d out of range at index %u\n", val, i);
            errors++;
        } else {
            seen[val]++;
        }
    }

    // Check all values appeared exactly once
    for (int i = 0; i < ARRAY_SIZE; i++) {
        if (seen[i] != 1) {
            printf("Error: value %d appeared %d times (expected 1)\n", i, seen[i]);
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
    free(seen);
    free(shared_array.items);
    free(threads);
    free(thread_indices);

    printf("Done!\n");
    return 0;
}

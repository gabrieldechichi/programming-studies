// Demo 8: Semaphore
// Tests: sem_t - counting semaphore for resource limiting

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>

#define NUM_THREADS 8
#define MAX_CONCURRENT 3  // Only 3 threads can be in critical section at once
#define WORK_ITERATIONS 5

static sem_t semaphore;
static pthread_mutex_t print_mutex = PTHREAD_MUTEX_INITIALIZER;
static int current_in_section = 0;
static int max_observed = 0;

void* thread_func(void* arg) {
    int id = *(int*)arg;

    for (int i = 0; i < WORK_ITERATIONS; i++) {
        // Wait on semaphore (decrement)
        sem_wait(&semaphore);

        // Enter critical section
        pthread_mutex_lock(&print_mutex);
        current_in_section++;
        if (current_in_section > max_observed) {
            max_observed = current_in_section;
        }
        printf("Thread %d: ENTER (iteration %d, %d threads in section)\n",
               id, i, current_in_section);
        pthread_mutex_unlock(&print_mutex);

        // Simulate work in critical section
        for (volatile int j = 0; j < 10000; j++) {}

        // Leave critical section
        pthread_mutex_lock(&print_mutex);
        printf("Thread %d: LEAVE (iteration %d, %d threads in section)\n",
               id, i, current_in_section);
        current_in_section--;
        pthread_mutex_unlock(&print_mutex);

        // Signal semaphore (increment)
        sem_post(&semaphore);
    }

    return NULL;
}

int demo_main(void) {
    printf("=== Demo: Semaphore ===\n\n");

    printf("Testing counting semaphore:\n");
    printf("  %d threads competing\n", NUM_THREADS);
    printf("  Max %d allowed in critical section\n\n", MAX_CONCURRENT);

    // Initialize semaphore with count = MAX_CONCURRENT
    if (sem_init(&semaphore, 0, MAX_CONCURRENT) != 0) {
        printf("ERROR: sem_init failed\n");
        return 1;
    }

    pthread_t threads[NUM_THREADS];
    int thread_ids[NUM_THREADS];

    // Create threads
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_ids[i] = i;
        pthread_create(&threads[i], NULL, thread_func, &thread_ids[i]);
    }

    // Join threads
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    // Verify
    printf("\nResults:\n");
    printf("  Max threads observed in section: %d (limit was %d)\n",
           max_observed, MAX_CONCURRENT);
    printf("  Current in section: %d (should be 0)\n", current_in_section);

    if (max_observed > MAX_CONCURRENT) {
        printf("\n[FAIL] Semaphore allowed too many threads!\n");
        return 1;
    }

    if (current_in_section != 0) {
        printf("\n[FAIL] Not all threads exited cleanly!\n");
        return 1;
    }

    printf("\n[PASS] Semaphore correctly limits concurrency!\n");
    printf("  - Never exceeded %d concurrent threads\n", MAX_CONCURRENT);
    printf("  - All threads completed successfully\n");

    // Cleanup
    sem_destroy(&semaphore);
    pthread_mutex_destroy(&print_mutex);

    return 0;
}

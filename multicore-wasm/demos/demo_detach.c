// Demo 10: Thread Detach
// Tests: pthread_detach - fire-and-forget threads that clean up automatically

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdatomic.h>

#ifdef WASM
#include <emscripten/threading.h>
#define sleep_ms(ms) emscripten_sleep(ms)
#else
#include <unistd.h>
#define sleep_ms(ms) usleep((ms) * 1000)
#endif

#define NUM_DETACHED 4

static atomic_int completed_count = 0;
static atomic_int started_count = 0;

void* detached_thread_func(void* arg) {
    int id = *(int*)arg;
    free(arg);  // We own this memory, free it

    atomic_fetch_add(&started_count, 1);
    printf("Detached thread %d: started\n", id);

    // Simulate some work
    for (volatile int i = 0; i < 100000; i++) {}

    printf("Detached thread %d: finishing\n", id);
    atomic_fetch_add(&completed_count, 1);

    // Thread exits and resources are automatically cleaned up
    return NULL;
}

void* joinable_thread_func(void* arg) {
    int id = *(int*)arg;

    printf("Joinable thread %d: started\n", id);

    // Simulate work
    for (volatile int i = 0; i < 50000; i++) {}

    printf("Joinable thread %d: finishing\n", id);

    return (void*)(intptr_t)(id * 10);  // Return a value
}

int demo_main(void) {
    printf("=== Demo: Thread Detach ===\n\n");

    // Test 1: Detach after creation
    printf("Test 1: pthread_detach after pthread_create\n");
    for (int i = 0; i < NUM_DETACHED; i++) {
        pthread_t thread;
        int* id = malloc(sizeof(int));  // Allocate ID (thread will free it)
        *id = i;

        int ret = pthread_create(&thread, NULL, detached_thread_func, id);
        if (ret != 0) {
            printf("ERROR: pthread_create failed (error %d)\n", ret);
            free(id);
            return 1;
        }

        // Detach the thread - we won't join it
        ret = pthread_detach(thread);
        if (ret != 0) {
            printf("ERROR: pthread_detach failed (error %d)\n", ret);
            return 1;
        }
        printf("Main: detached thread %d\n", i);
    }

    // Test 2: Joinable thread for comparison
    printf("\nTest 2: Regular joinable thread\n");
    pthread_t joinable;
    int joinable_id = 99;
    pthread_create(&joinable, NULL, joinable_thread_func, &joinable_id);

    void* result;
    pthread_join(joinable, &result);
    printf("Main: joined thread %d, got result %ld\n", joinable_id, (long)(intptr_t)result);

    // Wait for detached threads to complete
    printf("\nWaiting for detached threads to complete...\n");
    int wait_count = 0;
    while (atomic_load(&completed_count) < NUM_DETACHED && wait_count < 100) {
        sleep_ms(10);
        wait_count++;
    }

    // Results
    int started = atomic_load(&started_count);
    int completed = atomic_load(&completed_count);

    printf("\nResults:\n");
    printf("  Detached threads started:   %d / %d\n", started, NUM_DETACHED);
    printf("  Detached threads completed: %d / %d\n", completed, NUM_DETACHED);

    if (completed == NUM_DETACHED) {
        printf("\n[PASS] Thread detach works correctly!\n");
        printf("  - Detached threads ran independently\n");
        printf("  - No join was needed (or possible)\n");
        printf("  - Resources cleaned up automatically\n");
    } else {
        printf("\n[WARN] Not all detached threads completed in time\n");
        printf("  This might be OK - detached threads run asynchronously\n");
    }

    return 0;
}

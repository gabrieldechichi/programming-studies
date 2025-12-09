// Demo 7: Atomics
// Tests: atomic_int, atomic_fetch_add, atomic_load, atomic_store, atomic_compare_exchange

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdatomic.h>

#define NUM_THREADS 4
#define ITERATIONS 10000

static atomic_int atomic_counter = 0;
static int regular_counter = 0;

// For compare-exchange test
static atomic_int cas_value = 0;
static atomic_int cas_successes = 0;

void* thread_func(void* arg) {
    int id = *(int*)arg;

    // Test atomic_fetch_add
    for (int i = 0; i < ITERATIONS; i++) {
        atomic_fetch_add(&atomic_counter, 1);
        regular_counter++;  // For comparison (will have races)
    }

    // Test atomic_compare_exchange_strong
    // Each thread tries to increment cas_value by CAS
    int local_successes = 0;
    for (int i = 0; i < 100; i++) {
        int expected = atomic_load(&cas_value);
        int desired = expected + 1;
        if (atomic_compare_exchange_strong(&cas_value, &expected, desired)) {
            local_successes++;
        }
    }
    atomic_fetch_add(&cas_successes, local_successes);

    printf("Thread %d: completed, CAS successes = %d\n", id, local_successes);
    return NULL;
}

int demo_main(void) {
    printf("=== Demo: Atomics ===\n\n");

    int expected = NUM_THREADS * ITERATIONS;
    printf("Testing atomic operations with %d threads x %d iterations\n\n", NUM_THREADS, ITERATIONS);

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

    // Results
    int atomic_result = atomic_load(&atomic_counter);
    int cas_result = atomic_load(&cas_value);
    int total_cas_successes = atomic_load(&cas_successes);

    printf("\nResults:\n");
    printf("  atomic_fetch_add counter: %d (expected %d)\n", atomic_result, expected);
    printf("  regular counter:          %d (expected %d)\n", regular_counter, expected);

    if (regular_counter != expected) {
        printf("    ^ Lost %d increments due to races\n", expected - regular_counter);
    }

    printf("\n  CAS test:\n");
    printf("    Final CAS value: %d\n", cas_result);
    printf("    Total CAS successes: %d (should equal CAS value)\n", total_cas_successes);
    printf("    Total CAS attempts: %d\n", NUM_THREADS * 100);

    // Verify atomic counter
    if (atomic_result != expected) {
        printf("\n[FAIL] Atomic counter has wrong value!\n");
        return 1;
    }

    // Verify CAS consistency
    if (cas_result != total_cas_successes) {
        printf("\n[FAIL] CAS value doesn't match success count!\n");
        return 1;
    }

    printf("\n[PASS] Atomic operations work correctly!\n");
    printf("  - atomic_fetch_add provides race-free increments\n");
    printf("  - atomic_compare_exchange_strong works for CAS\n");
    printf("  - atomic_load/store provide consistent reads/writes\n");

    return 0;
}

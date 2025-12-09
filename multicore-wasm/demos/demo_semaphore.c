// Demo 8: Semaphore
// Tests: counting semaphore for resource limiting

#include "os/os.h"

#define NUM_THREADS 8
#define MAX_CONCURRENT 3  // Only 3 threads can be in critical section at once
#define WORK_ITERATIONS 5

static Semaphore sem;
static Mutex print_mutex;
static i32 current_in_section = 0;
static i32 max_observed = 0;

void thread_func(void* arg) {
    i32 id = *(i32*)arg;

    for (i32 i = 0; i < WORK_ITERATIONS; i++) {
        // Wait on semaphore (decrement)
        semaphore_take(sem);

        // Enter critical section
        mutex_take(print_mutex);
        current_in_section++;
        if (current_in_section > max_observed) {
            max_observed = current_in_section;
        }
        LOG_INFO("Thread %: ENTER (iteration %, % threads in section)",
                 FMT_INT(id), FMT_INT(i), FMT_INT(current_in_section));
        mutex_drop(print_mutex);

        // Simulate work in critical section
        for (volatile i32 j = 0; j < 10000; j++) {}

        // Leave critical section
        mutex_take(print_mutex);
        LOG_INFO("Thread %: LEAVE (iteration %, % threads in section)",
                 FMT_INT(id), FMT_INT(i), FMT_INT(current_in_section));
        current_in_section--;
        mutex_drop(print_mutex);

        // Signal semaphore (increment)
        semaphore_drop(sem);
    }
}

int demo_main(void) {
    LOG_INFO("=== Demo: Semaphore ===");

    LOG_INFO("Testing counting semaphore:");
    LOG_INFO("  % threads competing", FMT_INT(NUM_THREADS));
    LOG_INFO("  Max % allowed in critical section", FMT_INT(MAX_CONCURRENT));

    // Initialize semaphore with count = MAX_CONCURRENT
    sem = semaphore_alloc(MAX_CONCURRENT);
    print_mutex = mutex_alloc();

    Thread threads[NUM_THREADS];
    i32 thread_ids[NUM_THREADS];

    // Create threads
    for (i32 i = 0; i < NUM_THREADS; i++) {
        thread_ids[i] = i;
        threads[i] = thread_launch(thread_func, &thread_ids[i]);
    }

    // Join threads
    for (i32 i = 0; i < NUM_THREADS; i++) {
        thread_join(threads[i], 0);
    }

    // Verify
    LOG_INFO("Results:");
    LOG_INFO("  Max threads observed in section: % (limit was %)",
             FMT_INT(max_observed), FMT_INT(MAX_CONCURRENT));
    LOG_INFO("  Current in section: % (should be 0)", FMT_INT(current_in_section));

    if (max_observed > MAX_CONCURRENT) {
        LOG_ERROR("[FAIL] Semaphore allowed too many threads!");
        return 1;
    }

    if (current_in_section != 0) {
        LOG_ERROR("[FAIL] Not all threads exited cleanly!");
        return 1;
    }

    LOG_INFO("[PASS] Semaphore correctly limits concurrency!");
    LOG_INFO("  - Never exceeded % concurrent threads", FMT_INT(MAX_CONCURRENT));
    LOG_INFO("  - All threads completed successfully");

    // Cleanup
    semaphore_release(sem);
    mutex_release(print_mutex);

    return 0;
}

// Demo 2: Thread Local Storage
// Tests: thread_local / _Thread_local variables - each thread has its own copy

#include "os/os.h"

#define NUM_THREADS 4
#define ITERATIONS 1000

// Thread-local variable - each thread gets its own copy
__thread i32 tls_counter = 0;
__thread i32 tls_thread_id = -1;

typedef struct {
    i32 id;
    b32 failed;
} TlsThreadArg;

void thread_func(void* arg) {
    TlsThreadArg* targ = (TlsThreadArg*)arg;
    i32 id = targ->id;
    tls_thread_id = id;

    LOG_INFO("Thread %: TLS counter initial value = %", FMT_INT(id), FMT_INT(tls_counter));

    // Increment thread-local counter
    for (i32 i = 0; i < ITERATIONS; i++) {
        tls_counter++;
    }

    LOG_INFO("Thread %: TLS counter final value = % (expected %)",
             FMT_INT(id), FMT_INT(tls_counter), FMT_INT(ITERATIONS));

    // Verify TLS isolation
    if (tls_counter != ITERATIONS) {
        LOG_ERROR("Thread %: [FAIL] TLS counter corrupted!", FMT_INT(id));
        targ->failed = true;
        return;
    }

    if (tls_thread_id != id) {
        LOG_ERROR("Thread %: [FAIL] TLS thread_id corrupted! Got %", FMT_INT(id), FMT_INT(tls_thread_id));
        targ->failed = true;
        return;
    }

    targ->failed = false;
}

int demo_main(void) {
    LOG_INFO("=== Demo: Thread Local Storage ===");

    Thread threads[NUM_THREADS];
    TlsThreadArg thread_args[NUM_THREADS];

    // Verify main thread TLS
    tls_counter = 999;
    tls_thread_id = -999;
    LOG_INFO("Main: Set TLS counter to %, thread_id to %", FMT_INT(tls_counter), FMT_INT(tls_thread_id));

    // Create threads
    LOG_INFO("Creating % threads...", FMT_INT(NUM_THREADS));
    for (i32 i = 0; i < NUM_THREADS; i++) {
        thread_args[i].id = i;
        thread_args[i].failed = false;
        threads[i] = thread_launch(thread_func, &thread_args[i]);
    }

    // Join threads
    i32 failures = 0;
    for (i32 i = 0; i < NUM_THREADS; i++) {
        thread_join(threads[i], 0);
        if (thread_args[i].failed) {
            failures++;
        }
    }

    // Verify main thread TLS wasn't affected
    LOG_INFO("Main: TLS counter = % (expected 999)", FMT_INT(tls_counter));
    LOG_INFO("Main: TLS thread_id = % (expected -999)", FMT_INT(tls_thread_id));

    if (tls_counter != 999 || tls_thread_id != -999) {
        LOG_ERROR("[FAIL] Main thread TLS was corrupted by worker threads!");
        return 1;
    }

    if (failures == 0) {
        LOG_INFO("[PASS] Thread Local Storage works correctly!");
        LOG_INFO("  - Each thread had isolated TLS variables");
        LOG_INFO("  - Main thread TLS was not affected");
    } else {
        LOG_ERROR("[FAIL] % threads had TLS issues!", FMT_INT(failures));
        return 1;
    }

    return 0;
}

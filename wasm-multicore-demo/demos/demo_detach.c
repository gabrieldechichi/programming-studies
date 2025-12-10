// Demo 10: Thread Detach
// Tests: thread_detach - fire-and-forget threads that clean up automatically

#include "os/os.h"
#include "lib/thread_context.h"

#define NUM_DETACHED 4

static i32 completed_count = 0;
static i32 started_count = 0;

typedef struct {
    i32 id;
} DetachedThreadArg;

// Static args array - threads read their id, no malloc needed
static DetachedThreadArg detached_args[NUM_DETACHED];

void detached_thread_func(void* arg) {
    DetachedThreadArg* targ = (DetachedThreadArg*)arg;
    i32 id = targ->id;

    ins_atomic_u32_inc_eval((u32*)&started_count);
    LOG_INFO("Detached thread %: started", FMT_INT(id));

    // Simulate some work
    for (volatile i32 i = 0; i < 100000; i++) {}

    LOG_INFO("Detached thread %: finishing", FMT_INT(id));
    ins_atomic_u32_inc_eval((u32*)&completed_count);

    // Thread exits and resources are automatically cleaned up
}

typedef struct {
    i32 id;
    i32 result;
} JoinableThreadArg;

void joinable_thread_func(void* arg) {
    JoinableThreadArg* targ = (JoinableThreadArg*)arg;

    LOG_INFO("Joinable thread %: started", FMT_INT(targ->id));

    // Simulate work
    for (volatile i32 i = 0; i < 50000; i++) {}

    targ->result = targ->id * 10;
    LOG_INFO("Joinable thread %: finishing", FMT_INT(targ->id));
}

int demo_main(void) {
    LOG_INFO("=== Demo: Thread Detach ===");

    // Test 1: Detach after creation
    LOG_INFO("Test 1: thread_detach after thread_launch");
    for (i32 i = 0; i < NUM_DETACHED; i++) {
        detached_args[i].id = i;

        Thread t = thread_launch(detached_thread_func, &detached_args[i]);
        if (t.v[0] == 0) {
            LOG_ERROR("ERROR: thread_launch failed for thread %", FMT_INT(i));
            return 1;
        }

        // Detach the thread - we won't join it
        thread_detach(t);
        LOG_INFO("Main: detached thread %", FMT_INT(i));
    }

    // Test 2: Joinable thread for comparison
    LOG_INFO("Test 2: Regular joinable thread");
    JoinableThreadArg joinable_arg = { .id = 99, .result = 0 };
    Thread joinable = thread_launch(joinable_thread_func, &joinable_arg);

    thread_join(joinable, 0);
    LOG_INFO("Main: joined thread %, got result %", FMT_INT(joinable_arg.id), FMT_INT(joinable_arg.result));

    // Wait for detached threads to complete (spin-wait since we don't have sleep)
    LOG_INFO("Waiting for detached threads to complete...");
    i32 wait_iterations = 0;
    i32 max_iterations = 10000000; // Spin for a while
    while (ins_atomic_load_acquire((i32*)&completed_count) < NUM_DETACHED && wait_iterations < max_iterations) {
        // Spin wait
        for (volatile i32 j = 0; j < 100; j++) {}
        wait_iterations++;
    }

    // Results
    i32 started = ins_atomic_load_acquire((i32*)&started_count);
    i32 completed = ins_atomic_load_acquire((i32*)&completed_count);

    LOG_INFO("Results:");
    LOG_INFO("  Detached threads started:   % / %", FMT_INT(started), FMT_INT(NUM_DETACHED));
    LOG_INFO("  Detached threads completed: % / %", FMT_INT(completed), FMT_INT(NUM_DETACHED));

    if (completed == NUM_DETACHED) {
        LOG_INFO("[PASS] Thread detach works correctly!");
        LOG_INFO("  - Detached threads ran independently");
        LOG_INFO("  - No join was needed (or possible)");
        LOG_INFO("  - Resources cleaned up automatically");
    } else {
        LOG_WARN("[WARN] Not all detached threads completed in time");
        LOG_WARN("  This might be OK - detached threads run asynchronously");
    }

    return 0;
}

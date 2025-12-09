// Demo 1: Basic Thread Creation
// Tests: thread_launch, thread_join, passing arguments

#include "os/os.h"

#define NUM_THREADS 4

typedef struct {
    i32 thread_id;
    i32 input_value;
    i32 result;
} ThreadArg;

void thread_func(void* arg) {
    ThreadArg* targ = (ThreadArg*)arg;

    LOG_INFO("Thread %: started with input value %", FMT_INT(targ->thread_id), FMT_INT(targ->input_value));

    targ->result = targ->input_value * 2;

    LOG_INFO("Thread %: computed result %", FMT_INT(targ->thread_id), FMT_INT(targ->result));
}

int demo_main(void) {
    LOG_INFO("=== Demo: Thread Create/Join ===");

    Thread threads[NUM_THREADS];
    ThreadArg thread_args[NUM_THREADS];

    LOG_INFO("Creating % threads...", FMT_INT(NUM_THREADS));
    for (i32 i = 0; i < NUM_THREADS; i++) {
        thread_args[i].thread_id = i;
        thread_args[i].input_value = (i + 1) * 10;
        thread_args[i].result = 0;

        threads[i] = thread_launch(thread_func, &thread_args[i]);
    }

    LOG_INFO("Joining threads and collecting results...");
    i32 total = 0;
    for (i32 i = 0; i < NUM_THREADS; i++) {
        thread_join(threads[i], 0);

        LOG_INFO("Main: Thread % returned result %", FMT_INT(thread_args[i].thread_id), FMT_INT(thread_args[i].result));
        total += thread_args[i].result;
    }

    LOG_INFO("Total sum of all results: %", FMT_INT(total));
    LOG_INFO("Expected: %", FMT_INT((10 + 20 + 30 + 40) * 2));

    if (total == 200) {
        LOG_INFO("[PASS] Thread create/join works correctly!");
    } else {
        LOG_ERROR("[FAIL] Unexpected result!");
        return 1;
    }

    return 0;
}

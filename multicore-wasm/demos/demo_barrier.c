// Demo 5: Barrier
// Tests: Barrier - synchronize N threads at a rendezvous point

#include "os/os.h"

#define NUM_THREADS 4
#define NUM_PHASES 3

static Barrier barrier;
static i32 phase_results[NUM_PHASES][NUM_THREADS] = {0};

typedef struct {
    i32 id;
} BarrierThreadArg;

void thread_func(void* arg) {
    BarrierThreadArg* targ = (BarrierThreadArg*)arg;
    i32 id = targ->id;

    for (i32 phase = 0; phase < NUM_PHASES; phase++) {
        // Do some "work" for this phase
        phase_results[phase][id] = (phase + 1) * (id + 1);
        LOG_INFO("Thread %: completed phase % (result=%)",
                 FMT_INT(id), FMT_INT(phase), FMT_INT(phase_results[phase][id]));

        // Wait for all threads to complete this phase
        barrier_wait(barrier);

        // Log after barrier (only first thread to show phase complete)
        if (id == 0) {
            LOG_INFO("--- All threads reached barrier (phase % complete) ---", FMT_INT(phase));
        }
        barrier_wait(barrier);  // Sync again so message prints before next phase starts
    }
}

int demo_main(void) {
    LOG_INFO("=== Demo: Barrier ===");

    LOG_INFO("% threads will synchronize at barriers through % phases",
             FMT_INT(NUM_THREADS), FMT_INT(NUM_PHASES));

    // Initialize barrier for NUM_THREADS
    barrier = barrier_alloc(NUM_THREADS);

    Thread threads[NUM_THREADS];
    BarrierThreadArg thread_args[NUM_THREADS];

    // Create threads
    for (i32 i = 0; i < NUM_THREADS; i++) {
        thread_args[i].id = i;
        threads[i] = thread_launch(thread_func, &thread_args[i]);
    }

    // Join threads
    for (i32 i = 0; i < NUM_THREADS; i++) {
        thread_join(threads[i], 0);
    }

    // Verify all phase results
    LOG_INFO("Verifying phase results...");
    i32 errors = 0;
    for (i32 phase = 0; phase < NUM_PHASES; phase++) {
        for (i32 t = 0; t < NUM_THREADS; t++) {
            i32 expected = (phase + 1) * (t + 1);
            i32 got = phase_results[phase][t];
            LOG_INFO("Phase % [%]=% (expected %)", FMT_INT(phase), FMT_INT(t), FMT_INT(got), FMT_INT(expected));
            if (got != expected) {
                errors++;
            }
        }
    }

    if (errors == 0) {
        LOG_INFO("[PASS] Barrier synchronization works correctly!");
        LOG_INFO("  - All threads waited for each other at each phase");
        LOG_INFO("  - Results from all phases are correct");
    } else {
        LOG_ERROR("[FAIL] % errors in phase results!", FMT_INT(errors));
        return 1;
    }

    // Cleanup
    barrier_release(barrier);

    return 0;
}

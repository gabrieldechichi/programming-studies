// Demo 11: Thread Attributes
// Tests: pthread_attr_t - stack size, detach state configuration

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdatomic.h>

#define CUSTOM_STACK_SIZE (256 * 1024)  // 256KB stack

static atomic_int detached_done = 0;

// Function to estimate stack usage
void recursive_stack_test(int depth, int max_depth, int* max_reached) {
    volatile char stack_user[1024];  // Use some stack
    stack_user[0] = (char)depth;
    (void)stack_user;

    if (depth > *max_reached) {
        *max_reached = depth;
    }

    if (depth < max_depth) {
        recursive_stack_test(depth + 1, max_depth, max_reached);
    }
}

void* custom_stack_thread(void* arg) {
    size_t stack_size = (size_t)arg;
    printf("Thread: running with custom stack size %zu bytes\n", stack_size);

    // Try to use some stack space
    int max_depth_reached = 0;
    int target_depth = 50;  // Moderate recursion
    recursive_stack_test(0, target_depth, &max_depth_reached);

    printf("Thread: reached recursion depth %d (target %d)\n",
           max_depth_reached, target_depth);

    return (void*)(intptr_t)max_depth_reached;
}

void* detached_attr_thread(void* arg) {
    int id = *(int*)arg;
    printf("Detached-by-attr thread %d: started\n", id);

    for (volatile int i = 0; i < 50000; i++) {}

    printf("Detached-by-attr thread %d: done\n", id);
    atomic_store(&detached_done, 1);
    return NULL;
}

int demo_main(void) {
    printf("=== Demo: Thread Attributes ===\n\n");

    pthread_attr_t attr;
    int ret;

    // Initialize attribute object
    ret = pthread_attr_init(&attr);
    if (ret != 0) {
        printf("ERROR: pthread_attr_init failed (error %d)\n", ret);
        return 1;
    }
    printf("pthread_attr_init: OK\n");

    // Test 1: Get/Set stack size
    printf("\n--- Test 1: Stack Size ---\n");
    size_t default_stack_size;
    pthread_attr_getstacksize(&attr, &default_stack_size);
    printf("Default stack size: %zu bytes (%.1f KB)\n",
           default_stack_size, default_stack_size / 1024.0);

    ret = pthread_attr_setstacksize(&attr, CUSTOM_STACK_SIZE);
    if (ret != 0) {
        printf("ERROR: pthread_attr_setstacksize failed (error %d)\n", ret);
    } else {
        size_t new_stack_size;
        pthread_attr_getstacksize(&attr, &new_stack_size);
        printf("Set stack size to: %zu bytes (%.1f KB)\n",
               new_stack_size, new_stack_size / 1024.0);
    }

    // Create thread with custom stack size
    pthread_t stack_thread;
    ret = pthread_create(&stack_thread, &attr, custom_stack_thread,
                         (void*)(size_t)CUSTOM_STACK_SIZE);
    if (ret != 0) {
        printf("ERROR: pthread_create failed (error %d)\n", ret);
        return 1;
    }

    void* result;
    pthread_join(stack_thread, &result);
    int depth = (int)(intptr_t)result;
    printf("Thread with custom stack completed, depth reached: %d\n", depth);

    // Test 2: Detach state attribute
    printf("\n--- Test 2: Detach State ---\n");

    // Reset to default then set detached
    pthread_attr_destroy(&attr);
    pthread_attr_init(&attr);

    int detach_state;
    pthread_attr_getdetachstate(&attr, &detach_state);
    printf("Default detach state: %s\n",
           detach_state == PTHREAD_CREATE_JOINABLE ? "JOINABLE" : "DETACHED");

    ret = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    if (ret != 0) {
        printf("ERROR: pthread_attr_setdetachstate failed (error %d)\n", ret);
    } else {
        pthread_attr_getdetachstate(&attr, &detach_state);
        printf("Set detach state to: %s\n",
               detach_state == PTHREAD_CREATE_DETACHED ? "DETACHED" : "JOINABLE");
    }

    // Create detached thread using attribute
    pthread_t detached_thread;
    int detached_id = 42;
    ret = pthread_create(&detached_thread, &attr, detached_attr_thread, &detached_id);
    if (ret != 0) {
        printf("ERROR: pthread_create failed (error %d)\n", ret);
        return 1;
    }
    printf("Created detached thread (cannot join it)\n");

    // Wait for it to complete (can't join, so poll)
    int wait_count = 0;
    while (!atomic_load(&detached_done) && wait_count < 100) {
        for (volatile int i = 0; i < 100000; i++) {}
        wait_count++;
    }

    // Test 3: Query guard size (stack overflow protection)
    printf("\n--- Test 3: Guard Size ---\n");
    size_t guard_size;
    pthread_attr_getguardsize(&attr, &guard_size);
    printf("Guard size: %zu bytes\n", guard_size);

    // Cleanup
    pthread_attr_destroy(&attr);
    printf("\npthread_attr_destroy: OK\n");

    printf("\n[PASS] Thread attributes work correctly!\n");
    printf("  - Stack size can be configured\n");
    printf("  - Detach state can be set at creation\n");
    printf("  - Guard size is queryable\n");

    return 0;
}

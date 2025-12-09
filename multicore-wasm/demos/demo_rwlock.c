// Demo 9: Read-Write Lock
// Tests: pthread_rwlock_t - multiple readers OR single writer

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define NUM_READERS 4
#define NUM_WRITERS 2
#define READ_ITERATIONS 10
#define WRITE_ITERATIONS 5

static pthread_rwlock_t rwlock = PTHREAD_RWLOCK_INITIALIZER;
static pthread_mutex_t stats_mutex = PTHREAD_MUTEX_INITIALIZER;

static int shared_data = 0;
static int active_readers = 0;
static int active_writers = 0;
static int max_concurrent_readers = 0;
static int reader_while_writer = 0;  // Should stay 0
static int writer_while_reader = 0;  // Should stay 0
static int writer_while_writer = 0;  // Should stay 0

void* reader_func(void* arg) {
    int id = *(int*)arg;

    for (int i = 0; i < READ_ITERATIONS; i++) {
        pthread_rwlock_rdlock(&rwlock);

        // Track stats
        pthread_mutex_lock(&stats_mutex);
        active_readers++;
        if (active_readers > max_concurrent_readers) {
            max_concurrent_readers = active_readers;
        }
        if (active_writers > 0) {
            reader_while_writer++;
        }
        int readers = active_readers;
        int writers = active_writers;
        pthread_mutex_unlock(&stats_mutex);

        // Read shared data
        int value = shared_data;
        printf("Reader %d: read value %d (readers=%d, writers=%d)\n",
               id, value, readers, writers);

        // Simulate read work
        for (volatile int j = 0; j < 1000; j++) {}

        pthread_mutex_lock(&stats_mutex);
        active_readers--;
        pthread_mutex_unlock(&stats_mutex);

        pthread_rwlock_unlock(&rwlock);
    }

    return NULL;
}

void* writer_func(void* arg) {
    int id = *(int*)arg;

    for (int i = 0; i < WRITE_ITERATIONS; i++) {
        pthread_rwlock_wrlock(&rwlock);

        // Track stats
        pthread_mutex_lock(&stats_mutex);
        active_writers++;
        if (active_readers > 0) {
            writer_while_reader++;
        }
        if (active_writers > 1) {
            writer_while_writer++;
        }
        int readers = active_readers;
        int writers = active_writers;
        pthread_mutex_unlock(&stats_mutex);

        // Write shared data
        shared_data++;
        printf("Writer %d: wrote value %d (readers=%d, writers=%d)\n",
               id, shared_data, readers, writers);

        // Simulate write work
        for (volatile int j = 0; j < 2000; j++) {}

        pthread_mutex_lock(&stats_mutex);
        active_writers--;
        pthread_mutex_unlock(&stats_mutex);

        pthread_rwlock_unlock(&rwlock);
    }

    return NULL;
}

int demo_main(void) {
    printf("=== Demo: Read-Write Lock ===\n\n");

    printf("Testing rwlock with:\n");
    printf("  %d readers x %d iterations\n", NUM_READERS, READ_ITERATIONS);
    printf("  %d writers x %d iterations\n\n", NUM_WRITERS, WRITE_ITERATIONS);

    pthread_t readers[NUM_READERS];
    pthread_t writers[NUM_WRITERS];
    int reader_ids[NUM_READERS];
    int writer_ids[NUM_WRITERS];

    // Create readers
    for (int i = 0; i < NUM_READERS; i++) {
        reader_ids[i] = i;
        pthread_create(&readers[i], NULL, reader_func, &reader_ids[i]);
    }

    // Create writers
    for (int i = 0; i < NUM_WRITERS; i++) {
        writer_ids[i] = i;
        pthread_create(&writers[i], NULL, writer_func, &writer_ids[i]);
    }

    // Join all
    for (int i = 0; i < NUM_READERS; i++) {
        pthread_join(readers[i], NULL);
    }
    for (int i = 0; i < NUM_WRITERS; i++) {
        pthread_join(writers[i], NULL);
    }

    // Results
    int expected_writes = NUM_WRITERS * WRITE_ITERATIONS;
    printf("\nResults:\n");
    printf("  Final shared_data: %d (expected %d)\n", shared_data, expected_writes);
    printf("  Max concurrent readers: %d\n", max_concurrent_readers);
    printf("  Violations:\n");
    printf("    Reader while writer active: %d (should be 0)\n", reader_while_writer);
    printf("    Writer while reader active: %d (should be 0)\n", writer_while_reader);
    printf("    Writer while writer active: %d (should be 0)\n", writer_while_writer);

    int pass = 1;
    if (shared_data != expected_writes) {
        printf("\n[FAIL] Wrong final value!\n");
        pass = 0;
    }
    if (reader_while_writer > 0 || writer_while_reader > 0 || writer_while_writer > 0) {
        printf("\n[FAIL] Lock violations detected!\n");
        pass = 0;
    }

    if (pass) {
        printf("\n[PASS] Read-write lock works correctly!\n");
        printf("  - Multiple readers can read concurrently (max observed: %d)\n",
               max_concurrent_readers);
        printf("  - Writers have exclusive access\n");
        printf("  - No reader/writer conflicts\n");
    }

    // Cleanup
    pthread_rwlock_destroy(&rwlock);
    pthread_mutex_destroy(&stats_mutex);

    return pass ? 0 : 1;
}

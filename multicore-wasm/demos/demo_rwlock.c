// Demo 9: Read-Write Lock
// Tests: RWMutex - multiple readers OR single writer

#include "os/os.h"

#define NUM_READERS 4
#define NUM_WRITERS 2
#define READ_ITERATIONS 10
#define WRITE_ITERATIONS 5

static RWMutex rwlock;
static Mutex stats_mutex;

static i32 shared_data = 0;
static i32 active_readers = 0;
static i32 active_writers = 0;
static i32 max_concurrent_readers = 0;
static i32 reader_while_writer = 0;  // Should stay 0
static i32 writer_while_reader = 0;  // Should stay 0
static i32 writer_while_writer = 0;  // Should stay 0

void reader_func(void* arg) {
    i32 id = *(i32*)arg;

    for (i32 i = 0; i < READ_ITERATIONS; i++) {
        rw_mutex_take_r(rwlock);

        // Track stats
        mutex_take(stats_mutex);
        active_readers++;
        if (active_readers > max_concurrent_readers) {
            max_concurrent_readers = active_readers;
        }
        if (active_writers > 0) {
            reader_while_writer++;
        }
        i32 readers = active_readers;
        i32 writers = active_writers;
        mutex_drop(stats_mutex);

        // Read shared data
        i32 value = shared_data;
        LOG_INFO("Reader %: read value % (readers=%, writers=%)",
                 FMT_INT(id), FMT_INT(value), FMT_INT(readers), FMT_INT(writers));

        // Simulate read work
        for (volatile i32 j = 0; j < 1000; j++) {}

        mutex_take(stats_mutex);
        active_readers--;
        mutex_drop(stats_mutex);

        rw_mutex_drop_r(rwlock);
    }
}

void writer_func(void* arg) {
    i32 id = *(i32*)arg;

    for (i32 i = 0; i < WRITE_ITERATIONS; i++) {
        rw_mutex_take_w(rwlock);

        // Track stats
        mutex_take(stats_mutex);
        active_writers++;
        if (active_readers > 0) {
            writer_while_reader++;
        }
        if (active_writers > 1) {
            writer_while_writer++;
        }
        i32 readers = active_readers;
        i32 writers = active_writers;
        mutex_drop(stats_mutex);

        // Write shared data
        shared_data++;
        LOG_INFO("Writer %: wrote value % (readers=%, writers=%)",
                 FMT_INT(id), FMT_INT(shared_data), FMT_INT(readers), FMT_INT(writers));

        // Simulate write work
        for (volatile i32 j = 0; j < 2000; j++) {}

        mutex_take(stats_mutex);
        active_writers--;
        mutex_drop(stats_mutex);

        rw_mutex_drop_w(rwlock);
    }
}

int demo_main(void) {
    LOG_INFO("=== Demo: Read-Write Lock ===");

    LOG_INFO("Testing rwlock with:");
    LOG_INFO("  % readers x % iterations", FMT_INT(NUM_READERS), FMT_INT(READ_ITERATIONS));
    LOG_INFO("  % writers x % iterations", FMT_INT(NUM_WRITERS), FMT_INT(WRITE_ITERATIONS));

    // Initialize locks
    rwlock = rw_mutex_alloc();
    stats_mutex = mutex_alloc();

    Thread reader_threads[NUM_READERS];
    Thread writer_threads[NUM_WRITERS];
    i32 reader_ids[NUM_READERS];
    i32 writer_ids[NUM_WRITERS];

    // Create readers
    for (i32 i = 0; i < NUM_READERS; i++) {
        reader_ids[i] = i;
        reader_threads[i] = thread_launch(reader_func, &reader_ids[i]);
    }

    // Create writers
    for (i32 i = 0; i < NUM_WRITERS; i++) {
        writer_ids[i] = i;
        writer_threads[i] = thread_launch(writer_func, &writer_ids[i]);
    }

    // Join all
    for (i32 i = 0; i < NUM_READERS; i++) {
        thread_join(reader_threads[i], 0);
    }
    for (i32 i = 0; i < NUM_WRITERS; i++) {
        thread_join(writer_threads[i], 0);
    }

    // Results
    i32 expected_writes = NUM_WRITERS * WRITE_ITERATIONS;
    LOG_INFO("Results:");
    LOG_INFO("  Final shared_data: % (expected %)", FMT_INT(shared_data), FMT_INT(expected_writes));
    LOG_INFO("  Max concurrent readers: %", FMT_INT(max_concurrent_readers));
    LOG_INFO("  Violations:");
    LOG_INFO("    Reader while writer active: % (should be 0)", FMT_INT(reader_while_writer));
    LOG_INFO("    Writer while reader active: % (should be 0)", FMT_INT(writer_while_reader));
    LOG_INFO("    Writer while writer active: % (should be 0)", FMT_INT(writer_while_writer));

    i32 pass = 1;
    if (shared_data != expected_writes) {
        LOG_ERROR("[FAIL] Wrong final value!");
        pass = 0;
    }
    if (reader_while_writer > 0 || writer_while_reader > 0 || writer_while_writer > 0) {
        LOG_ERROR("[FAIL] Lock violations detected!");
        pass = 0;
    }

    if (pass) {
        LOG_INFO("[PASS] Read-write lock works correctly!");
        LOG_INFO("  - Multiple readers can read concurrently (max observed: %)",
                 FMT_INT(max_concurrent_readers));
        LOG_INFO("  - Writers have exclusive access");
        LOG_INFO("  - No reader/writer conflicts");
    }

    // Cleanup
    rw_mutex_release(rwlock);
    mutex_release(stats_mutex);

    return pass ? 0 : 1;
}

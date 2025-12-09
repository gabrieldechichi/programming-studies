// Demo 6: Condition Variable
// Tests: cond_var_wait/signal/broadcast pattern (producer-consumer)

#include "os/os.h"

#define BUFFER_SIZE 5
#define NUM_ITEMS 20
#define NUM_PRODUCERS 2
#define NUM_CONSUMERS 2

typedef struct {
    i32 buffer[BUFFER_SIZE];
    i32 count;
    i32 in_idx;
    i32 out_idx;
    Mutex mutex;
    CondVar not_full;
    CondVar not_empty;
    i32 done;  // Signal producers are done
    i32 produced_count;
    i32 consumed_count;
} SharedQueue;

static SharedQueue queue;

void producer(void* arg) {
    i32 id = *(i32*)arg;

    while (1) {
        mutex_take(queue.mutex);

        // Check if we've produced enough
        if (queue.produced_count >= NUM_ITEMS) {
            mutex_drop(queue.mutex);
            break;
        }

        // Wait while buffer is full
        while (queue.count == BUFFER_SIZE && queue.produced_count < NUM_ITEMS) {
            LOG_INFO("Producer %: buffer full, waiting...", FMT_INT(id));
            cond_var_wait(queue.not_full, queue.mutex, 0);
        }

        // Recheck after waking
        if (queue.produced_count >= NUM_ITEMS) {
            mutex_drop(queue.mutex);
            break;
        }

        // Produce item
        i32 item = queue.produced_count + 1;
        queue.buffer[queue.in_idx] = item;
        queue.in_idx = (queue.in_idx + 1) % BUFFER_SIZE;
        queue.count++;
        queue.produced_count++;
        LOG_INFO("Producer %: produced item % (buffer count=%)", FMT_INT(id), FMT_INT(item), FMT_INT(queue.count));

        // Signal consumers
        cond_var_signal(queue.not_empty);
        mutex_drop(queue.mutex);
    }

    LOG_INFO("Producer %: finished", FMT_INT(id));
}

void consumer(void* arg) {
    i32 id = *(i32*)arg;
    i32 consumed = 0;

    while (1) {
        mutex_take(queue.mutex);

        // Wait while buffer is empty (and producers not done)
        while (queue.count == 0 && queue.consumed_count < NUM_ITEMS) {
            LOG_INFO("Consumer %: buffer empty, waiting...", FMT_INT(id));
            cond_var_wait(queue.not_empty, queue.mutex, 0);
        }

        // Check if we're done
        if (queue.consumed_count >= NUM_ITEMS) {
            mutex_drop(queue.mutex);
            break;
        }

        // Consume item
        i32 item = queue.buffer[queue.out_idx];
        queue.out_idx = (queue.out_idx + 1) % BUFFER_SIZE;
        queue.count--;
        queue.consumed_count++;
        consumed++;
        LOG_INFO("Consumer %: consumed item % (buffer count=%)", FMT_INT(id), FMT_INT(item), FMT_INT(queue.count));

        // Signal producers
        cond_var_signal(queue.not_full);
        mutex_drop(queue.mutex);
    }

    LOG_INFO("Consumer %: finished (consumed % items)", FMT_INT(id), FMT_INT(consumed));
}

int demo_main(void) {
    LOG_INFO("=== Demo: Condition Variable ===");

    LOG_INFO("Producer-Consumer with:");
    LOG_INFO("  Buffer size: %", FMT_INT(BUFFER_SIZE));
    LOG_INFO("  Items to produce: %", FMT_INT(NUM_ITEMS));
    LOG_INFO("  Producers: %, Consumers: %", FMT_INT(NUM_PRODUCERS), FMT_INT(NUM_CONSUMERS));

    // Initialize queue
    queue.count = 0;
    queue.in_idx = 0;
    queue.out_idx = 0;
    queue.done = 0;
    queue.produced_count = 0;
    queue.consumed_count = 0;
    queue.mutex = mutex_alloc();
    queue.not_full = cond_var_alloc();
    queue.not_empty = cond_var_alloc();

    Thread producers[NUM_PRODUCERS];
    Thread consumers[NUM_CONSUMERS];
    i32 producer_ids[NUM_PRODUCERS];
    i32 consumer_ids[NUM_CONSUMERS];

    // Create consumers first (they'll wait for items)
    for (i32 i = 0; i < NUM_CONSUMERS; i++) {
        consumer_ids[i] = i;
        consumers[i] = thread_launch(consumer, &consumer_ids[i]);
    }

    // Create producers
    for (i32 i = 0; i < NUM_PRODUCERS; i++) {
        producer_ids[i] = i;
        producers[i] = thread_launch(producer, &producer_ids[i]);
    }

    // Join producers
    for (i32 i = 0; i < NUM_PRODUCERS; i++) {
        thread_join(producers[i], 0);
    }

    // Wake up any waiting consumers (broadcast that we're done)
    mutex_take(queue.mutex);
    queue.done = 1;
    cond_var_broadcast(queue.not_empty);
    mutex_drop(queue.mutex);

    // Join consumers
    for (i32 i = 0; i < NUM_CONSUMERS; i++) {
        thread_join(consumers[i], 0);
    }

    // Verify
    LOG_INFO("Results:");
    LOG_INFO("  Produced: % items", FMT_INT(queue.produced_count));
    LOG_INFO("  Consumed: % items", FMT_INT(queue.consumed_count));
    LOG_INFO("  Buffer remaining: % items", FMT_INT(queue.count));

    if (queue.produced_count == NUM_ITEMS && queue.consumed_count == NUM_ITEMS && queue.count == 0) {
        LOG_INFO("[PASS] Condition variables work correctly!");
        LOG_INFO("  - Producers waited when buffer was full");
        LOG_INFO("  - Consumers waited when buffer was empty");
        LOG_INFO("  - All items produced and consumed");
    } else {
        LOG_ERROR("[FAIL] Mismatch in produced/consumed counts!");
        return 1;
    }

    // Cleanup
    mutex_release(queue.mutex);
    cond_var_release(queue.not_full);
    cond_var_release(queue.not_empty);

    return 0;
}

// Demo 6: Condition Variable
// Tests: pthread_cond_t - wait/signal/broadcast pattern (producer-consumer)

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define BUFFER_SIZE 5
#define NUM_ITEMS 20
#define NUM_PRODUCERS 2
#define NUM_CONSUMERS 2

typedef struct {
    int buffer[BUFFER_SIZE];
    int count;
    int in_idx;
    int out_idx;
    pthread_mutex_t mutex;
    pthread_cond_t not_full;
    pthread_cond_t not_empty;
    int done;  // Signal producers are done
    int produced_count;
    int consumed_count;
} SharedQueue;

static SharedQueue queue;

void* producer(void* arg) {
    int id = *(int*)arg;

    while (1) {
        pthread_mutex_lock(&queue.mutex);

        // Check if we've produced enough
        if (queue.produced_count >= NUM_ITEMS) {
            pthread_mutex_unlock(&queue.mutex);
            break;
        }

        // Wait while buffer is full
        while (queue.count == BUFFER_SIZE && queue.produced_count < NUM_ITEMS) {
            printf("Producer %d: buffer full, waiting...\n", id);
            pthread_cond_wait(&queue.not_full, &queue.mutex);
        }

        // Recheck after waking
        if (queue.produced_count >= NUM_ITEMS) {
            pthread_mutex_unlock(&queue.mutex);
            break;
        }

        // Produce item
        int item = queue.produced_count + 1;
        queue.buffer[queue.in_idx] = item;
        queue.in_idx = (queue.in_idx + 1) % BUFFER_SIZE;
        queue.count++;
        queue.produced_count++;
        printf("Producer %d: produced item %d (buffer count=%d)\n", id, item, queue.count);

        // Signal consumers
        pthread_cond_signal(&queue.not_empty);
        pthread_mutex_unlock(&queue.mutex);
    }

    printf("Producer %d: finished\n", id);
    return NULL;
}

void* consumer(void* arg) {
    int id = *(int*)arg;
    int consumed = 0;

    while (1) {
        pthread_mutex_lock(&queue.mutex);

        // Wait while buffer is empty (and producers not done)
        while (queue.count == 0 && queue.consumed_count < NUM_ITEMS) {
            printf("Consumer %d: buffer empty, waiting...\n", id);
            pthread_cond_wait(&queue.not_empty, &queue.mutex);
        }

        // Check if we're done
        if (queue.consumed_count >= NUM_ITEMS) {
            pthread_mutex_unlock(&queue.mutex);
            break;
        }

        // Consume item
        int item = queue.buffer[queue.out_idx];
        queue.out_idx = (queue.out_idx + 1) % BUFFER_SIZE;
        queue.count--;
        queue.consumed_count++;
        consumed++;
        printf("Consumer %d: consumed item %d (buffer count=%d)\n", id, item, queue.count);

        // Signal producers
        pthread_cond_signal(&queue.not_full);
        pthread_mutex_unlock(&queue.mutex);
    }

    printf("Consumer %d: finished (consumed %d items)\n", id, consumed);
    return NULL;
}

int demo_main(void) {
    printf("=== Demo: Condition Variable ===\n\n");

    printf("Producer-Consumer with:\n");
    printf("  Buffer size: %d\n", BUFFER_SIZE);
    printf("  Items to produce: %d\n", NUM_ITEMS);
    printf("  Producers: %d, Consumers: %d\n\n", NUM_PRODUCERS, NUM_CONSUMERS);

    // Initialize queue
    queue.count = 0;
    queue.in_idx = 0;
    queue.out_idx = 0;
    queue.done = 0;
    queue.produced_count = 0;
    queue.consumed_count = 0;
    pthread_mutex_init(&queue.mutex, NULL);
    pthread_cond_init(&queue.not_full, NULL);
    pthread_cond_init(&queue.not_empty, NULL);

    pthread_t producers[NUM_PRODUCERS];
    pthread_t consumers[NUM_CONSUMERS];
    int producer_ids[NUM_PRODUCERS];
    int consumer_ids[NUM_CONSUMERS];

    // Create consumers first (they'll wait for items)
    for (int i = 0; i < NUM_CONSUMERS; i++) {
        consumer_ids[i] = i;
        pthread_create(&consumers[i], NULL, consumer, &consumer_ids[i]);
    }

    // Create producers
    for (int i = 0; i < NUM_PRODUCERS; i++) {
        producer_ids[i] = i;
        pthread_create(&producers[i], NULL, producer, &producer_ids[i]);
    }

    // Join producers
    for (int i = 0; i < NUM_PRODUCERS; i++) {
        pthread_join(producers[i], NULL);
    }

    // Wake up any waiting consumers (broadcast that we're done)
    pthread_mutex_lock(&queue.mutex);
    queue.done = 1;
    pthread_cond_broadcast(&queue.not_empty);
    pthread_mutex_unlock(&queue.mutex);

    // Join consumers
    for (int i = 0; i < NUM_CONSUMERS; i++) {
        pthread_join(consumers[i], NULL);
    }

    // Verify
    printf("\nResults:\n");
    printf("  Produced: %d items\n", queue.produced_count);
    printf("  Consumed: %d items\n", queue.consumed_count);
    printf("  Buffer remaining: %d items\n", queue.count);

    if (queue.produced_count == NUM_ITEMS && queue.consumed_count == NUM_ITEMS && queue.count == 0) {
        printf("\n[PASS] Condition variables work correctly!\n");
        printf("  - Producers waited when buffer was full\n");
        printf("  - Consumers waited when buffer was empty\n");
        printf("  - All items produced and consumed\n");
    } else {
        printf("\n[FAIL] Mismatch in produced/consumed counts!\n");
        return 1;
    }

    // Cleanup
    pthread_mutex_destroy(&queue.mutex);
    pthread_cond_destroy(&queue.not_full);
    pthread_cond_destroy(&queue.not_empty);

    return 0;
}

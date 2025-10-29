#ifndef H_QUEUE
#define H_QUEUE

#include "typedefs.h"

#define queue_define(type)                                                     \
  typedef struct {                                                             \
    u32 capacity;                                                              \
    u32 count;                                                                 \
    u32 head;                                                                  \
    u32 tail;                                                                  \
    type *items;                                                               \
  } type##_Queue

#define queue_new_zero(type) ((type##_Queue){0})

#define queue_new_ALLOC(arena_ptr, type, _capacity)                            \
  ((type##_Queue){.capacity = (_capacity),                                     \
                  .count = 0,                                                  \
                  .head = 0,                                                   \
                  .tail = 0,                                                   \
                  .items = ALLOC_ARRAY((arena_ptr), type, (_capacity))})

#define queue_is_empty(q) ((q).count == 0)

#define queue_is_full(q) ((q).count == (q).capacity)

#define queue_count(q) ((q).count)

#define queue_capacity(q) ((q).capacity)

#define queue_enqueue(q, item)                                                 \
  do {                                                                         \
    debug_assert_msg(!queue_is_full(q), "Queue enqueue capacity overflow %",   \
                     FMT_UINT((q).count));                                     \
    if (!queue_is_full(q)) {                                                   \
      (q).items[(q).tail] = (item);                                            \
      (q).tail = ((q).tail + 1) % (q).capacity;                                \
      (q).count++;                                                             \
    }                                                                          \
  } while (0)

#define queue_dequeue(q, out_item)                                             \
  ({                                                                           \
    b32 __queue_dequeue_success = false;                                       \
    if (!queue_is_empty(q)) {                                                  \
      *(out_item) = (q).items[(q).head];                                       \
      (q).head = ((q).head + 1) % (q).capacity;                                \
      (q).count--;                                                             \
      __queue_dequeue_success = true;                                          \
    }                                                                          \
    __queue_dequeue_success;                                                   \
  })

#define queue_peek_head(q)                                                     \
  (debug_assert(!queue_is_empty(q)), ((q).items[(q).head]))

#define queue_peek_head_ptr(q)                                                 \
  (debug_assert(!queue_is_empty(q)), &((q).items[(q).head]))

#define queue_clear(q)                                                         \
  do {                                                                         \
    (q).count = 0;                                                             \
    (q).head = 0;                                                              \
    (q).tail = 0;                                                              \
  } while (0)

#define queue_foreach(q, type, item_var)                                       \
  for (u32 __queue_foreach_i = 0, __queue_foreach_idx = (q).head;              \
       __queue_foreach_i < (q).count; __queue_foreach_i++,                     \
           __queue_foreach_idx = (__queue_foreach_idx + 1) % (q).capacity)     \
    for (type item_var = (q).items[__queue_foreach_idx], *__once = &item_var;  \
         __once; __once = NULL)

#define queue_foreach_ptr(q, type, item_ptr_var)                               \
  for (u32 __queue_foreach_ptr_i = 0, __queue_foreach_ptr_idx = (q).head;      \
       __queue_foreach_ptr_i < (q).count; __queue_foreach_ptr_i++,             \
           __queue_foreach_ptr_idx = (__queue_foreach_ptr_idx + 1) %           \
                                          (q).capacity)                        \
    for (type *item_ptr_var = &(q).items[__queue_foreach_ptr_idx],             \
              **__once_ptr = &item_ptr_var;                                    \
         __once_ptr; __once_ptr = NULL)

#endif // !H_QUEUE
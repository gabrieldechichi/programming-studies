#include "handle.h"
#include "assert.h"
#include "lib/array.h"
#include "typedefs.h"
#include <string.h>

#define HANDLE_MAX_INDEX UINT32_MAX

HandleArray _ha_init(Allocator *allocator, u32 initial_capacity,
                     u32 item_stride) {
  assert(initial_capacity > 0);
  assert(item_stride > 0);

  HandleArray array = {0};
  array.item_stride = item_stride;
  array.capacity = initial_capacity;
  array.len = 0;
  array.next = 0;

  array.items = ALLOC_ARRAY(allocator, u8, item_stride * initial_capacity);
  array.handles = dyn_arr_new_alloc(allocator, Handle, initial_capacity);
  array.sparse_indexes =
      dyn_arr_new_alloc(allocator, SparseIndex, initial_capacity);

  return array;
}

Handle _ha_add(HandleArray *array, void *item) {
  if (array->next >= array->sparse_indexes.len) {
    // add new element

    // todo: check overflow
    u32 next_idx = array->len;
    memcpy(array->items + next_idx * array->item_stride, item,
           array->item_stride);
    array->len++;

    SparseIndex sparse_idx = {
        .idx_or_next = next_idx,
        .gen = 1,
    };

    Handle handle = {
        .idx = array->sparse_indexes.len,
        .gen = sparse_idx.gen,
    };

    arr_append(array->sparse_indexes, sparse_idx);
    arr_append(array->handles, handle);
    array->next++;
    return handle;
  } else {
    // we can reuse a slot

    // todo: array to solve this
    u32 next_idx = array->len;
    memcpy(array->items + next_idx * array->item_stride, item,
           array->item_stride);
    array->len++;

    // we can reuse a slot
    SparseIndex* sparse_idx = &array->sparse_indexes.items[array->next];
    u32 saved_next = sparse_idx->idx_or_next;  // Save the next free slot FIRST

    sparse_idx->idx_or_next = next_idx;
    assert_msg(sparse_idx->gen < UINT32_MAX, "Generation overflow");
    sparse_idx->gen++;

    Handle handle = {.idx = array->next, .gen = sparse_idx->gen};
    arr_append(array->handles, handle);

    array->next = saved_next;  // Use the saved value
    return handle;
  }
}

void *_ha_get_assert(HandleArray *array, Handle handle) {
  void *ptr = _ha_get(array, handle);
  debug_assert(ptr);
  return ptr;
}

void *_ha_get(HandleArray *array, Handle handle) {
  if (handle.idx >= array->sparse_indexes.len) {
    return NULL;
  }

  SparseIndex sparse_idx = array->sparse_indexes.items[handle.idx];
  if (sparse_idx.gen != handle.gen) {
    return NULL;
  }

  return array->items + sparse_idx.idx_or_next * array->item_stride;
}

void _ha_remove(HandleArray *array, Handle handle) {
  if (handle.idx >= array->sparse_indexes.len) {
    return ;
  }

  SparseIndex* sparse_idx = &array->sparse_indexes.items[handle.idx];
  if (sparse_idx->gen != handle.gen) {
    return ;
  }
  u32 remove_idx = sparse_idx->idx_or_next;

  // linked list, point to previous next
  sparse_idx->idx_or_next = array->next;
  sparse_idx->gen++;
  // first free is the element that was just removed
  array->next = handle.idx;

  // remove swap
  // todo: use array code here
  u32 last_idx = array->len - 1;
  memcpy(array->items + remove_idx * array->item_stride,
         array->items + last_idx * array->item_stride, array->item_stride);
  array->len--;

  arr_remove_swap(array->handles, remove_idx);

  // update the sparse array to point to the correct element after remove swap
  if (remove_idx < array->len) {
    Handle swapped_handle = array->handles.items[remove_idx];
    array->sparse_indexes.items[swapped_handle.idx].idx_or_next = remove_idx;
  }
}

b32 _ha_is_valid(HandleArray *array, Handle handle) {
  return _ha_get(array, handle) != NULL;
}

u32 _ha_len(HandleArray *array) {
  debug_assert(array);
  if (!array) {
    return 0;
  }
  return array->len;
}

void _ha_clear(HandleArray *array) {
  assert(array);
  array->len = 0;
  array->handles.len = 0;
  array->sparse_indexes.len = 0;
  array->next = 0;
}
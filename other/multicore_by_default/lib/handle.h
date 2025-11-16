/*
    handle.h - Generational handles for safe references

    OVERVIEW

    --- Handles use generation counters to detect stale references

    --- HandleArray: sparse array with stable handles (removing items doesn't invalidate other handles)

    --- use macros to generate typed handle arrays: HANDLE_ARRAY_DEFINE(Type)

    USAGE
        HANDLE_ARRAY_DEFINE(Entity);

        HandleArray_Entity entities = ha_init(Entity, &alloc, 100);
        Handle h = ha_add(Entity, &entities, my_entity);

        Entity *e = ha_get(Entity, &entities, h);
        if (e) { ... }

        ha_remove(Entity, &entities, h);
*/

#ifndef H_HANDLE
#define H_HANDLE
#include "array.h"
#include "memory.h"
#include "typedefs.h"

/* handle with generational index (idx + generation counter) */
typedef struct {
  u32 idx;
  u32 gen;
} Handle;
arr_define(Handle);

/* sparse index entry (stores either array index or next free slot) */
typedef struct {
  u32 idx_or_next;
  u32 gen;
} SparseIndex;
arr_define(SparseIndex);

#define INVALID_HANDLE ((Handle){0})

force_inline b32 handle_equals(const Handle a, const Handle b) {
  return a.gen == b.gen && a.idx == b.idx;
}

force_inline b32 handle_is_valid(const Handle h) { return h.gen != 0; }

/* sparse array accessed by handles, supports add/remove without invalidating handles */
typedef struct {
  u8 *items;
  u32 item_stride;
  u32 capacity;
  u32 len;

  Handle_DynArray handles;
  SparseIndex_DynArray sparse_indexes;
  u32 next;
} HandleArray;

/* initialize handle array (use ha_init macro instead) */
HandleArray _ha_init(Allocator *allocator, u32 initial_capacity,
                     u32 item_stride);

/* add item to array, returns handle (use ha_add macro instead) */
Handle _ha_add(HandleArray *array, void *item);

/* get pointer to item by handle, returns NULL if invalid (use ha_get macro) */
void *_ha_get(HandleArray *array, Handle handle);
void *_ha_get_assert(HandleArray *array, Handle handle);

/* remove item by handle (use ha_remove macro) */
void _ha_remove(HandleArray *array, Handle handle);

/* check if handle is still valid */
b32 _ha_is_valid(HandleArray *array, Handle handle);

/* get number of items in array */
u32 _ha_len(HandleArray *array);

/* clear all items from array */
void _ha_clear(HandleArray *array);

/* define typed handle: TYPED_HANDLE_DEFINE(Entity) -> Entity_Handle */
#define TYPED_HANDLE_DEFINE(type_name) typedef Handle type_name##_Handle

/* define typed handle array: HANDLE_ARRAY_DEFINE(Entity) -> HandleArray_Entity */
#define HANDLE_ARRAY_DEFINE(type) typedef HandleArray HandleArray_##type
#define ha_init(type, allocator, capacity)                                     \
  (_ha_init(allocator, capacity, sizeof(type)))

#define ha_add(type, array, item) (_ha_add((HandleArray *)(array), &(item)))

#define ha_get(type, array, handle)                                            \
  ((type *)_ha_get((HandleArray *)(array), *(Handle *)&(handle)))

#define ha_get_assert(type, array, handle)                                     \
  ((type *)_ha_get_assert((HandleArray *)(array), *(Handle *)&(handle)))

#define ha_remove(type, array, handle)                                         \
  (_ha_remove((HandleArray *)(array), *(Handle *)&(handle)))

#define ha_is_valid(type, array, handle)                                       \
  _ha_is_valid((HandleArray *)(array), *(Handle *)&(handle))

#define ha_len(type, array) (_ha_len((HandleArray *)(array)))

#define ha_clear(type, array) (_ha_clear((HandleArray *)(array)))

#define ha_foreach(type, array, e)                                             \
  for (type * e, *_ptr = (array).items;                                        \
       (void *)_ptr <                                                          \
           (array).items + (array).capacity * (array).item_stride &&           \
       (e = _ptr, 1);                                                          \
       ++_ptr)

#define ha_foreach_handle(array, e) arr_foreach((array).handles, Handle, e)

#endif

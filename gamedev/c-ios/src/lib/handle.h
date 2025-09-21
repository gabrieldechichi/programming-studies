#ifndef H_HANDLE
#define H_HANDLE
#include "array.h"
#include "memory.h"
#include "typedefs.h"

typedef struct {
  u32 idx;
  u32 gen;
} Handle;
arr_define(Handle);
slice_define(Handle);

typedef struct {
  u32 idx_or_next;
  u32 gen;
} SparseIndex;
arr_define(SparseIndex);
slice_define(SparseIndex);

#define INVALID_HANDLE ((Handle){0})

force_inline b32 handle_equals(const Handle a, const Handle b) {
  return a.gen == b.gen && a.idx == b.idx;
}

force_inline b32 handle_is_valid(const Handle h) { return h.gen != 0; }

typedef struct {
  // todo: void* dynamic array support
  void *items;
  u32 item_stride;
  u32 capacity;
  u32 len;

  Handle_Slice handles;
  SparseIndex_Slice sparse_indexes;
  u32 next;
} HandleArray;

HandleArray _ha_init(Allocator *allocator, u32 initial_capacity,
                     u32 item_stride);

Handle _ha_add(HandleArray *array, void *item);

// Get pointer to item by handle (returns NULL if invalid)
void *_ha_get(HandleArray *array, Handle handle);
void *_ha_get_assert(HandleArray *array, Handle handle);

// Remove item by handle (returns pointer to item before removal, NULL if
// invalid)
void _ha_remove(HandleArray *array, Handle handle);

b32 _ha_is_valid(HandleArray *array, Handle handle);

u32 _ha_len(HandleArray *array);

void _ha_clear(HandleArray *array);

#define _HANDLE_STRUCT_DEFINE(type_name)                                       \
  typedef struct {                                                             \
    u32 idx;                                                                   \
    u32 gen;                                                                   \
  } type_name##_Handle;

#define _HANDLE_ARRAY_STRUCT_DEFINE(type)                                      \
  typedef struct {                                                             \
    void *items;                                                               \
    u32 item_stride;                                                           \
    u32 capacity;                                                              \
    u32 len;                                                                   \
    Handle_Slice handles;                                                      \
    SparseIndex_Slice sparse_indexes;                                          \
    u32 next;                                                                  \
  } HandleArray_##type;

// Public macros for defining typed versions
#define TYPED_HANDLE_DEFINE(type_name) _HANDLE_STRUCT_DEFINE(type_name)

#define HANDLE_ARRAY_DEFINE(type) _HANDLE_ARRAY_STRUCT_DEFINE(type)

#define INVALID_TYPED_HANDLE(type_name)                                        \
  ((type_name##_Handle){.idx = 0, .gen = 0})

#define cast_handle(handle_type, x)                                            \
  ({                                                                           \
    __auto_type _to_handle_x = (x);                                            \
    debug_assert(sizeof(_to_handle_x) == sizeof(handle_type));                 \
    (handle_type){.idx = _to_handle_x.idx, .gen = _to_handle_x.gen};           \
  })

// Operation macros that cast between typed and generic versions
#define ha_init(type, allocator, capacity)                                     \
  ({                                                                           \
    HandleArray _ha = _ha_init(allocator, capacity, sizeof(type));             \
    *(HandleArray_##type *)&_ha;                                               \
  })

#define ha_add(type, array, item)                                              \
  ({                                                                           \
    is_type(HandleArray_##type *, array);                                      \
    cast_handle(type##_Handle, _ha_add((HandleArray *)(array), &(item)));      \
  })

#define ha_get(type, array, handle)                                            \
  ({                                                                           \
    is_type(HandleArray_##type *, array);                                      \
    ((type *)_ha_get((HandleArray *)(array), *(Handle *)&(handle)));           \
  })

#define ha_get_assert(type, array, handle)                                     \
  ({                                                                           \
    is_type(HandleArray_##type *, array);                                      \
    ((type *)_ha_get_assert((HandleArray *)(array), *(Handle *)&(handle)));    \
  })

#define ha_remove(type, array, handle)                                         \
  ({                                                                           \
    is_type(HandleArray_##type *, array);                                      \
    (_ha_remove((HandleArray *)(array), *(Handle *)&(handle)));                \
  })

#define ha_is_valid(type, array, handle)                                       \
  ({                                                                           \
    is_type(HandleArray_##type *, array);                                      \
    _ha_is_valid((HandleArray *)(array), *(Handle *)&(handle));                \
  })

#define ha_len(type, array)                                                    \
  ({                                                                           \
    is_type(HandleArray_##type *, array);                                      \
    _ha_len((HandleArray *)(array));                                           \
  })

#define ha_clear(type, array)                                                  \
  ({                                                                           \
    is_type(HandleArray_##type *, array);                                      \
    _ha_clear((HandleArray *)(array));                                         \
  })

#define ha_foreach(type, array, e)                                                \
  for (type * e, *_ptr = (array).items;                                        \
       (void *)_ptr <                                                          \
           (array).items + (array).capacity * (array).item_stride &&           \
       (e = _ptr, 1);                                                          \
       ++_ptr)

#define ha_foreach_handle(array, e) arr_foreach((array).handles, e)

#endif

/*
    array.h - Dynamic arrays

    OVERVIEW

    --- macros for generating type safe arrays

    --- array manipulation: arr_append, arr_remove_swap, arr_increase_len
*/

#ifndef H_ARRAY
#define H_ARRAY

#include "typedefs.h"

/* generates Type_Array (len + items) and Type_DynArray (cap + len + items) */
#define arr_define(type)                                                       \
  typedef struct {                                                             \
    u32 len;                                                                   \
    type *items;                                                               \
  } type##_Array;                                                              \
  typedef struct {                                                             \
    u32 cap;                                                                   \
    u32 len;                                                                   \
    type *items;                                                               \
  } type##_DynArray

/* append element to array (asserts if over capacity) */
#define arr_append(xs, x)                                                      \
  do {                                                                         \
    debug_assert_msg((xs).len < (xs).cap, "Slice append capacity overflow %",  \
                     FMT_UINT((xs).len));                                      \
    if ((xs).len < (xs).cap) {                                                 \
      (xs).items[(xs).len++] = (x);                                            \
    }                                                                          \
  } while (0)

#define dyn_arr_append(xs, x) arr_append(xs, x)

/* increase array length by new_len */
#define arr_increase_len(xs, new_len)                                          \
  do {                                                                         \
    debug_assert((((xs).len) + new_len) <= (xs).cap);                          \
    if ((((xs).len) + new_len) <= (xs).cap) {                                  \
      (xs).len = new_len + ((xs).len);                                         \
    }                                                                          \
  } while (0)

#define dyn_arr_increase_len(xs, new_len) arr_increase_len(xs, new_len)

/* remove element at idx by swapping with last element (O(1), order not
 * preserved) */
#define arr_remove_swap(xs, idx)                                               \
  do {                                                                         \
    u32 __array_remove_idx = (idx);                                            \
    if ((i32)__array_remove_idx == (i32)(xs).len - 1) {                        \
      (xs).len--;                                                              \
    } else if (__array_remove_idx >= 0 &&                                      \
               (i32)__array_remove_idx < (i32)(xs).len) {                      \
      (xs).items[__array_remove_idx] = (xs).items[(xs).len - 1];               \
      (xs).len--;                                                              \
    }                                                                          \
  } while (0)

#define dyn_arr_remove_swap(xs, idx) arr_remove_swap(xs, idx)

#define arr_is_valid_idx(arr, idx)                                             \
  ((i32)(idx) >= (i32)0 && (i32)(idx) < (i32)(arr).len)

#define arr_get_ptr_noassert(arr, idx)                                         \
  (arr_is_valid_idx(arr, idx) ? &((arr).items[idx]) : (void *)NULL)

#define arr_get(arr, idx)                                                      \
  ((debug_assert(arr_is_valid_idx(arr, idx)), (arr).items[idx]))

// todo: see if we can re-add assert here
#define arr_get_ptr(arr, idx) (arr_get_ptr_noassert(arr, idx))

/* create zero-initialized array */
#define arr_new_zero(type) ((type##_Array){0})

/* create array with length, allocates items */
#define arr_new_alloc(arena_ptr, type, length)                                 \
  ((type##_Array){.len = (length),                                             \
                  .items = ALLOC_ARRAY((arena_ptr), type, (length))})

/* wrap C array as Array (no allocation) */
#define arr_from_c_array(type, _arr, _len)                                     \
  ((type##_Array){.len = (_len), .items = (_arr)})

/* create zero-initialized dynamic array */
#define dyn_arr_new_zero(type) ((type##_DynArray){0})

/* wrap C array as DynArray (no allocation) */
#define dyn_arr_from_c_array(type, _arr, _len)                                 \
  ((type##_DynArray){.len = (_len), .cap = (_len), .items = (_arr)})

#define dyn_arr_new_alloc(arena_ptr, type, _cap)                               \
  ((type##_DynArray){.len = 0,                                                 \
                     .cap = (_cap),                                            \
                     .items = ALLOC_ARRAY((arena_ptr), type, (_cap))})

#define ARR_INVALID_INDEX -1

/* iterate raw pointer array with element pointer */
#define foreach_ptr(arr, len, type, e)                                         \
  for (type *e = arr; e < arr + len; ++e)

#define foreach(arr, len, type, e)                                             \
  for (type e, *_ptr = (arr); _ptr < (arr) + (len) && (e = *_ptr, 1); ++_ptr)

#define arr_foreach_ptr(arr, type, e) foreach_ptr(arr.items, arr.len, type, e)
#define arr_foreach(arr, type, e) foreach (arr.items, arr.len, type, e)

#endif // !H_ARRAY

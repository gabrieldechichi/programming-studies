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
      memcpy((void *)&(xs).items[(xs).len], &(x), sizeof((x)));                \
      (xs).len++;                                                              \
    }                                                                          \
  } while (0)

#define Array(type) type##_Array
#define DynArray(type) type##_DynArray
#define ConcurrentArray(type) type##_ConcurrentArray

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
  (arr_is_valid_idx(arr, idx) ? &((arr).items[idx]) : NULL)

#define arr_get(arr, idx)                                                      \
  ((debug_assert(arr_is_valid_idx(arr, idx)), (arr).items[idx]))

#define arr_get_ptr(arr, idx)                                                  \
  ((debug_assert_expr_msg(arr_is_valid_idx(arr, idx),                          \
                          "Array out of bounds. len: %, idx: %",               \
                          FMT_UINT((arr).len), FMT_UINT((idx))),               \
    arr_get_ptr_noassert(arr, idx)))

/* create zero-initialized array */
#define arr_new_zero(type) ((type##_Array){0})

/* create array with length, allocates items */
#define arr_new_alloc(arena_ptr, type, length)                                 \
  ((type##_Array){.len = (length),                                             \
                  .items = ALLOC_ARRAY((arena_ptr), type, (length))})

/* wrap C array as Array (no allocation) */
#define arr_from_c_array(type, _arr, _len)                                     \
  ((type##_Array){.len = (_len), .items = (_arr)})

#define arr_from_const_array(arr)                                                  \
  {.items = arr, .len = ARRAY_SIZE(arr)}

#define arr_const_define(type, ...)                                                 \
  {.items = &((type[]){{0}, __VA_ARGS__})[1], .len = ((size_t)(sizeof((type[]){{0}, __VA_ARGS__}) / sizeof(type) - 1))}

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
#define foreach_ptr(arr, len, type, e) for (type *e = arr; e < arr + len; ++e)

#define foreach(arr, len, type, e)                                             \
  for (type e, *_ptr = (arr); _ptr < (arr) + (len) && (e = *_ptr, 1); ++_ptr)

#define arr_foreach_ptr(arr, type, e) foreach_ptr(arr.items, arr.len, type, e)
#define arr_foreach(arr, type, e) foreach (arr.items, arr.len, type, e)

#define arr_define_concurrent(type)                                            \
  typedef struct {                                                             \
    u32 cap;                                                                   \
    i32 len_atomic;                                                            \
    type *items;                                                               \
  } type##_ConcurrentArray

#define concurrent_arr_new_alloc(arena_ptr, type, _cap)                        \
  ((type##_ConcurrentArray){.len_atomic = 0,                                   \
                            .cap = (_cap),                                     \
                            .items = ALLOC_ARRAY((arena_ptr), type, (_cap))})

#define concurrent_arr_append(xs, x)                                           \
  do {                                                                         \
    i32 __idx = ins_atomic_u32_inc_eval(&(xs).len_atomic) - 1;                 \
    debug_assert_msg(__idx < (i32)(xs).cap,                                    \
                     "Concurrent array capacity overflow");                    \
    (xs).items[__idx] = (x);                                                   \
  } while (0)

#define concurrent_arr_reserve_idx(xs)                                         \
  (ins_atomic_u32_inc_eval(&(xs).len_atomic) - 1)

#define concurrent_arr_reserve(xs, count)                                      \
  (ins_atomic_u32_add_eval(&(xs).len_atomic, (count)) - (count))

#define concurrent_arr_len(xs)                                                 \
  ((u32)ins_atomic_load_acquire(&(xs).len_atomic))

#define concurrent_arr_get(xs, idx) ((xs).items[idx])

#define concurrent_arr_get_ptr(xs, idx) (&(xs).items[idx])

arr_define(CString);
arr_define_concurrent(CString);

#endif // !H_ARRAY


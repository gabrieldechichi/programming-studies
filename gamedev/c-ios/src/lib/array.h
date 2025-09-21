#ifndef H_ARRAY
#define H_ARRAY

#include "typedefs.h"

/*defines a size array*/
#define arr_define(type)                                                       \
  typedef struct {                                                             \
    u32 len;                                                                   \
    type *items;                                                               \
  } type##_Array

/*defines a dynamic array (slice)*/
#define slice_define(type)                                                     \
  typedef struct {                                                             \
    u32 cap;                                                                   \
    u32 len;                                                                   \
    type *items;                                                               \
  } type##_Slice

#define slice_append(xs, x)                                                    \
  do {                                                                         \
    debug_assert_msg((xs).len < (xs).cap, "Slice append capacity overflow %",  \
                     FMT_UINT((xs).len));                                      \
    if ((xs).len < (xs).cap) {                                                 \
      (xs).items[(xs).len++] = x;                                              \
    }                                                                          \
  } while (0)

#define slice_increase_len(xs, new_len)                                        \
  do {                                                                         \
    debug_assert((((xs).len) + new_len) <= (xs).cap);                          \
    if ((((xs).len) + new_len) <= (xs).cap) {                                  \
      (xs).len = new_len + ((xs).len);                                         \
    }                                                                          \
  } while (0)

#define slice_remove_swap(xs, idx)                                             \
  do {                                                                         \
    __auto_type __slice_remove_idx = (idx);                                    \
    if ((i32)__slice_remove_idx == (i32)(xs).len - 1) {                        \
      (xs).len--;                                                              \
    } else if (__slice_remove_idx >= 0 &&                                      \
               (i32)__slice_remove_idx < (i32)(xs).len) {                      \
      (xs).items[__slice_remove_idx] = (xs).items[(xs).len - 1];               \
      (xs).len--;                                                              \
    }                                                                          \
  } while (0)

#define arr_is_valid_idx(arr, idx)                                             \
  ((i32)(idx) >= (i32)0 && (i32)(idx) < (i32)(arr).len)

#define arr_get_ptr_noassert(arr, idx)                                         \
  (arr_is_valid_idx(arr, idx) ? &((arr).items[idx]) : (void *)NULL)

#define arr_get(arr, idx)                                                      \
  ((debug_assert(arr_is_valid_idx(arr, idx)), (arr).items[idx]))

#define arr_get_ptr(arr, idx)                                                  \
  ((debug_assert(arr_is_valid_idx(arr, idx))), (arr_get_ptr_noassert(arr, idx)))

#define foreach_ptr(arr, len, type, e)                                         \
  for (type * e, *_ptr = (arr); _ptr < (arr) + (len) && (e = _ptr, 1); ++_ptr)

#define foreach(arr, len, type, e)                                             \
  for (type e, *_ptr = (arr); _ptr < (arr) + (len) && (e = *_ptr, 1); ++_ptr)

#define arr_foreach_ptr(arr, e)                                                \
  foreach_ptr(arr.items, arr.len, typeof(arr.items[0]), e)
#define arr_foreach(arr, e)                                                    \
  foreach (arr.items, arr.len, typeof(arr.items[0]), e)

#define arr_new_zero(type) ((type##_Array){0})

#define arr_new_ALLOC(arena_ptr, type, length)                                 \
  ((type##_Array){.len = (length),                                             \
                  .items = ALLOC_ARRAY((arena_ptr), type, (length))})

#define arr_from_c_array(type, _arr, _len)                                     \
  ((type##_Array){.len = (_len), .items = (_arr)})

#define arr_from_c_array_alloc(type, arena_ptr, _arr)                          \
  ({                                                                           \
    __auto_type __arr_c_inner = (_arr);                                        \
    u32 __arr_c_len = ARRAY_SIZE(_arr);                                        \
    __auto_type __arr_inner = arr_new_ALLOC(arena_ptr, type, __arr_c_len);     \
    memcpy(__arr_inner.items, __arr_c_inner, sizeof(type) * __arr_c_len);      \
    (type##_Array) __arr_inner;                                                \
  })

#define slice_new_zero(type) ((type##_Slice){0})

#define slice_from_c_array(type, _arr, _len)                                   \
  ((type##_Slice){.len = (_len), .cap = (_len), .items = (_arr)})

#define slice_new_ALLOC(arena_ptr, type, _cap)                                 \
  ((type##_Slice){.len = 0,                                                    \
                  .cap = (_cap),                                               \
                  .items = ALLOC_ARRAY((arena_ptr), type, (_cap))})

#define ARR_INVALID_INDEX -1

#define arr_find_index_raw(arr, len, value)                                    \
  ({                                                                           \
    i32 _idx = ARR_INVALID_INDEX;                                              \
    for (u32 _i = 0; _i < (len); ++_i) {                                       \
      if ((arr)[_i] == (value)) {                                              \
        _idx = (i32)_i;                                                        \
        break;                                                                 \
      }                                                                        \
    }                                                                          \
    _idx;                                                                      \
  })

#define arr_find_index_pred_raw(arr, len, expr)                                \
  ({                                                                           \
    __auto_type __arr_find_pred_arr = (arr);                                   \
    __auto_type __arr_find_pred_len = (len);                                   \
    i32 __arr_find_pred_idx = ARR_INVALID_INDEX;                               \
    for (u32 __arr_find_pred_i = 0; __arr_find_pred_i < __arr_find_pred_len;   \
         ++__arr_find_pred_i) {                                                \
      __auto_type _item = (__arr_find_pred_arr)[__arr_find_pred_i];            \
      if (expr) {                                                              \
        __arr_find_pred_idx = (i32)__arr_find_pred_i;                          \
        break;                                                                 \
      }                                                                        \
    }                                                                          \
    __arr_find_pred_idx;                                                       \
  })

#define arr_find_index(arr, value) arr_find_index_raw(arr.items, arr.len, value)
#define arr_find_index_pred(arr, pred)                                         \
  ({                                                                           \
    __auto_type __arr_find_index_pred_arr = (arr);                             \
    arr_find_index_pred_raw(__arr_find_index_pred_arr.items,                   \
                            __arr_find_index_pred_arr.len, pred);              \
  })

#define arr_sum_raw(arr, len, type)                                            \
  ({                                                                           \
    __auto_type __arr_sum_arr = (arr);                                         \
    __auto_type __arr_sum_len = (len);                                         \
    type __arr_sum_sum = 0;                                                    \
    for (u32 __arr_sum_i = 0; __arr_sum_i < __arr_sum_len; ++__arr_sum_i) {    \
      __arr_sum_sum += __arr_sum_arr[__arr_sum_i];                             \
    }                                                                          \
    __arr_sum_sum;                                                             \
  })

#endif // !H_ARRAY

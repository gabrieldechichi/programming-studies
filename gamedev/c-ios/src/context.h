#ifndef H_CONTEXT
#define H_CONTEXT

#include "lib/memory.h"
typedef struct {
  Allocator allocator;
  Allocator temp_allocator;
  u32 user_data_type_id;
  void *user_data;
} GameContext;

#define TYPE_ID(type) (type##_TYPE.type_id)

#define ctx_set_user_data(_ctx, _type, _user_data)                             \
  (_ctx)->user_data_type_id = TYPE_ID(_type);                                  \
  (_ctx)->user_data = _user_data;

#define ctx_user_data(_ctx, _type)                                             \
  ({                                                                           \
    assert((_ctx)->user_data_type_id == TYPE_ID(_type));                       \
    (_type *)(_ctx)->user_data;                                                \
  })

#endif // !H_CONTEXT

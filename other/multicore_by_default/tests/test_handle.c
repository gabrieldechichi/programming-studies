#include "../lib/handle.h"
#include "../lib/test.h"
#include "../lib/memory.h"

typedef struct {
  u32 id;
  f32 value;
} TestItem;

// Define typed handle and handle array for TestItem
TYPED_HANDLE_DEFINE(TestItem);
HANDLE_ARRAY_DEFINE(TestItem);

void test_ha_init(TestContext* ctx) {
  Allocator* allocator = &ctx->allocator;
  HandleArray_TestItem array = ha_init(TestItem, allocator, 10);

  assert_eq(array.capacity, 10);
  assert_eq(array.item_stride, sizeof(TestItem));
  assert_eq(array.next, 0);
  assert_eq(array.len, 0);
  assert_true(array.items);
  assert_true(array.handles.items);
  assert_true(array.sparse_indexes.items);
}

void test_ha_add_single(TestContext* ctx) {
  Allocator* allocator = &ctx->allocator;

  HandleArray_TestItem array = ha_init(TestItem, allocator, 10);

  TestItem item = ((TestItem){.id = 42, .value = 3.14f});
  TestItem_Handle h = ha_add(TestItem, &array, item);

  assert_true(handle_is_valid(*(Handle *)&h));
  assert_eq(h.idx, 0);
  assert_eq(h.gen, 1);
  assert_eq(array.len, 1);
}

typedef struct {
  u32 stuff_one;
  b32 stuff_two;
} Stuff;

void test_ha_get_valid(TestContext* ctx) {
  Allocator* allocator = &ctx->allocator;
  HandleArray_TestItem array = ha_init(TestItem, allocator, 10);

  for (u32 i = 0; i < 4; i++) {
    TestItem item = ((TestItem){.id = 42, .value = 3.14});
    TestItem_Handle h = ha_add(TestItem, &array, item);

    TestItem *retrieved = ha_get(TestItem, &array, h);
    assert_true(retrieved != NULL);
    assert_eq(retrieved->id, 42);
    assert_eq(retrieved->value, 3.14f);
    assert_mem_eq(TestItem, retrieved, &item);
  }
}

void test_ha_remove_single(TestContext* ctx) {
  Allocator* allocator = &ctx->allocator;
  HandleArray_TestItem array = ha_init(TestItem, allocator, 10);

  TestItem item = ((TestItem){.id = 42, .value = 3.14f});
  TestItem_Handle h = ha_add(TestItem, &array, item);
  assert_eq(array.len, 1);

  ha_remove(TestItem, &array, h);
  assert_eq(array.len, 0);

  TestItem *retrieved = ha_get(TestItem, &array, h);
  assert_true(retrieved == NULL);
  assert_eq(array.len, 0);
}

void test_ha_remove_multi(TestContext* ctx) {
  Allocator* allocator = &ctx->allocator;
  HandleArray_TestItem array = ha_init(TestItem, allocator, 10);

  TestItem items[] = {
      {.id = 1, .value = 1.14f}, {.id = 2, .value = 2.14f},
      {.id = 3, .value = 3.14f}, {.id = 4, .value = 4.14f},
      {.id = 5, .value = 5.14f},
  };
  TestItem_Handle handles[ARRAY_SIZE(items)];

  for (u32 i = 0; i < ARRAY_SIZE(items); i++) {
    handles[i] = ha_add(TestItem, &array, items[i]);
    assert_eq(array.len, i + 1);
  }
  assert_eq(array.len, ARRAY_SIZE(items));

  // remove a item in the middle
  u32 idx_to_remove_1 = 2;
  {
    ha_remove(TestItem, &array, handles[idx_to_remove_1]);
    assert_eq(array.len, ARRAY_SIZE(items) - 1);
    TestItem *retrieved = ha_get(TestItem, &array, handles[idx_to_remove_1]);
    assert_true(retrieved == NULL);
  }

  // remove an item in the end
  u32 idx_to_remove_2 = ARRAY_SIZE(items) - 1;
  {
    ha_remove(TestItem, &array, handles[idx_to_remove_2]);
    assert_eq(array.len, ARRAY_SIZE(items) - 2);
    TestItem *retrieved = ha_get(TestItem, &array, handles[idx_to_remove_2]);
    assert_true(retrieved == NULL);
  }

  // re-add the item
  {
    handles[idx_to_remove_2] = ha_add(TestItem, &array, items[idx_to_remove_2]);
    TestItem *retrieved = ha_get(TestItem, &array, handles[idx_to_remove_2]);
    assert_false(retrieved == NULL);
    assert_mem_eq(TestItem, retrieved, &items[idx_to_remove_2]);
  }

  // re-add the item
  {
    handles[idx_to_remove_1] = ha_add(TestItem, &array, items[idx_to_remove_1]);
    TestItem *retrieved = ha_get(TestItem, &array, handles[idx_to_remove_1]);
    assert_false(retrieved == NULL);
    assert_mem_eq(TestItem, retrieved, &items[idx_to_remove_1]);
  }

  for (u32 i = 0; i < ARRAY_SIZE(items); i++) {
    TestItem *item = ha_get(TestItem, &array, handles[i]);
    assert_mem_eq(TestItem, item, &items[i]);
  }
}

void test_ha_len(TestContext* ctx) {
  Allocator* allocator = &ctx->allocator;
  HandleArray_TestItem array = ha_init(TestItem, allocator, 10);

  assert_eq(ha_len(TestItem, &array), 0);

  TestItem item1 = ((TestItem){.id = 1, .value = 1.0f});
  TestItem item2 = ((TestItem){.id = 2, .value = 2.0f});
  TestItem item3 = ((TestItem){.id = 3, .value = 3.0f});

  ha_add(TestItem, &array, item1);
  assert_eq(ha_len(TestItem, &array), 1);

  TestItem_Handle h2 = ha_add(TestItem, &array, item2);
  assert_eq(ha_len(TestItem, &array), 2);

  ha_add(TestItem, &array, item3);
  assert_eq(ha_len(TestItem, &array), 3);

  ha_remove(TestItem, &array, h2);
  assert_eq(ha_len(TestItem, &array), 2);
}

void test_ha_clear(TestContext* ctx) {
  Allocator* allocator = &ctx->allocator;
  HandleArray_TestItem array = ha_init(TestItem, allocator, 10);

  TestItem item1 = ((TestItem){.id = 1, .value = 1.0f});
  TestItem item2 = ((TestItem){.id = 2, .value = 2.0f});

  TestItem_Handle h1 = ha_add(TestItem, &array, item1);
  TestItem_Handle h2 = ha_add(TestItem, &array, item2);

  ha_clear(TestItem, &array);

  assert_eq(array.len, 0);
  assert_eq(array.next, 0);
  assert_eq(array.handles.len, 0);
  assert_eq(array.sparse_indexes.len, 0);

  TestItem *retrieved1 = ha_get(TestItem, &array, h1);
  TestItem *retrieved2 = ha_get(TestItem, &array, h2);
  assert_true(retrieved1 == NULL);
  assert_true(retrieved2 == NULL);
}

void test_invalid_handle(TestContext* ctx) {
  Allocator* allocator = &ctx->allocator;
  HandleArray_TestItem array = ha_init(TestItem, allocator, 10);

  TestItem_Handle invalid = INVALID_TYPED_HANDLE(TestItem);
  assert_false(handle_is_valid(*(Handle *)&invalid));

  TestItem *retrieved = ha_get(TestItem, &array, invalid);
  assert_true(retrieved == NULL);
}

void test_handle_multi_add_remove(TestContext* ctx) {
  Allocator* allocator = &ctx->allocator;
  HandleArray_TestItem array = ha_init(TestItem, allocator, 10);

  for (u32 i = 0; i < 4; i++) {
    TestItem item = ((TestItem){.id = 42, .value = 3.14f});
    TestItem_Handle h = ha_add(TestItem, &array, item);
    TestItem *retrieved_1 = ha_get(TestItem, &array, h);
    assert_true(retrieved_1 != NULL);
    assert_eq(retrieved_1->id, 42);
    assert_eq(retrieved_1->value, 3.14f);
    assert_mem_eq(TestItem, retrieved_1, &item);

    ha_remove(TestItem, &array, h);

    TestItem item2 = ((TestItem){.id = 99, .value = 2.71f});
    TestItem_Handle h2 = ha_add(TestItem, &array, item2);
    TestItem *retrieved_2 = ha_get(TestItem, &array, h2);
    assert_true(retrieved_2 != NULL);
    assert_eq(retrieved_2->id, 99);
    assert_eq(retrieved_2->value, 2.71f);
    assert_mem_eq(TestItem, retrieved_2, &item2);
  }
}

void test_stale_handle(TestContext* ctx) {
  Allocator* allocator = &ctx->allocator;
  HandleArray_TestItem array = ha_init(TestItem, allocator, 10);

  TestItem item = ((TestItem){.id = 42, .value = 3.14f});
  TestItem_Handle h = ha_add(TestItem, &array, item);

  ha_remove(TestItem, &array, h);

  TestItem item2 = ((TestItem){.id = 99, .value = 2.71f});
  TestItem_Handle h2 = ha_add(TestItem, &array, item2);

  TestItem *retrieved = ha_get(TestItem, &array, h);
  assert_true(retrieved == NULL);

  TestItem *retrieved2 = ha_get(TestItem, &array, h2);
  assert_true(retrieved2 != NULL);
  assert_eq(retrieved2->id, 99);
}

void test_out_of_bounds_handle(TestContext* ctx) {
  Allocator* allocator = &ctx->allocator;
  HandleArray_TestItem array = ha_init(TestItem, allocator, 10);

  TestItem_Handle out_of_bounds = ((TestItem_Handle){.idx = 100, .gen = 0});
  TestItem *retrieved = ha_get(TestItem, &array, out_of_bounds);
  assert_true(retrieved == NULL);

  TestItem_Handle zero_idx = ((TestItem_Handle){.idx = 0, .gen = 0});
  retrieved = ha_get(TestItem, &array, zero_idx);
  assert_true(retrieved == NULL);
}

void test_handle_equals(TestContext* ctx) {
  Handle h1 = ((Handle){.idx = 5, .gen = 2});
  Handle h2 = ((Handle){.idx = 5, .gen = 2});
  Handle h3 = ((Handle){.idx = 5, .gen = 3});
  Handle h4 = ((Handle){.idx = 6, .gen = 2});

  assert_true(handle_equals(h1, h2));
  assert_false(handle_equals(h1, h3));
  assert_false(handle_equals(h1, h4));
}

void test_handle_is_valid(TestContext* ctx) {
  // valid handles have gen != 0
  Handle valid1 = ((Handle){.idx = 1, .gen = 1});
  Handle valid2 = ((Handle){.idx = 0, .gen = 2});
  Handle valid3 = ((Handle){.idx = 5, .gen = 10});
  Handle invalid = INVALID_HANDLE;
  Handle invalid2 = {.idx = 1, .gen = 0};

  assert_true(handle_is_valid(valid1));
  assert_true(handle_is_valid(valid2));
  assert_true(handle_is_valid(valid3));
  assert_false(handle_is_valid(invalid));
  assert_false(handle_is_valid(invalid2));
}

void test_typed_handle_cast(TestContext* ctx) {
  // Test casting between generic and typed handles
  Handle generic_handle = ((Handle){.idx = 42, .gen = 7});
  TestItem_Handle typed_handle = generic_handle;

  assert_eq(typed_handle.idx, 42);
  assert_eq(typed_handle.gen, 7);

  // Test casting back
  Handle back_to_generic = *(Handle *)&typed_handle;
  assert_eq(back_to_generic.idx, 42);
  assert_eq(back_to_generic.gen, 7);
}

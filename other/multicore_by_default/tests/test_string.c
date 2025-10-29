#include "../lib/string.h"
#include "../lib/test.h"

void test_str_len_basic(TestContext* ctx) {
  assert_eq(str_len("hello"), 5);
  assert_eq(str_len(""), 0);
  assert_eq(str_len("test"), 4);
}

void test_str_equal_basic(TestContext* ctx) {
  assert_true(str_equal("hello", "hello"));
  assert_false(str_equal("hello", "world"));
  assert_true(str_equal("", ""));
}

void test_str_contains_basic(TestContext* ctx) {
  assert_true(str_contains("hello world", "world"));
  assert_true(str_contains("hello world", "hello"));
  assert_false(str_contains("hello world", "xyz"));
}
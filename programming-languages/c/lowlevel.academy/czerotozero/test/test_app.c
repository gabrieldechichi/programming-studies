#include "app.h"
#include "common.h"
#include "file.h"
#include "unity.h"
#include <stdbool.h>

#define ARRAY_SIZE(x) sizeof(x) / sizeof(x[0])

void setUp(void) {
    // set stuff up here
}

void tearDown(void) {
    // clean stuff up here
}

app_run_params_t default_run_params() {
    app_run_params_t p = {0};
    char *args[] = {""};
    p.args = args;
    return p;
}

void test_create_db_file() {
    app_run_params_t p = default_run_params();
    p.newfile = true;
    p.filepath = "test.db";
    int r = run(p);
    TEST_ASSERT_EQUAL_INT(STATUS_SUCCESS, r);

    FILE *f = fopen(p.filepath, "r");
    TEST_ASSERT_NOT_NULL(f);
    fclose(f);

    remove(p.filepath);
}

void test_new_db_file_alredy_exists() {
    app_run_params_t p = default_run_params();
    p.newfile = true;
    p.filepath = "test.db";

    {
        FILE *f = fopen(p.filepath, "w+");
        TEST_ASSERT_NOT_NULL(f);
        fclose(f);
    }

    int r = run(p);
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR, r);

    remove(p.filepath);
}

void test_open_db_file() {
    app_run_params_t p = default_run_params();
    p.newfile = true;
    p.filepath = "test.db";

    // create file first
    {
        p.newfile = true;
        int r = run(p);
        TEST_ASSERT_EQUAL_INT(STATUS_SUCCESS, r);
    }

    // now try to open it
    {
        p.newfile = false;
        int r = run(p);
        TEST_ASSERT_EQUAL_INT(STATUS_SUCCESS, r);
    }

    remove(p.filepath);
}

void test_open_not_exists() {
    app_run_params_t p = default_run_params();
    p.newfile = false;
    p.filepath = "test.db";

    remove(p.filepath);
    int r = run(p);
    TEST_ASSERT_EQUAL_INT(STATUS_ERROR, r);

    remove(p.filepath);
}

// not needed when using generate_test_runner.rb
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_create_db_file);
    RUN_TEST(test_new_db_file_alredy_exists);
    RUN_TEST(test_open_db_file);
    RUN_TEST(test_open_not_exists);
    return UNITY_END();
}

#include "app.h"
#include "common.h"
#include "file.h"
#include "parse.h"
#include "unity.h"
#include "unity_internals.h"
#include <stdbool.h>
#include <stdlib.h>

#define ARRAY_SIZE(x) sizeof(x) / sizeof(x[0])
#define TEST_ASSERT_SUCCESS(r) TEST_ASSERT_EQUAL_INT(STATUS_SUCCESS, r)
#define TEST_ASSERT_ERROR(r) TEST_ASSERT_EQUAL_INT(STATUS_ERROR, r)

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

    // run the program
    {
        int r = run(p);
        TEST_ASSERT_EQUAL_INT(STATUS_SUCCESS, r);
    }

    // verity file was created
    FILE *f = fopen(p.filepath, "r");
    TEST_ASSERT_NOT_NULL(f);

    // verify valid header
    {
        db_t *db = NULL;
        int r = read_db_file(f, &db);
        TEST_ASSERT_EQUAL_INT(r, STATUS_SUCCESS);
        free_db(&db);
    }

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

void test_corrupted_header() {
    app_run_params_t p = default_run_params();
    p.newfile = false;
    p.filepath = "test.db";

    db_t *db = NULL;
    TEST_ASSERT_SUCCESS(new_db_alloc(&db));

    FILE *file = NULL;
    TEST_ASSERT_SUCCESS(create_db_file(p.filepath, &file));

    //corrupt header
    db->header->magic = 123;
    write_db_file(file, db);

    //test failure
    TEST_ASSERT_ERROR(run(p));

    //cleanup
    remove(p.filepath);
    fclose(file);
    free_db(&db);
}

// not needed when using generate_test_runner.rb
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_create_db_file);
    RUN_TEST(test_new_db_file_alredy_exists);
    RUN_TEST(test_open_db_file);
    RUN_TEST(test_open_not_exists);
    RUN_TEST(test_corrupted_header);
    return UNITY_END();
}

#undef ARRAY_SIZE
#undef TEST_ASSERT_SUCCESS

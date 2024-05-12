#include "parse.h"
#include "types.h"
#include "unity.h"
#include <stdbool.h>

void setUp(void) {
    // set stuff up here
}

void tearDown(void) {
    // clean stuff up here
}

void test_write_header(void) {
    db_header_t header = {0};
    header.version = 1;
    header.employess = 5;
    header.file_size = sizeof(header);

    char *file_name = "test.db";
    int r = write_header(&header, file_name);
    TEST_ASSERT_EQUAL_INT(0, r);

    FILE *f = fopen(file_name, "r");
    TEST_ASSERT_NOT_NULL(f);
    fclose(f);
    remove(file_name);
}

void test_read_header(void) {
    char *file_name = "test.db";
    db_header_t source_header = {0};
    source_header.version = 1;
    source_header.employess = 5;
    source_header.file_size = sizeof(source_header);
    {
        int r = write_header(&source_header, file_name);
        TEST_ASSERT_EQUAL_INT(0, r);
    }
    {
        db_header_t header = {0};
        int r = read_header(&header, file_name);
        TEST_ASSERT_EQUAL_INT(0, r);
        TEST_ASSERT_EQUAL_INT(source_header.version, header.version);
        TEST_ASSERT_EQUAL_INT(source_header.employess, header.employess);
        TEST_ASSERT_EQUAL_INT(source_header.file_size, header.file_size);
    }

    remove(file_name);
}

// not needed when using generate_test_runner.rb
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_write_header);
    RUN_TEST(test_read_header);
    return UNITY_END();
}

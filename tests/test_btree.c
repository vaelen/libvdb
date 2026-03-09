/*
 * VDB B-Tree Tests
 *
 * Create, insert, find, delete, persistence, and StringKey tests.
 */

#include "../include/vdb.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int tests_run = 0;
static int tests_passed = 0;

#define RUN_TEST(test_func) do { \
    printf("  " #test_func "... "); \
    tests_run++; \
    if (test_func()) { \
        printf("PASSED\n"); \
        tests_passed++; \
    } else { \
        printf("FAILED\n"); \
    } \
} while(0)

#define ASSERT_EQ(expected, actual) do { \
    if ((long)(expected) != (long)(actual)) { \
        printf("Expected: %ld, Got: %ld\n", (long)(expected), (long)(actual)); \
        return 0; \
    } \
} while(0)

#define ASSERT_TRUE(cond) do { \
    if (!(cond)) { \
        printf("Assertion failed: %s\n", #cond); \
        return 0; \
    } \
} while(0)

static const char *BTREE_TEST_FILE = "tmp/test.bt";

static void cleanup(void) {
    remove(BTREE_TEST_FILE);
}

/* Verify type sizes - catches LP64 issues where long != 4 bytes */
int test_type_sizes(void) {
    ASSERT_EQ(1, (int)sizeof(byte));
    ASSERT_EQ(2, (int)sizeof(int16));
    ASSERT_EQ(2, (int)sizeof(uint16));
    ASSERT_EQ(4, (int)sizeof(int32));
    ASSERT_EQ(4, (int)sizeof(uint32));
    return 1;
}

/* Verify header fields survive a write/read round-trip.
   This catches type-size bugs: if int32 is actually 8 bytes,
   the LE32 serialization reads back garbage. */
int test_btree_header_roundtrip(void) {
    BTree tree;

    cleanup();
    ASSERT_TRUE(CreateBTree(BTREE_TEST_FILE));
    ASSERT_TRUE(OpenBTree(&tree, BTREE_TEST_FILE));

    ASSERT_EQ(1, tree.header.root_page);
    ASSERT_EQ(2, tree.header.next_free_page);
    ASSERT_EQ(2, tree.header.page_count);
    ASSERT_EQ(1, tree.header.version);
    ASSERT_EQ(61, tree.header.order);
    ASSERT_TRUE(memcmp(tree.header.magic, "BTRE", 4) == 0);

    CloseBTree(&tree);
    cleanup();
    return 1;
}

int test_btree_create(void) {
    cleanup();
    ASSERT_TRUE(CreateBTree(BTREE_TEST_FILE));
    cleanup();
    return 1;
}

int test_btree_open_close(void) {
    BTree tree;
    cleanup();
    ASSERT_TRUE(CreateBTree(BTREE_TEST_FILE));
    ASSERT_TRUE(OpenBTree(&tree, BTREE_TEST_FILE));
    ASSERT_TRUE(tree.is_open);
    CloseBTree(&tree);
    ASSERT_TRUE(!tree.is_open);
    cleanup();
    return 1;
}

int test_btree_insert_find(void) {
    BTree tree;
    int32 values[10];
    int16 count;

    cleanup();
    ASSERT_TRUE(CreateBTree(BTREE_TEST_FILE));
    ASSERT_TRUE(OpenBTree(&tree, BTREE_TEST_FILE));

    ASSERT_TRUE(BTreeInsert(&tree, 100, 1000));
    ASSERT_TRUE(BTreeInsert(&tree, 200, 2000));
    ASSERT_TRUE(BTreeInsert(&tree, 50, 500));

    ASSERT_TRUE(BTreeFind(&tree, 100, values, 10, &count));
    ASSERT_EQ(1, count);
    ASSERT_EQ(1000, values[0]);

    ASSERT_TRUE(BTreeFind(&tree, 200, values, 10, &count));
    ASSERT_EQ(1, count);
    ASSERT_EQ(2000, values[0]);

    ASSERT_TRUE(BTreeFind(&tree, 50, values, 10, &count));
    ASSERT_EQ(1, count);
    ASSERT_EQ(500, values[0]);

    /* Key not found */
    ASSERT_TRUE(!BTreeFind(&tree, 999, values, 10, &count));

    CloseBTree(&tree);
    cleanup();
    return 1;
}

int test_btree_multi_value(void) {
    BTree tree;
    int32 values[10];
    int16 count;

    cleanup();
    ASSERT_TRUE(CreateBTree(BTREE_TEST_FILE));
    ASSERT_TRUE(OpenBTree(&tree, BTREE_TEST_FILE));

    ASSERT_TRUE(BTreeInsert(&tree, 42, 100));
    ASSERT_TRUE(BTreeInsert(&tree, 42, 200));
    ASSERT_TRUE(BTreeInsert(&tree, 42, 300));

    ASSERT_TRUE(BTreeFind(&tree, 42, values, 10, &count));
    ASSERT_EQ(3, count);
    ASSERT_EQ(100, values[0]);
    ASSERT_EQ(200, values[1]);
    ASSERT_EQ(300, values[2]);

    CloseBTree(&tree);
    cleanup();
    return 1;
}

int test_btree_delete(void) {
    BTree tree;
    int32 values[10];
    int16 count;

    cleanup();
    ASSERT_TRUE(CreateBTree(BTREE_TEST_FILE));
    ASSERT_TRUE(OpenBTree(&tree, BTREE_TEST_FILE));

    ASSERT_TRUE(BTreeInsert(&tree, 10, 100));
    ASSERT_TRUE(BTreeInsert(&tree, 20, 200));
    ASSERT_TRUE(BTreeInsert(&tree, 30, 300));

    ASSERT_TRUE(BTreeDelete(&tree, 20));
    ASSERT_TRUE(!BTreeFind(&tree, 20, values, 10, &count));

    /* Others still there */
    ASSERT_TRUE(BTreeFind(&tree, 10, values, 10, &count));
    ASSERT_TRUE(BTreeFind(&tree, 30, values, 10, &count));

    CloseBTree(&tree);
    cleanup();
    return 1;
}

int test_btree_delete_value(void) {
    BTree tree;
    int32 values[10];
    int16 count;

    cleanup();
    ASSERT_TRUE(CreateBTree(BTREE_TEST_FILE));
    ASSERT_TRUE(OpenBTree(&tree, BTREE_TEST_FILE));

    ASSERT_TRUE(BTreeInsert(&tree, 42, 100));
    ASSERT_TRUE(BTreeInsert(&tree, 42, 200));
    ASSERT_TRUE(BTreeInsert(&tree, 42, 300));

    ASSERT_TRUE(BTreeDeleteValue(&tree, 42, 200));

    ASSERT_TRUE(BTreeFind(&tree, 42, values, 10, &count));
    ASSERT_EQ(2, count);
    ASSERT_EQ(100, values[0]);
    ASSERT_EQ(300, values[1]);

    /* Deleting last values removes the key */
    ASSERT_TRUE(BTreeDeleteValue(&tree, 42, 100));
    ASSERT_TRUE(BTreeDeleteValue(&tree, 42, 300));
    ASSERT_TRUE(!BTreeFind(&tree, 42, values, 10, &count));

    CloseBTree(&tree);
    cleanup();
    return 1;
}

int test_btree_persistence(void) {
    BTree tree;
    int32 values[10];
    int16 count;

    cleanup();
    ASSERT_TRUE(CreateBTree(BTREE_TEST_FILE));
    ASSERT_TRUE(OpenBTree(&tree, BTREE_TEST_FILE));

    ASSERT_TRUE(BTreeInsert(&tree, 100, 1000));
    ASSERT_TRUE(BTreeInsert(&tree, 200, 2000));
    CloseBTree(&tree);

    /* Reopen and verify */
    ASSERT_TRUE(OpenBTree(&tree, BTREE_TEST_FILE));
    ASSERT_TRUE(BTreeFind(&tree, 100, values, 10, &count));
    ASSERT_EQ(1, count);
    ASSERT_EQ(1000, values[0]);

    ASSERT_TRUE(BTreeFind(&tree, 200, values, 10, &count));
    ASSERT_EQ(1, count);
    ASSERT_EQ(2000, values[0]);

    CloseBTree(&tree);
    cleanup();
    return 1;
}

int test_string_key(void) {
    int32 k1 = StringKey("hello");
    int32 k2 = StringKey("HELLO");
    int32 k3 = StringKey("world");

    /* Case insensitive */
    ASSERT_EQ(k1, k2);
    /* Different strings -> different keys (usually) */
    ASSERT_TRUE(k1 != k3);

    return 1;
}

int main(void) {
    /* Ensure tmp directory exists */
    system("mkdir -p tmp");

    printf("=== B-Tree Tests ===\n");

    RUN_TEST(test_type_sizes);
    RUN_TEST(test_btree_header_roundtrip);
    RUN_TEST(test_btree_create);
    RUN_TEST(test_btree_open_close);
    RUN_TEST(test_btree_insert_find);
    RUN_TEST(test_btree_multi_value);
    RUN_TEST(test_btree_delete);
    RUN_TEST(test_btree_delete_value);
    RUN_TEST(test_btree_persistence);
    RUN_TEST(test_string_key);

    printf("\n%d/%d tests passed\n", tests_passed, tests_run);

    cleanup();
    return (tests_passed == tests_run) ? 0 : 1;
}

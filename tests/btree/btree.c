/* Copyright 2026, Andrew C. Young <andrew@vaelen.org> */
/* SPDX-License-Identifier: MIT */

/*
 * tests/btree/btree.c - B-Tree test suite
 *
 * Tests the file-based B-Tree implementation including creation,
 * insertion, lookup, deletion, persistence, and string keys.
 */

#include <stdio.h>
#include <string.h>
#include "test.h"
#include "btree.h"

/* Test file name used by most tests */
#define TEST_FILE "test.bt"

/* Helper: remove test file if it exists */
static void RemoveTestFile(void)
{
    remove(TEST_FILE);
}

/* ---- Test 1: Type size verification ---- */

static void TestTypeSizes(void)
{
    /* Serialized size is always 20 bytes; struct may be larger due to padding */
    TestAssertTrue(sizeof(BTreeHeader) >= 20,
                   "BTreeHeader should be at least 20 bytes");
    TestAssertTrue(sizeof(LeafEntry) >= 10,
                   "LeafEntry should be at least 10 bytes");
    TestAssertTrue(sizeof(LeafNode) > sizeof(LeafEntry),
                   "LeafNode should be larger than LeafEntry");
    TestAssertTrue(sizeof(BTree) >= sizeof(BTreeHeader),
                   "BTree should contain at least a header");
}

/* ---- Test 2: Header serialization round-trip ---- */

static void TestHeaderRoundTrip(void)
{
    BTree tree;
    BTreeHeader *hdr;

    RemoveTestFile();

    /* Create and reopen to verify header round-trip */
    TestAssertTrue(CreateBTree(TEST_FILE),
                   "CreateBTree should succeed");
    TestAssertTrue(OpenBTree(&tree, TEST_FILE),
                   "OpenBTree should succeed");

    hdr = &tree.header;

    TestAssertTrue(memcmp(hdr->magic, "BTRE", 4) == 0,
                   "magic should be BTRE");
    TestAssertEq(1, (long)hdr->version,
                 "version should be 1");
    TestAssertEq(BT_MAX_KEYS + 1, (long)hdr->order,
                 "order should be BT_MAX_KEYS + 1");
    TestAssertEq(1, (long)hdr->root_page,
                 "root_page should be 1");
    TestAssertEq(2, (long)hdr->next_free_page,
                 "next_free_page should be 2");
    TestAssertEq(2, (long)hdr->page_count,
                 "page_count should be 2");

    CloseBTree(&tree);
    RemoveTestFile();
}

/* ---- Test 3: Create and open ---- */

static void TestCreateAndOpen(void)
{
    BTree tree;

    RemoveTestFile();

    TestAssertTrue(CreateBTree(TEST_FILE),
                   "CreateBTree should succeed");
    TestAssertTrue(OpenBTree(&tree, TEST_FILE),
                   "OpenBTree should succeed");

    TestAssertTrue(tree.is_open, "tree should be open");
    TestAssertTrue(memcmp(tree.header.magic, "BTRE", 4) == 0,
                   "opened tree should have BTRE magic");

    CloseBTree(&tree);
    TestAssertTrue(!tree.is_open, "tree should be closed");

    RemoveTestFile();
}

/* ---- Test 4: Single key-value insert and find ---- */

static void TestSingleInsertFind(void)
{
    BTree tree;
    int32 values[10];
    int16 count;

    RemoveTestFile();
    CreateBTree(TEST_FILE);
    OpenBTree(&tree, TEST_FILE);

    TestAssertTrue(BTreeInsert(&tree, 42, 100),
                   "insert key=42 val=100 should succeed");

    TestAssertTrue(BTreeFind(&tree, 42, values, 10, &count),
                   "find key=42 should succeed");
    TestAssertEq(1, (long)count, "should find 1 value");
    TestAssertEq(100, (long)values[0], "value should be 100");

    CloseBTree(&tree);
    RemoveTestFile();
}

/* ---- Test 5: Multiple key-value pairs ---- */

static void TestMultipleKeys(void)
{
    BTree tree;
    int32 values[10];
    int16 count;

    RemoveTestFile();
    CreateBTree(TEST_FILE);
    OpenBTree(&tree, TEST_FILE);

    BTreeInsert(&tree, 10, 1000);
    BTreeInsert(&tree, 30, 3000);
    BTreeInsert(&tree, 20, 2000);

    /* Verify all three keys */
    TestAssertTrue(BTreeFind(&tree, 10, values, 10, &count),
                   "find key=10 should succeed");
    TestAssertEq(1000, (long)values[0], "key=10 value should be 1000");

    TestAssertTrue(BTreeFind(&tree, 20, values, 10, &count),
                   "find key=20 should succeed");
    TestAssertEq(2000, (long)values[0], "key=20 value should be 2000");

    TestAssertTrue(BTreeFind(&tree, 30, values, 10, &count),
                   "find key=30 should succeed");
    TestAssertEq(3000, (long)values[0], "key=30 value should be 3000");

    CloseBTree(&tree);
    RemoveTestFile();
}

/* ---- Test 6: Multiple values per key ---- */

static void TestMultipleValues(void)
{
    BTree tree;
    int32 values[10];
    int16 count;

    RemoveTestFile();
    CreateBTree(TEST_FILE);
    OpenBTree(&tree, TEST_FILE);

    BTreeInsert(&tree, 50, 501);
    BTreeInsert(&tree, 50, 502);
    BTreeInsert(&tree, 50, 503);

    TestAssertTrue(BTreeFind(&tree, 50, values, 10, &count),
                   "find key=50 should succeed");
    TestAssertEq(3, (long)count, "should find 3 values");
    TestAssertEq(501, (long)values[0], "first value should be 501");
    TestAssertEq(502, (long)values[1], "second value should be 502");
    TestAssertEq(503, (long)values[2], "third value should be 503");

    CloseBTree(&tree);
    RemoveTestFile();
}

/* ---- Test 7: Find non-existent key ---- */

static void TestFindNonExistent(void)
{
    BTree tree;
    int32 values[10];
    int16 count;

    RemoveTestFile();
    CreateBTree(TEST_FILE);
    OpenBTree(&tree, TEST_FILE);

    BTreeInsert(&tree, 1, 10);

    TestAssertTrue(!BTreeFind(&tree, 999, values, 10, &count),
                   "find non-existent key should return FALSE");
    TestAssertEq(0, (long)count, "count should be 0 for missing key");

    CloseBTree(&tree);
    RemoveTestFile();
}

/* ---- Test 8: Delete specific value ---- */

static void TestDeleteValue(void)
{
    BTree tree;
    int32 values[10];
    int16 count;

    RemoveTestFile();
    CreateBTree(TEST_FILE);
    OpenBTree(&tree, TEST_FILE);

    BTreeInsert(&tree, 70, 701);
    BTreeInsert(&tree, 70, 702);
    BTreeInsert(&tree, 70, 703);

    /* Delete the middle value */
    TestAssertTrue(BTreeDeleteValue(&tree, 70, 702),
                   "delete value 702 should succeed");

    TestAssertTrue(BTreeFind(&tree, 70, values, 10, &count),
                   "key=70 should still exist");
    TestAssertEq(2, (long)count, "should have 2 values remaining");
    TestAssertEq(701, (long)values[0], "first value should be 701");
    TestAssertEq(703, (long)values[1], "second value should be 703");

    /* Delete non-existent value should fail */
    TestAssertTrue(!BTreeDeleteValue(&tree, 70, 999),
                   "delete non-existent value should fail");

    CloseBTree(&tree);
    RemoveTestFile();
}

/* ---- Test 9: Delete key entirely ---- */

static void TestDeleteKey(void)
{
    BTree tree;
    int32 values[10];
    int16 count;

    RemoveTestFile();
    CreateBTree(TEST_FILE);
    OpenBTree(&tree, TEST_FILE);

    BTreeInsert(&tree, 10, 100);
    BTreeInsert(&tree, 20, 200);
    BTreeInsert(&tree, 30, 300);

    /* Delete middle key */
    TestAssertTrue(BTreeDelete(&tree, 20),
                   "delete key=20 should succeed");

    TestAssertTrue(!BTreeFind(&tree, 20, values, 10, &count),
                   "key=20 should no longer exist");

    /* Other keys should still be there */
    TestAssertTrue(BTreeFind(&tree, 10, values, 10, &count),
                   "key=10 should still exist");
    TestAssertTrue(BTreeFind(&tree, 30, values, 10, &count),
                   "key=30 should still exist");

    /* Delete non-existent key should fail */
    TestAssertTrue(!BTreeDelete(&tree, 999),
                   "delete non-existent key should fail");

    CloseBTree(&tree);
    RemoveTestFile();
}

/* ---- Test 10: Persistence across close/reopen ---- */

static void TestPersistence(void)
{
    BTree tree;
    int32 values[10];
    int16 count;

    RemoveTestFile();
    CreateBTree(TEST_FILE);

    /* Insert data and close */
    OpenBTree(&tree, TEST_FILE);
    BTreeInsert(&tree, 100, 1001);
    BTreeInsert(&tree, 100, 1002);
    BTreeInsert(&tree, 200, 2001);
    CloseBTree(&tree);

    /* Reopen and verify data persisted */
    OpenBTree(&tree, TEST_FILE);

    TestAssertTrue(BTreeFind(&tree, 100, values, 10, &count),
                   "key=100 should persist after reopen");
    TestAssertEq(2, (long)count, "key=100 should have 2 values");
    TestAssertEq(1001, (long)values[0], "first value should be 1001");
    TestAssertEq(1002, (long)values[1], "second value should be 1002");

    TestAssertTrue(BTreeFind(&tree, 200, values, 10, &count),
                   "key=200 should persist after reopen");
    TestAssertEq(1, (long)count, "key=200 should have 1 value");
    TestAssertEq(2001, (long)values[0], "value should be 2001");

    CloseBTree(&tree);
    RemoveTestFile();
}

/* ---- Test 11: StringKey utility ---- */

static void TestStringKey(void)
{
    int32 key1;
    int32 key2;
    int32 key3;
    int32 key4;

    /* Consistency: same input gives same key */
    key1 = StringKey("alice");
    key2 = StringKey("alice");
    TestAssertEq((long)key1, (long)key2,
                 "StringKey should be consistent");

    /* Case-insensitivity */
    key1 = StringKey("alice");
    key2 = StringKey("ALICE");
    key3 = StringKey("Alice");
    TestAssertEq((long)key1, (long)key2,
                 "StringKey should be case-insensitive (lower vs upper)");
    TestAssertEq((long)key1, (long)key3,
                 "StringKey should be case-insensitive (lower vs mixed)");

    /* Uniqueness: different strings should (usually) produce different keys */
    key1 = StringKey("alice");
    key2 = StringKey("bob");
    key3 = StringKey("carol");
    key4 = StringKey("dave");
    TestAssertNeq((long)key1, (long)key2, "alice != bob");
    TestAssertNeq((long)key1, (long)key3, "alice != carol");
    TestAssertNeq((long)key2, (long)key3, "bob != carol");
    TestAssertNeq((long)key1, (long)key4, "alice != dave");

    /* NULL input returns 0 */
    TestAssertEq(0, (long)StringKey(NULL),
                 "StringKey(NULL) should return 0");
}

/* ---- Test 12: DeleteValue removes key when last value deleted ---- */

static void TestDeleteLastValue(void)
{
    BTree tree;
    int32 values[10];
    int16 count;

    RemoveTestFile();
    CreateBTree(TEST_FILE);
    OpenBTree(&tree, TEST_FILE);

    BTreeInsert(&tree, 55, 550);

    /* Delete the only value -- should remove the key too */
    TestAssertTrue(BTreeDeleteValue(&tree, 55, 550),
                   "delete last value should succeed");
    TestAssertTrue(!BTreeFind(&tree, 55, values, 10, &count),
                   "key should be gone after last value deleted");

    CloseBTree(&tree);
    RemoveTestFile();
}

/* ---- Test 13: Insert fails when leaf is full ---- */

static void TestInsertLeafFull(void)
{
    BTree tree;
    int32 values[10];
    int16 count;
    int32 key;
    int inserted;
    bool result;

    RemoveTestFile();
    CreateBTree(TEST_FILE);
    OpenBTree(&tree, TEST_FILE);

    /*
     * Insert keys until BTreeInsert returns FALSE.
     * The on-disk page limit (~36 keys with 1 value each) will
     * be hit before BT_MAX_KEYS due to compact serialization.
     */
    inserted = 0;
    for (key = 1; key <= BT_MAX_KEYS + 1; key++) {
        result = BTreeInsert(&tree, key, key * 10);
        if (!result) {
            break;
        }
        inserted++;
    }

    TestAssertTrue(inserted > 0,
                   "should insert at least one key before full");
    TestAssertTrue(inserted <= BT_MAX_KEYS,
                   "should not exceed BT_MAX_KEYS insertions");

    /* Verify tree is still consistent: all inserted keys findable */
    for (key = 1; key <= (int32)inserted; key++) {
        TestAssertTrue(BTreeFind(&tree, key, values, 10, &count),
                       "inserted key should still be findable");
        TestAssertEq(1, (long)count,
                     "each key should have 1 value");
        TestAssertEq((long)(key * 10), (long)values[0],
                     "value should match what was inserted");
    }

    CloseBTree(&tree);
    RemoveTestFile();
}

/* ---- Test 14: Max values per key ---- */

static void TestMaxValuesPerKey(void)
{
    BTree tree;
    int32 values[BT_MAX_VALUES + 1];
    int16 count;
    int32 i;
    bool result;
    int inserted;

    RemoveTestFile();
    CreateBTree(TEST_FILE);
    OpenBTree(&tree, TEST_FILE);

    /* Insert BT_MAX_VALUES values for a single key */
    inserted = 0;
    for (i = 0; i < BT_MAX_VALUES; i++) {
        result = BTreeInsert(&tree, 99, 1000 + i);
        if (!result) {
            break;
        }
        inserted++;
    }

    /*
     * The on-disk page may overflow before BT_MAX_VALUES is reached
     * since all values must fit in 512 bytes. Either way, verify
     * what was inserted is findable.
     */
    TestAssertTrue(inserted > 0,
                   "should insert at least one value");

    TestAssertTrue(BTreeFind(&tree, 99, values,
                             (int16)(BT_MAX_VALUES + 1), &count),
                   "key should be findable");
    TestAssertEq((long)inserted, (long)count,
                 "should find all inserted values");

    /* Verify all values are correct */
    for (i = 0; i < (int32)count; i++) {
        TestAssertEq((long)(1000 + i), (long)values[i],
                     "value should match insertion order");
    }

    /* Try inserting one more beyond what succeeded */
    if (inserted == BT_MAX_VALUES) {
        result = BTreeInsert(&tree, 99, 1000 + BT_MAX_VALUES);
        TestAssertTrue(!result,
                       "insert beyond BT_MAX_VALUES should fail");
    }

    CloseBTree(&tree);
    RemoveTestFile();
}

/* ---- Test 15: Duplicate value insertion is a no-op ---- */

static void TestDuplicateValue(void)
{
    BTree tree;
    int32 values[10];
    int16 count;

    RemoveTestFile();
    CreateBTree(TEST_FILE);
    OpenBTree(&tree, TEST_FILE);

    BTreeInsert(&tree, 42, 100);
    BTreeInsert(&tree, 42, 200);

    /* Insert duplicate value */
    TestAssertTrue(BTreeInsert(&tree, 42, 100),
                   "duplicate insert should return TRUE (no-op)");

    /* Verify value count did not increase */
    TestAssertTrue(BTreeFind(&tree, 42, values, 10, &count),
                   "key should be findable");
    TestAssertEq(2, (long)count,
                 "should still have 2 values (no duplicate)");
    TestAssertEq(100, (long)values[0], "first value should be 100");
    TestAssertEq(200, (long)values[1], "second value should be 200");

    CloseBTree(&tree);
    RemoveTestFile();
}

int main(void)
{
    TestInit("B-Tree Tests");

    TestAdd("type sizes", TestTypeSizes);
    TestAdd("header round-trip", TestHeaderRoundTrip);
    TestAdd("create and open", TestCreateAndOpen);
    TestAdd("single insert/find", TestSingleInsertFind);
    TestAdd("multiple keys", TestMultipleKeys);
    TestAdd("multiple values per key", TestMultipleValues);
    TestAdd("find non-existent key", TestFindNonExistent);
    TestAdd("delete specific value", TestDeleteValue);
    TestAdd("delete key entirely", TestDeleteKey);
    TestAdd("persistence across close/reopen", TestPersistence);
    TestAdd("StringKey utility", TestStringKey);
    TestAdd("delete last value removes key", TestDeleteLastValue);
    TestAdd("insert fails when leaf full", TestInsertLeafFull);
    TestAdd("max values per key", TestMaxValuesPerKey);
    TestAdd("duplicate value is no-op", TestDuplicateValue);

    return TestRun();
}

/* Copyright 2026, Andrew C. Young <andrew@vaelen.org> */
/* SPDX-License-Identifier: MIT */

/*
 * tests/db/record.c - Record CRUD tests
 *
 * Tests AddRecord, FindRecordByID, FindRecordByString,
 * UpdateRecord, and DeleteRecord.
 */

#include <string.h>
#include <stdio.h>
#include "test.h"
#include "db.h"

static void CleanupTestFiles(void)
{
    remove("_TEST.DAT");
    remove("_TEST.IDX");
    remove("_TEST.JNL");
    remove("_TEST.I00");
    remove("_TEST.I01");
    remove("_TEST.TMP");
}

/* ---- AddRecord tests ---- */

static void test_add_single(void)
{
    Database db;
    byte data[100];
    int32 record_id;

    CleanupTestFiles();
    CreateDatabase("_TEST", 100);
    OpenDatabase("_TEST", &db);

    memset(data, 0, 100);
    memcpy(data, "Hello World", 11);

    TestAssertTrue(AddRecord(&db, data, &record_id),
                   "AddRecord returns TRUE");
    TestAssertEq(1, (long)record_id, "first record ID is 1");
    TestAssertEq(1, (long)db.header.record_count, "record_count is 1");
    TestAssertEq(2, (long)db.header.next_record_id, "next_record_id is 2");

    CloseDatabase(&db);
    CleanupTestFiles();
}

static void test_find_by_id(void)
{
    Database db;
    byte data[100];
    byte found[100];
    int32 record_id;

    CleanupTestFiles();
    CreateDatabase("_TEST", 100);
    OpenDatabase("_TEST", &db);

    memset(data, 0, 100);
    memcpy(data, "Test Record", 11);
    AddRecord(&db, data, &record_id);

    memset(found, 0, 100);
    TestAssertTrue(FindRecordByID(&db, record_id, found),
                   "FindRecordByID returns TRUE");
    TestAssertTrue(memcmp(data, found, 100) == 0,
                   "found data matches original");

    CloseDatabase(&db);
    CleanupTestFiles();
}

static void test_add_multiple(void)
{
    Database db;
    byte data[100];
    byte found[100];
    int32 id1;
    int32 id2;
    int32 id3;

    CleanupTestFiles();
    CreateDatabase("_TEST", 100);
    OpenDatabase("_TEST", &db);

    memset(data, 0, 100);
    memcpy(data, "Record One", 10);
    AddRecord(&db, data, &id1);

    memset(data, 0, 100);
    memcpy(data, "Record Two", 10);
    AddRecord(&db, data, &id2);

    memset(data, 0, 100);
    memcpy(data, "Record Three", 12);
    AddRecord(&db, data, &id3);

    TestAssertEq(3, (long)db.header.record_count, "3 records");
    TestAssertEq(1, (long)id1, "id1 is 1");
    TestAssertEq(2, (long)id2, "id2 is 2");
    TestAssertEq(3, (long)id3, "id3 is 3");

    /* Find each */
    memset(found, 0, 100);
    TestAssertTrue(FindRecordByID(&db, id1, found), "find id1");
    TestAssertTrue(memcmp(found, "Record One", 10) == 0, "id1 data matches");

    memset(found, 0, 100);
    TestAssertTrue(FindRecordByID(&db, id2, found), "find id2");
    TestAssertTrue(memcmp(found, "Record Two", 10) == 0, "id2 data matches");

    memset(found, 0, 100);
    TestAssertTrue(FindRecordByID(&db, id3, found), "find id3");
    TestAssertTrue(memcmp(found, "Record Three", 12) == 0, "id3 data matches");

    CloseDatabase(&db);
    CleanupTestFiles();
}

static void test_add_multi_page(void)
{
    Database db;
    byte data[600];
    byte found[600];
    int32 record_id;
    int i;

    CleanupTestFiles();
    CreateDatabase("_TEST", 600);
    OpenDatabase("_TEST", &db);

    /* Fill with pattern */
    for (i = 0; i < 600; i++) {
        data[i] = (byte)(i & 0xFF);
    }

    TestAssertTrue(AddRecord(&db, data, &record_id),
                   "AddRecord multi-page returns TRUE");

    memset(found, 0, 600);
    TestAssertTrue(FindRecordByID(&db, record_id, found),
                   "FindRecordByID multi-page returns TRUE");
    TestAssertTrue(memcmp(data, found, 600) == 0,
                   "multi-page data matches");

    CloseDatabase(&db);
    CleanupTestFiles();
}

static void test_find_not_found(void)
{
    Database db;
    byte found[100];

    CleanupTestFiles();
    CreateDatabase("_TEST", 100);
    OpenDatabase("_TEST", &db);

    TestAssertTrue(!FindRecordByID(&db, 999, found),
                   "FindRecordByID returns FALSE for non-existent ID");

    CloseDatabase(&db);
    CleanupTestFiles();
}

static void test_persistence(void)
{
    Database db;
    byte data[100];
    byte found[100];
    int32 record_id;

    CleanupTestFiles();
    CreateDatabase("_TEST", 100);
    OpenDatabase("_TEST", &db);

    memset(data, 0, 100);
    memcpy(data, "Persistent", 10);
    AddRecord(&db, data, &record_id);

    CloseDatabase(&db);

    /* Reopen and verify */
    OpenDatabase("_TEST", &db);
    TestAssertEq(1, (long)db.header.record_count, "record_count persists");

    memset(found, 0, 100);
    TestAssertTrue(FindRecordByID(&db, record_id, found),
                   "record found after reopen");
    TestAssertTrue(memcmp(found, "Persistent", 10) == 0,
                   "data persists after reopen");

    CloseDatabase(&db);
    CleanupTestFiles();
}

/* ---- DeleteRecord tests ---- */

static void test_delete_record(void)
{
    Database db;
    byte data[100];
    byte found[100];
    int32 record_id;

    CleanupTestFiles();
    CreateDatabase("_TEST", 100);
    OpenDatabase("_TEST", &db);

    memset(data, 0, 100);
    memcpy(data, "Delete Me", 9);
    AddRecord(&db, data, &record_id);

    TestAssertTrue(DeleteRecord(&db, record_id),
                   "DeleteRecord returns TRUE");
    TestAssertEq(0, (long)db.header.record_count,
                 "record_count is 0 after delete");
    TestAssertTrue(!FindRecordByID(&db, record_id, found),
                   "FindRecordByID returns FALSE after delete");

    CloseDatabase(&db);
    CleanupTestFiles();
}

static void test_delete_frees_pages(void)
{
    Database db;
    byte data[100];
    int32 record_id;

    CleanupTestFiles();
    CreateDatabase("_TEST", 100);
    OpenDatabase("_TEST", &db);

    memset(data, 0, 100);
    AddRecord(&db, data, &record_id);

    TestAssertEq(0, (long)db.free_list.free_page_count,
                 "no free pages before delete");

    DeleteRecord(&db, record_id);

    TestAssertTrue(db.free_list.free_page_count > 0,
                   "free pages after delete");

    CloseDatabase(&db);
    CleanupTestFiles();
}

static void test_delete_then_add(void)
{
    Database db;
    byte data[100];
    int32 id1;
    int32 id2;
    byte found[100];

    CleanupTestFiles();
    CreateDatabase("_TEST", 100);
    OpenDatabase("_TEST", &db);

    memset(data, 0, 100);
    memcpy(data, "First", 5);
    AddRecord(&db, data, &id1);

    DeleteRecord(&db, id1);

    memset(data, 0, 100);
    memcpy(data, "Second", 6);
    AddRecord(&db, data, &id2);

    memset(found, 0, 100);
    TestAssertTrue(FindRecordByID(&db, id2, found),
                   "new record found after delete+add");
    TestAssertTrue(memcmp(found, "Second", 6) == 0,
                   "new record data correct");

    CloseDatabase(&db);
    CleanupTestFiles();
}

/* ---- UpdateRecord tests ---- */

static void test_update_record(void)
{
    Database db;
    byte data[100];
    byte found[100];
    int32 record_id;

    CleanupTestFiles();
    CreateDatabase("_TEST", 100);
    OpenDatabase("_TEST", &db);

    memset(data, 0, 100);
    memcpy(data, "Original", 8);
    AddRecord(&db, data, &record_id);

    memset(data, 0, 100);
    memcpy(data, "Updated", 7);
    TestAssertTrue(UpdateRecord(&db, record_id, data),
                   "UpdateRecord returns TRUE");

    memset(found, 0, 100);
    FindRecordByID(&db, record_id, found);
    TestAssertTrue(memcmp(found, "Updated", 7) == 0,
                   "updated data matches");

    CloseDatabase(&db);
    CleanupTestFiles();
}

/* ---- FindRecordByString tests ---- */

static void test_find_by_string(void)
{
    Database db;
    byte data[100];
    byte found[100];
    int32 record_id;
    int32 found_id;
    BTree sec_idx;
    int32 key;

    CleanupTestFiles();
    CreateDatabase("_TEST", 100);
    OpenDatabase("_TEST", &db);

    /* Add a secondary index */
    AddIndex(&db, "Username", IT_STRING);

    /* Add a record */
    memset(data, 0, 100);
    memcpy(data, "alice", 5);
    AddRecord(&db, data, &record_id);

    /* Manually insert into secondary index (caller responsibility) */
    OpenBTree(&sec_idx, "_TEST.I00");
    key = GenerateIndexKey(IT_STRING, (const byte *)"alice");
    BTreeInsert(&sec_idx, key, record_id);
    CloseBTree(&sec_idx);

    /* Search */
    memset(found, 0, 100);
    found_id = 0;
    TestAssertTrue(
        FindRecordByString(&db, "Username", "alice", found, &found_id),
        "FindRecordByString returns TRUE");
    TestAssertEq((long)record_id, (long)found_id, "found correct record ID");
    TestAssertTrue(memcmp(found, "alice", 5) == 0, "found data matches");

    CloseDatabase(&db);
    CleanupTestFiles();
}

static void test_find_by_string_case_insensitive(void)
{
    Database db;
    byte data[100];
    byte found[100];
    int32 record_id;
    int32 found_id;
    BTree sec_idx;
    int32 key;

    CleanupTestFiles();
    CreateDatabase("_TEST", 100);
    OpenDatabase("_TEST", &db);

    AddIndex(&db, "Username", IT_STRING);

    memset(data, 0, 100);
    memcpy(data, "alice", 5);
    AddRecord(&db, data, &record_id);

    /* Insert with lowercase key */
    OpenBTree(&sec_idx, "_TEST.I00");
    key = GenerateIndexKey(IT_STRING, (const byte *)"alice");
    BTreeInsert(&sec_idx, key, record_id);
    CloseBTree(&sec_idx);

    /* Search with different case */
    memset(found, 0, 100);
    found_id = 0;
    TestAssertTrue(
        FindRecordByString(&db, "Username", "Alice", found, &found_id),
        "FindRecordByString case-insensitive finds Alice");

    memset(found, 0, 100);
    found_id = 0;
    TestAssertTrue(
        FindRecordByString(&db, "Username", "ALICE", found, &found_id),
        "FindRecordByString case-insensitive finds ALICE");

    CloseDatabase(&db);
    CleanupTestFiles();
}

static void test_find_by_string_not_found(void)
{
    Database db;
    byte found[100];
    int32 found_id;

    CleanupTestFiles();
    CreateDatabase("_TEST", 100);
    OpenDatabase("_TEST", &db);

    AddIndex(&db, "Username", IT_STRING);

    memset(found, 0, 100);
    found_id = 0;
    TestAssertTrue(
        !FindRecordByString(&db, "Username", "nobody", found, &found_id),
        "FindRecordByString returns FALSE for non-existent value");

    CloseDatabase(&db);
    CleanupTestFiles();
}

/* ---- Integration: Full CRUD cycle ---- */

static void test_full_crud_cycle(void)
{
    Database db;
    byte data[100];
    byte found[100];
    int32 ids[5];
    int i;

    CleanupTestFiles();
    CreateDatabase("_TEST", 100);
    OpenDatabase("_TEST", &db);

    /* Add 5 records */
    for (i = 0; i < 5; i++) {
        memset(data, 0, 100);
        data[0] = (byte)(i + 1);
        data[1] = (byte)('A' + i);
        AddRecord(&db, data, &ids[i]);
    }
    TestAssertEq(5, (long)db.header.record_count, "5 records added");

    /* Find each */
    for (i = 0; i < 5; i++) {
        memset(found, 0, 100);
        TestAssertTrue(FindRecordByID(&db, ids[i], found), "find record");
        TestAssertEq((long)(i + 1), (long)found[0], "data[0] matches");
    }

    /* Update 2 records */
    memset(data, 0, 100);
    data[0] = 99;
    UpdateRecord(&db, ids[0], data);

    memset(data, 0, 100);
    data[0] = 88;
    UpdateRecord(&db, ids[1], data);

    /* Delete 1 record */
    DeleteRecord(&db, ids[2]);
    TestAssertEq(4, (long)db.header.record_count, "4 records after delete");

    /* Verify remaining */
    memset(found, 0, 100);
    FindRecordByID(&db, ids[0], found);
    TestAssertEq(99, (long)found[0], "updated record 0");

    memset(found, 0, 100);
    FindRecordByID(&db, ids[1], found);
    TestAssertEq(88, (long)found[0], "updated record 1");

    TestAssertTrue(!FindRecordByID(&db, ids[2], found), "deleted record gone");

    /* Close and reopen */
    CloseDatabase(&db);
    OpenDatabase("_TEST", &db);

    TestAssertEq(4, (long)db.header.record_count, "4 records after reopen");
    memset(found, 0, 100);
    FindRecordByID(&db, ids[3], found);
    TestAssertEq(4, (long)found[0], "record 3 persists");

    CloseDatabase(&db);
    CleanupTestFiles();
}

/* ---- Large dataset ---- */

static void test_large_dataset(void)
{
    Database db;
    byte data[100];
    byte found[100];
    int32 ids[30];
    int i;
    int found_count;

    CleanupTestFiles();
    CreateDatabase("_TEST", 100);
    OpenDatabase("_TEST", &db);

    /* Add 30 records (within single-leaf btree capacity of ~36 keys) */
    for (i = 0; i < 30; i++) {
        memset(data, 0, 100);
        data[0] = (byte)((i + 1) & 0xFF);
        data[1] = (byte)(((i + 1) >> 8) & 0xFF);
        AddRecord(&db, data, &ids[i]);
    }
    TestAssertEq(30, (long)db.header.record_count, "30 records added");

    /* Verify all findable */
    found_count = 0;
    for (i = 0; i < 30; i++) {
        memset(found, 0, 100);
        if (FindRecordByID(&db, ids[i], found)) {
            found_count++;
        }
    }
    TestAssertEq(30, (long)found_count, "all 30 records findable");

    /* Delete half */
    for (i = 0; i < 15; i++) {
        DeleteRecord(&db, ids[i * 2]);
    }
    TestAssertEq(15, (long)db.header.record_count, "15 records after delete");

    /* Verify remaining */
    found_count = 0;
    for (i = 0; i < 30; i++) {
        memset(found, 0, 100);
        if (FindRecordByID(&db, ids[i], found)) {
            found_count++;
        }
    }
    TestAssertEq(15, (long)found_count, "15 records findable after delete");

    CloseDatabase(&db);
    CleanupTestFiles();
}

/* ---- Main ---- */

int main(void)
{
    TestInit("Record Operations");

    TestAdd("AddRecord single", test_add_single);
    TestAdd("FindRecordByID", test_find_by_id);
    TestAdd("AddRecord multiple", test_add_multiple);
    TestAdd("AddRecord multi-page", test_add_multi_page);
    TestAdd("FindRecordByID not found", test_find_not_found);
    TestAdd("Record persistence", test_persistence);
    TestAdd("DeleteRecord", test_delete_record);
    TestAdd("DeleteRecord frees pages", test_delete_frees_pages);
    TestAdd("Delete then add reuses pages", test_delete_then_add);
    TestAdd("UpdateRecord", test_update_record);
    TestAdd("FindRecordByString", test_find_by_string);
    TestAdd("FindRecordByString case-insensitive", test_find_by_string_case_insensitive);
    TestAdd("FindRecordByString not found", test_find_by_string_not_found);
    TestAdd("Full CRUD cycle", test_full_crud_cycle);
    TestAdd("Large dataset", test_large_dataset);

    return TestRun();
}

/* Copyright 2026, Andrew C. Young <andrew@vaelen.org> */
/* SPDX-License-Identifier: MIT */

/*
 * tests/db/maint.c - Maintenance operation tests
 *
 * Tests UpdateFreePages, AllocatePages, AddIndex, RebuildIndex,
 * CompactDatabase, and ValidateDatabase.
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
    remove("_TEST.I02");
    remove("_TEST.TMP");
}

/* ---- UpdateFreePages tests ---- */

static void test_update_free_empty(void)
{
    Database db;

    CleanupTestFiles();
    CreateDatabase("_TEST", 100);
    OpenDatabase("_TEST", &db);

    TestAssertTrue(UpdateFreePages(&db),
                   "UpdateFreePages returns TRUE");
    TestAssertEq(0, (long)db.free_list.free_page_count,
                 "no free pages in empty db");
    TestAssertEq(0, (long)db.free_list.free_page_list_len,
                 "free list len is 0");

    CloseDatabase(&db);
    CleanupTestFiles();
}

static void test_update_free_with_deleted(void)
{
    Database db;
    byte data[100];
    int32 id1;
    int32 id2;

    CleanupTestFiles();
    CreateDatabase("_TEST", 100);
    OpenDatabase("_TEST", &db);

    memset(data, 0, 100);
    AddRecord(&db, data, &id1);
    memset(data, 0, 100);
    AddRecord(&db, data, &id2);

    /* Delete one record to create empty pages */
    DeleteRecord(&db, id1);

    /* Manually reset free list and rescan */
    db.free_list.free_page_count = 0;
    db.free_list.free_page_list_len = 0;

    UpdateFreePages(&db);
    TestAssertTrue(db.free_list.free_page_count > 0,
                   "found free pages after delete");
    TestAssertTrue(db.free_list.free_page_list_len > 0,
                   "free list populated");

    CloseDatabase(&db);
    CleanupTestFiles();
}

/* ---- AllocatePages tests ---- */

static void test_allocate_no_free(void)
{
    Database db;
    int32 page;

    CleanupTestFiles();
    CreateDatabase("_TEST", 100);
    OpenDatabase("_TEST", &db);

    page = AllocatePages(&db, 1);
    TestAssertTrue(page >= 2, "allocated page is >= 2");

    CloseDatabase(&db);
    CleanupTestFiles();
}

static void test_allocate_from_free_list(void)
{
    Database db;
    byte data[100];
    int32 id1;
    int32 page;

    CleanupTestFiles();
    CreateDatabase("_TEST", 100);
    OpenDatabase("_TEST", &db);

    memset(data, 0, 100);
    AddRecord(&db, data, &id1);

    /* Delete to add to free list */
    DeleteRecord(&db, id1);
    TestAssertTrue(db.free_list.free_page_list_len > 0,
                   "free list has entries");

    /* Allocate should reuse freed page */
    page = AllocatePages(&db, 1);
    TestAssertTrue(page >= 2, "allocated from free list");

    CloseDatabase(&db);
    CleanupTestFiles();
}

/* ---- AddIndex tests ---- */

static void test_add_index(void)
{
    Database db;
    FILE *f;

    CleanupTestFiles();
    CreateDatabase("_TEST", 100);
    OpenDatabase("_TEST", &db);

    TestAssertTrue(AddIndex(&db, "Username", IT_STRING),
                   "AddIndex returns TRUE");
    TestAssertEq(1, (long)db.header.index_count, "index_count is 1");
    TestAssertStrEq("Username", db.header.indexes[0].field_name,
                    "field_name correct");
    TestAssertEq((long)IT_STRING, (long)db.header.indexes[0].index_type,
                 "index_type correct");

    /* Verify file exists */
    f = fopen("_TEST.I00", "rb");
    TestAssertTrue(f != NULL, "I00 file exists");
    if (f != NULL) {
        fclose(f);
    }

    CloseDatabase(&db);
    CleanupTestFiles();
}

static void test_add_max_indexes(void)
{
    Database db;
    int i;
    char name[30];
    int result;

    CleanupTestFiles();
    CreateDatabase("_TEST", 100);
    OpenDatabase("_TEST", &db);

    for (i = 0; i < DB_MAX_INDEXES; i++) {
        memset(name, 0, 30);
        name[0] = 'F';
        name[1] = (char)('0' + (i / 10));
        name[2] = (char)('0' + (i % 10));
        result = AddIndex(&db, name, IT_STRING);
        TestAssertTrue(result, "add index succeeds");
    }

    TestAssertEq(DB_MAX_INDEXES, (long)db.header.index_count,
                 "15 indexes added");

    /* Try one more - should fail */
    TestAssertTrue(!AddIndex(&db, "Overflow", IT_STRING),
                   "16th index fails");

    CloseDatabase(&db);

    /* Clean up all index files */
    for (i = 0; i < DB_MAX_INDEXES; i++) {
        char fname[20];
        fname[0] = '_'; fname[1] = 'T'; fname[2] = 'E';
        fname[3] = 'S'; fname[4] = 'T'; fname[5] = '.';
        fname[6] = 'I';
        fname[7] = (char)('0' + (i / 10));
        fname[8] = (char)('0' + (i % 10));
        fname[9] = '\0';
        remove(fname);
    }
    CleanupTestFiles();
}

/* ---- RebuildIndex tests ---- */

static void test_rebuild_primary(void)
{
    Database db;
    byte data[100];
    byte found[100];
    int32 id1;
    int32 id2;

    CleanupTestFiles();
    CreateDatabase("_TEST", 100);
    OpenDatabase("_TEST", &db);

    memset(data, 0, 100);
    memcpy(data, "Rec1", 4);
    AddRecord(&db, data, &id1);

    memset(data, 0, 100);
    memcpy(data, "Rec2", 4);
    AddRecord(&db, data, &id2);

    /* Rebuild primary index */
    TestAssertTrue(RebuildIndex(&db, -1),
                   "RebuildIndex primary returns TRUE");

    /* Verify records still findable */
    memset(found, 0, 100);
    TestAssertTrue(FindRecordByID(&db, id1, found),
                   "record 1 findable after rebuild");
    TestAssertTrue(memcmp(found, "Rec1", 4) == 0,
                   "record 1 data correct");

    memset(found, 0, 100);
    TestAssertTrue(FindRecordByID(&db, id2, found),
                   "record 2 findable after rebuild");

    CloseDatabase(&db);
    CleanupTestFiles();
}

static void test_rebuild_secondary(void)
{
    Database db;
    FILE *f;

    CleanupTestFiles();
    CreateDatabase("_TEST", 100);
    OpenDatabase("_TEST", &db);

    AddIndex(&db, "Username", IT_STRING);

    /* Rebuild secondary (creates empty file) */
    TestAssertTrue(RebuildIndex(&db, 0),
                   "RebuildIndex secondary returns TRUE");

    /* Verify file exists */
    f = fopen("_TEST.I00", "rb");
    TestAssertTrue(f != NULL, "I00 file exists after rebuild");
    if (f != NULL) {
        fclose(f);
    }

    CloseDatabase(&db);
    CleanupTestFiles();
}

/* ---- CompactDatabase tests ---- */

static void test_compact_empty(void)
{
    Database db;

    CleanupTestFiles();
    CreateDatabase("_TEST", 100);
    OpenDatabase("_TEST", &db);

    TestAssertTrue(CompactDatabase(&db),
                   "CompactDatabase empty returns TRUE");

    CloseDatabase(&db);
    CleanupTestFiles();
}

static void test_compact_with_gaps(void)
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
    memcpy(data, "Rec1", 4);
    AddRecord(&db, data, &id1);

    memset(data, 0, 100);
    memcpy(data, "Rec2", 4);
    AddRecord(&db, data, &id2);

    memset(data, 0, 100);
    memcpy(data, "Rec3", 4);
    AddRecord(&db, data, &id3);

    /* Delete middle record */
    DeleteRecord(&db, id2);
    TestAssertEq(2, (long)db.header.record_count, "2 records before compact");

    /* Compact */
    TestAssertTrue(CompactDatabase(&db),
                   "CompactDatabase returns TRUE");

    /* Verify remaining records */
    memset(found, 0, 100);
    TestAssertTrue(FindRecordByID(&db, id1, found),
                   "record 1 findable after compact");
    TestAssertTrue(memcmp(found, "Rec1", 4) == 0,
                   "record 1 data correct");

    memset(found, 0, 100);
    TestAssertTrue(FindRecordByID(&db, id3, found),
                   "record 3 findable after compact");
    TestAssertTrue(memcmp(found, "Rec3", 4) == 0,
                   "record 3 data correct");

    TestAssertEq(0, (long)db.free_list.free_page_count,
                 "no free pages after compact");

    CloseDatabase(&db);
    CleanupTestFiles();
}

static void test_compact_updates_timestamp(void)
{
    Database db;
    int32 old_ts;

    CleanupTestFiles();
    CreateDatabase("_TEST", 100);
    OpenDatabase("_TEST", &db);

    old_ts = db.header.last_compacted;
    CompactDatabase(&db);
    TestAssertTrue(db.header.last_compacted != old_ts,
                   "last_compacted updated");

    CloseDatabase(&db);
    CleanupTestFiles();
}

/* ---- ValidateDatabase tests ---- */

static void test_validate_fresh(void)
{
    Database db;

    CleanupTestFiles();
    CreateDatabase("_TEST", 100);
    OpenDatabase("_TEST", &db);

    TestAssertTrue(ValidateDatabase(&db),
                   "fresh database is valid");

    CloseDatabase(&db);
    CleanupTestFiles();
}

static void test_validate_after_ops(void)
{
    Database db;
    byte data[100];
    int32 id1;
    int32 id2;
    int32 id3;

    CleanupTestFiles();
    CreateDatabase("_TEST", 100);
    OpenDatabase("_TEST", &db);

    memset(data, 0, 100);
    AddRecord(&db, data, &id1);
    memset(data, 0, 100);
    AddRecord(&db, data, &id2);
    memset(data, 0, 100);
    AddRecord(&db, data, &id3);

    DeleteRecord(&db, id2);

    TestAssertTrue(ValidateDatabase(&db),
                   "database valid after operations");

    CloseDatabase(&db);
    CleanupTestFiles();
}

static void test_compaction_workflow(void)
{
    Database db;
    byte data[100];
    byte found[100];
    int32 ids[10];
    int i;
    int found_count;

    CleanupTestFiles();
    CreateDatabase("_TEST", 100);
    OpenDatabase("_TEST", &db);

    /* Add 10 records */
    for (i = 0; i < 10; i++) {
        memset(data, 0, 100);
        data[0] = (byte)(i + 1);
        AddRecord(&db, data, &ids[i]);
    }

    /* Delete 5 records */
    for (i = 0; i < 5; i++) {
        DeleteRecord(&db, ids[i * 2]);
    }
    TestAssertEq(5, (long)db.header.record_count,
                 "5 records after deleting 5");

    /* Compact */
    TestAssertTrue(CompactDatabase(&db),
                   "CompactDatabase returns TRUE");

    /* Verify 5 remain */
    found_count = 0;
    for (i = 0; i < 10; i++) {
        memset(found, 0, 100);
        if (FindRecordByID(&db, ids[i], found)) {
            found_count++;
        }
    }
    TestAssertEq(5, (long)found_count, "5 records findable after compact");

    /* Validate */
    TestAssertTrue(ValidateDatabase(&db),
                   "database valid after compact");

    CloseDatabase(&db);
    CleanupTestFiles();
}

/* ---- Main ---- */

int main(void)
{
    TestInit("Maintenance Operations");

    TestAdd("UpdateFreePages empty", test_update_free_empty);
    TestAdd("UpdateFreePages with deleted", test_update_free_with_deleted);
    TestAdd("AllocatePages no free", test_allocate_no_free);
    TestAdd("AllocatePages from free list", test_allocate_from_free_list);
    TestAdd("AddIndex", test_add_index);
    TestAdd("AddIndex max", test_add_max_indexes);
    TestAdd("RebuildIndex primary", test_rebuild_primary);
    TestAdd("RebuildIndex secondary", test_rebuild_secondary);
    TestAdd("CompactDatabase empty", test_compact_empty);
    TestAdd("CompactDatabase with gaps", test_compact_with_gaps);
    TestAdd("CompactDatabase updates timestamp", test_compact_updates_timestamp);
    TestAdd("ValidateDatabase fresh", test_validate_fresh);
    TestAdd("ValidateDatabase after ops", test_validate_after_ops);
    TestAdd("Compaction workflow", test_compaction_workflow);

    return TestRun();
}

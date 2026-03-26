/* Copyright 2026, Andrew C. Young <andrew@vaelen.org> */
/* SPDX-License-Identifier: MIT */

/*
 * tests/db/dbutil.c - Tests for database serialization helpers
 *
 * Tests CalculatePagesNeeded, GenerateIndexKey, ComputeJournalChecksum,
 * and round-trip serialization for all on-disk structures.
 */

#include <string.h>
#include "test.h"
#include "db.h"

/* ---- CalculatePagesNeeded tests ---- */

static void test_pages_needed_small(void)
{
    TestAssertEq(1, (long)CalculatePagesNeeded(1),
                 "1 byte needs 1 page");
    TestAssertEq(1, (long)CalculatePagesNeeded(100),
                 "100 bytes needs 1 page");
    TestAssertEq(1, (long)CalculatePagesNeeded(506),
                 "506 bytes needs 1 page");
}

static void test_pages_needed_multi(void)
{
    TestAssertEq(2, (long)CalculatePagesNeeded(507),
                 "507 bytes needs 2 pages");
    TestAssertEq(2, (long)CalculatePagesNeeded(1012),
                 "1012 bytes needs 2 pages");
    TestAssertEq(3, (long)CalculatePagesNeeded(1013),
                 "1013 bytes needs 3 pages");
}

static void test_pages_needed_zero(void)
{
    TestAssertEq(1, (long)CalculatePagesNeeded(0),
                 "0 bytes needs 1 page (minimum)");
}

/* ---- GenerateIndexKey tests ---- */

static void test_generate_key_string(void)
{
    int32 key1;
    int32 key2;
    int32 key3;

    key1 = GenerateIndexKey(IT_STRING, (const byte *)"alice");
    key2 = GenerateIndexKey(IT_STRING, (const byte *)"Alice");
    key3 = GenerateIndexKey(IT_STRING, (const byte *)"ALICE");

    TestAssertEq((long)key1, (long)key2,
                 "alice and Alice produce same key");
    TestAssertEq((long)key1, (long)key3,
                 "alice and ALICE produce same key");
    TestAssertTrue(key1 != 0, "string key should be non-zero");
}

static void test_generate_key_id(void)
{
    byte buf[4];
    int32 key;

    WriteInt32LE(buf, 42);
    key = GenerateIndexKey(IT_ID, buf);
    TestAssertEq(42, (long)key, "IT_ID returns value directly");
}

static void test_generate_key_null(void)
{
    TestAssertEq(0, (long)GenerateIndexKey(IT_STRING, NULL),
                 "NULL string returns 0");
    TestAssertEq(0, (long)GenerateIndexKey(IT_ID, NULL),
                 "NULL id returns 0");
}

/* ---- DBHeader serialization tests ---- */

static void test_header_round_trip(void)
{
    DBHeader orig;
    DBHeader restored;
    byte buf[DB_PAGE_SIZE];

    memset(&orig, 0, sizeof(DBHeader));
    memcpy(orig.signature, DB_SIGNATURE, 4);
    orig.version = DB_VERSION;
    orig.page_size = DB_PAGE_SIZE;
    orig.record_size = 100;
    orig.record_count = 42;
    orig.next_record_id = 43;
    orig.last_compacted = 1234567;
    orig.journal_pending = TRUE;
    orig.index_count = 2;

    StrNCopy(orig.indexes[0].field_name, "Username", 30);
    orig.indexes[0].index_type = IT_STRING;
    orig.indexes[0].index_number = 0;

    StrNCopy(orig.indexes[1].field_name, "Email", 30);
    orig.indexes[1].index_type = IT_STRING;
    orig.indexes[1].index_number = 1;

    SerializeHeader(&orig, buf);
    DeserializeHeader(buf, &restored);

    TestAssertStrEq(DB_SIGNATURE, restored.signature,
                    "signature round-trip");
    TestAssertEq((long)DB_VERSION, (long)restored.version,
                 "version round-trip");
    TestAssertEq((long)DB_PAGE_SIZE, (long)restored.page_size,
                 "page_size round-trip");
    TestAssertEq(100, (long)restored.record_size,
                 "record_size round-trip");
    TestAssertEq(42, (long)restored.record_count,
                 "record_count round-trip");
    TestAssertEq(43, (long)restored.next_record_id,
                 "next_record_id round-trip");
    TestAssertEq(1234567, (long)restored.last_compacted,
                 "last_compacted round-trip");
    TestAssertEq(1, (long)restored.journal_pending,
                 "journal_pending round-trip");
    TestAssertEq(2, (long)restored.index_count,
                 "index_count round-trip");
    TestAssertStrEq("Username", restored.indexes[0].field_name,
                    "index 0 field_name round-trip");
    TestAssertEq((long)IT_STRING, (long)restored.indexes[0].index_type,
                 "index 0 type round-trip");
    TestAssertEq(0, (long)restored.indexes[0].index_number,
                 "index 0 number round-trip");
    TestAssertStrEq("Email", restored.indexes[1].field_name,
                    "index 1 field_name round-trip");
    TestAssertEq(1, (long)restored.indexes[1].index_number,
                 "index 1 number round-trip");
}

/* ---- DBFreeList serialization tests ---- */

static void test_freelist_round_trip(void)
{
    DBFreeList orig;
    DBFreeList restored;
    byte buf[DB_PAGE_SIZE];

    memset(&orig, 0, sizeof(DBFreeList));
    orig.free_page_count = 5;
    orig.free_page_list_len = 3;
    orig.free_pages[0] = 10;
    orig.free_pages[1] = 20;
    orig.free_pages[2] = 30;

    SerializeFreeList(&orig, buf);
    DeserializeFreeList(buf, &restored);

    TestAssertEq(5, (long)restored.free_page_count,
                 "free_page_count round-trip");
    TestAssertEq(3, (long)restored.free_page_list_len,
                 "free_page_list_len round-trip");
    TestAssertEq(10, (long)restored.free_pages[0],
                 "free_pages[0] round-trip");
    TestAssertEq(20, (long)restored.free_pages[1],
                 "free_pages[1] round-trip");
    TestAssertEq(30, (long)restored.free_pages[2],
                 "free_pages[2] round-trip");
}

/* ---- DBPage serialization tests ---- */

static void test_page_round_trip(void)
{
    DBPage orig;
    DBPage restored;
    byte buf[DB_PAGE_SIZE];
    int i;

    memset(&orig, 0, sizeof(DBPage));
    orig.id = 42;
    orig.status = PS_ACTIVE;
    orig.reserved = 0;
    for (i = 0; i < DB_PAGE_DATA_SIZE; i++) {
        orig.data[i] = (byte)(i & 0xFF);
    }

    SerializePage(&orig, buf);
    DeserializePage(buf, &restored);

    TestAssertEq(42, (long)restored.id, "page id round-trip");
    TestAssertEq((long)PS_ACTIVE, (long)restored.status,
                 "page status round-trip");
    TestAssertTrue(memcmp(orig.data, restored.data, DB_PAGE_DATA_SIZE) == 0,
                   "page data round-trip");
}

/* ---- DBJournalEntry serialization tests ---- */

static void test_journal_entry_round_trip(void)
{
    DBJournalEntry orig;
    DBJournalEntry restored;
    byte buf[DB_JOURNAL_ENTRY_SIZE];
    int i;

    memset(&orig, 0, sizeof(DBJournalEntry));
    orig.operation = JO_ADD;
    orig.page_num = -1;
    orig.record_id = 99;
    for (i = 0; i < 507; i++) {
        orig.data[i] = (byte)((i * 3) & 0xFF);
    }
    orig.checksum = 0xABCD;

    SerializeJournalEntry(&orig, buf);
    DeserializeJournalEntry(buf, &restored);

    TestAssertEq((long)JO_ADD, (long)restored.operation,
                 "journal operation round-trip");
    TestAssertEq(-1, (long)restored.page_num,
                 "journal page_num round-trip");
    TestAssertEq(99, (long)restored.record_id,
                 "journal record_id round-trip");
    TestAssertTrue(memcmp(orig.data, restored.data, 507) == 0,
                   "journal data round-trip");
    TestAssertEq((long)0xABCD, (long)restored.checksum,
                 "journal checksum round-trip");
}

/* ---- ComputeJournalChecksum tests ---- */

static void test_journal_checksum(void)
{
    DBJournalEntry entry;
    uint16 cs1;
    uint16 cs2;

    memset(&entry, 0, sizeof(DBJournalEntry));
    entry.operation = JO_UPDATE;
    entry.page_num = 5;
    entry.record_id = 10;
    entry.data[0] = 0xAA;

    cs1 = ComputeJournalChecksum(&entry);
    TestAssertTrue(cs1 != 0, "checksum should be non-zero for non-trivial data");

    /* Changing data should change checksum */
    entry.data[0] = 0xBB;
    cs2 = ComputeJournalChecksum(&entry);
    TestAssertTrue(cs1 != cs2, "different data produces different checksum");
}

/* ---- DBIndexInfo serialization tests ---- */

static void test_index_info_round_trip(void)
{
    DBHeader hdr;
    DBHeader restored;
    byte buf[DB_PAGE_SIZE];

    memset(&hdr, 0, sizeof(DBHeader));
    memcpy(hdr.signature, DB_SIGNATURE, 4);
    hdr.version = DB_VERSION;
    hdr.page_size = DB_PAGE_SIZE;
    hdr.index_count = 1;

    StrNCopy(hdr.indexes[0].field_name, "LongFieldNameTest12345", 30);
    hdr.indexes[0].index_type = IT_ID;
    hdr.indexes[0].index_number = 7;

    SerializeHeader(&hdr, buf);
    DeserializeHeader(buf, &restored);

    TestAssertStrEq("LongFieldNameTest12345",
                    restored.indexes[0].field_name,
                    "long field_name round-trip");
    TestAssertEq((long)IT_ID, (long)restored.indexes[0].index_type,
                 "index_type round-trip");
    TestAssertEq(7, (long)restored.indexes[0].index_number,
                 "index_number round-trip");
}

/* ---- BuildFilename tests ---- */

static void test_build_filename(void)
{
    char buf[64];

    BuildFilename("USERS", "DAT", buf, sizeof(buf));
    TestAssertStrEq("USERS.DAT", buf, "BuildFilename DAT");

    BuildFilename("USERS", "IDX", buf, sizeof(buf));
    TestAssertStrEq("USERS.IDX", buf, "BuildFilename IDX");

    BuildFilename("USERS", "I00", buf, sizeof(buf));
    TestAssertStrEq("USERS.I00", buf, "BuildFilename I00");

    BuildFilename("USERS", "JNL", buf, sizeof(buf));
    TestAssertStrEq("USERS.JNL", buf, "BuildFilename JNL");
}

/* ---- Main ---- */

int main(void)
{
    TestInit("Database Utilities");

    TestAdd("CalculatePagesNeeded small", test_pages_needed_small);
    TestAdd("CalculatePagesNeeded multi", test_pages_needed_multi);
    TestAdd("CalculatePagesNeeded zero", test_pages_needed_zero);
    TestAdd("GenerateIndexKey string", test_generate_key_string);
    TestAdd("GenerateIndexKey id", test_generate_key_id);
    TestAdd("GenerateIndexKey null", test_generate_key_null);
    TestAdd("DBHeader round-trip", test_header_round_trip);
    TestAdd("DBFreeList round-trip", test_freelist_round_trip);
    TestAdd("DBPage round-trip", test_page_round_trip);
    TestAdd("DBJournalEntry round-trip", test_journal_entry_round_trip);
    TestAdd("ComputeJournalChecksum", test_journal_checksum);
    TestAdd("DBIndexInfo round-trip", test_index_info_round_trip);
    TestAdd("BuildFilename", test_build_filename);

    return TestRun();
}

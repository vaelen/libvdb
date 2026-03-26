/* Copyright 2026, Andrew C. Young <andrew@vaelen.org> */
/* SPDX-License-Identifier: MIT */

/*
 * tests/db/db.c - Database lifecycle tests
 *
 * Tests CreateDatabase, OpenDatabase, CloseDatabase, and
 * page I/O helper functions.
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

/* ---- CreateDatabase tests ---- */

static void test_create_database(void)
{
    FILE *f;
    byte buf[DB_PAGE_SIZE];
    DBHeader hdr;

    CleanupTestFiles();

    TestAssertTrue(CreateDatabase("_TEST", 100),
                   "CreateDatabase returns TRUE");

    /* Verify .DAT file exists and has 2 pages */
    f = fopen("_TEST.DAT", "rb");
    TestAssertTrue(f != NULL, "DAT file exists");

    if (f != NULL) {
        long size;
        fseek(f, 0, SEEK_END);
        size = ftell(f);
        TestAssertEq(1024, size, "DAT file has 2 pages (1024 bytes)");

        /* Read and verify header */
        fseek(f, 0, SEEK_SET);
        fread(buf, 1, DB_PAGE_SIZE, f);
        DeserializeHeader(buf, &hdr);

        TestAssertStrEq("VDB", hdr.signature, "signature is VDB");
        TestAssertEq(1, (long)hdr.version, "version is 1");
        TestAssertEq(512, (long)hdr.page_size, "page_size is 512");
        TestAssertEq(100, (long)hdr.record_size, "record_size is 100");
        TestAssertEq(0, (long)hdr.record_count, "record_count is 0");
        TestAssertEq(1, (long)hdr.next_record_id, "next_record_id is 1");
        TestAssertEq(0, (long)hdr.journal_pending, "journal_pending is FALSE");
        TestAssertEq(0, (long)hdr.index_count, "index_count is 0");

        fclose(f);
    }

    /* Verify .IDX file exists */
    f = fopen("_TEST.IDX", "rb");
    TestAssertTrue(f != NULL, "IDX file exists");
    if (f != NULL) {
        fclose(f);
    }

    CleanupTestFiles();
}

static void test_create_various_sizes(void)
{
    CleanupTestFiles();

    TestAssertTrue(CreateDatabase("_TEST", 100),
                   "Create with 100 byte records");
    CleanupTestFiles();

    TestAssertTrue(CreateDatabase("_TEST", 506),
                   "Create with 506 byte records");
    CleanupTestFiles();

    TestAssertTrue(CreateDatabase("_TEST", 1012),
                   "Create with 1012 byte records");
    CleanupTestFiles();
}

/* ---- OpenDatabase tests ---- */

static void test_open_database(void)
{
    Database db;

    CleanupTestFiles();
    CreateDatabase("_TEST", 100);

    TestAssertTrue(OpenDatabase("_TEST", &db),
                   "OpenDatabase returns TRUE");
    TestAssertTrue(db.is_open, "db.is_open is TRUE");
    TestAssertStrEq("VDB", db.header.signature, "header signature loaded");
    TestAssertEq(100, (long)db.header.record_size, "record_size loaded");
    TestAssertEq(0, (long)db.header.record_count, "record_count is 0");
    TestAssertEq(1, (long)db.header.next_record_id, "next_record_id is 1");

    CloseDatabase(&db);
    CleanupTestFiles();
}

static void test_open_invalid(void)
{
    Database db;

    CleanupTestFiles();

    TestAssertTrue(!OpenDatabase("_NOFILE", &db),
                   "OpenDatabase returns FALSE for non-existent file");
}

/* ---- CloseDatabase tests ---- */

static void test_close_database(void)
{
    Database db;

    CleanupTestFiles();
    CreateDatabase("_TEST", 100);
    OpenDatabase("_TEST", &db);

    CloseDatabase(&db);
    TestAssertTrue(!db.is_open, "is_open is FALSE after close");

    CleanupTestFiles();
}

static void test_reopen_database(void)
{
    Database db;

    CleanupTestFiles();
    CreateDatabase("_TEST", 200);
    OpenDatabase("_TEST", &db);
    CloseDatabase(&db);

    /* Reopen and verify header preserved */
    TestAssertTrue(OpenDatabase("_TEST", &db),
                   "Reopen succeeds");
    TestAssertEq(200, (long)db.header.record_size,
                 "record_size preserved after reopen");
    TestAssertEq(0, (long)db.header.record_count,
                 "record_count preserved after reopen");

    CloseDatabase(&db);
    CleanupTestFiles();
}

/* ---- Page I/O tests ---- */

static void test_page_io(void)
{
    Database db;
    DBPage write_page;
    DBPage read_page;

    CleanupTestFiles();
    CreateDatabase("_TEST", 100);
    OpenDatabase("_TEST", &db);

    /* Write a page */
    memset(&write_page, 0, sizeof(DBPage));
    write_page.id = 42;
    write_page.status = PS_ACTIVE;
    write_page.data[0] = 0xAA;
    write_page.data[1] = 0xBB;

    TestAssertTrue(WritePageToDisk(&db, 2, &write_page),
                   "WritePageToDisk succeeds");

    /* Read it back */
    TestAssertTrue(ReadPageFromDisk(&db, 2, &read_page),
                   "ReadPageFromDisk succeeds");
    TestAssertEq(42, (long)read_page.id, "page id matches");
    TestAssertEq((long)PS_ACTIVE, (long)read_page.status, "page status matches");
    TestAssertEq(0xAA, (long)read_page.data[0], "data[0] matches");
    TestAssertEq(0xBB, (long)read_page.data[1], "data[1] matches");

    CloseDatabase(&db);
    CleanupTestFiles();
}

static void test_get_total_pages(void)
{
    Database db;

    CleanupTestFiles();
    CreateDatabase("_TEST", 100);
    OpenDatabase("_TEST", &db);

    TestAssertEq(2, (long)GetTotalPages(&db),
                 "fresh database has 2 pages");

    CloseDatabase(&db);
    CleanupTestFiles();
}

/* ---- Main ---- */

int main(void)
{
    TestInit("Database Lifecycle");

    TestAdd("CreateDatabase", test_create_database);
    TestAdd("CreateDatabase various sizes", test_create_various_sizes);
    TestAdd("OpenDatabase", test_open_database);
    TestAdd("OpenDatabase invalid", test_open_invalid);
    TestAdd("CloseDatabase", test_close_database);
    TestAdd("Reopen database", test_reopen_database);
    TestAdd("Page I/O", test_page_io);
    TestAdd("GetTotalPages", test_get_total_pages);

    return TestRun();
}

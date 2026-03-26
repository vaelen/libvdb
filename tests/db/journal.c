/* Copyright 2026, Andrew C. Young <andrew@vaelen.org> */
/* SPDX-License-Identifier: MIT */

/*
 * tests/db/journal.c - Journal and transaction tests
 *
 * Tests BeginTransaction, CommitTransaction, RollbackTransaction,
 * WriteJournalEntry, ReplayJournal, and journaled record operations.
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

/* ---- Basic transaction tests ---- */

static void test_begin_transaction(void)
{
    Database db;

    CleanupTestFiles();
    CreateDatabase("_TEST", 100);
    OpenDatabase("_TEST", &db);

    TestAssertTrue(BeginTransaction(&db),
                   "BeginTransaction returns TRUE");
    TestAssertTrue(db.header.journal_pending,
                   "journal_pending is TRUE");
    TestAssertTrue(db.journal_file != NULL,
                   "journal_file is open");

    CommitTransaction(&db);
    CloseDatabase(&db);
    CleanupTestFiles();
}

static void test_commit_transaction(void)
{
    Database db;

    CleanupTestFiles();
    CreateDatabase("_TEST", 100);
    OpenDatabase("_TEST", &db);

    BeginTransaction(&db);
    TestAssertTrue(CommitTransaction(&db),
                   "CommitTransaction returns TRUE");
    TestAssertTrue(!db.header.journal_pending,
                   "journal_pending is FALSE after commit");

    CloseDatabase(&db);
    CleanupTestFiles();
}

static void test_rollback_transaction(void)
{
    Database db;

    CleanupTestFiles();
    CreateDatabase("_TEST", 100);
    OpenDatabase("_TEST", &db);

    BeginTransaction(&db);
    TestAssertTrue(RollbackTransaction(&db),
                   "RollbackTransaction returns TRUE");
    TestAssertTrue(!db.header.journal_pending,
                   "journal_pending is FALSE after rollback");

    CloseDatabase(&db);
    CleanupTestFiles();
}

/* ---- Journal entry round-trip ---- */

static void test_journal_entry_write_read(void)
{
    Database db;
    DBJournalEntry write_entry;
    DBJournalEntry read_entry;
    byte buf[DB_JOURNAL_ENTRY_SIZE];
    FILE *jnl;
    uint16 expected_cs;

    CleanupTestFiles();
    CreateDatabase("_TEST", 100);
    OpenDatabase("_TEST", &db);

    BeginTransaction(&db);

    /* Write an entry */
    memset(&write_entry, 0, sizeof(DBJournalEntry));
    write_entry.operation = JO_ADD;
    write_entry.page_num = -1;
    write_entry.record_id = 42;
    write_entry.data[0] = PS_ACTIVE;
    memcpy(write_entry.data + 1, "Test Data", 9);
    write_entry.checksum = ComputeJournalChecksum(&write_entry);

    WriteJournalEntry(&db, &write_entry);

    /* Close journal and read it back manually */
    fclose(db.journal_file);
    db.journal_file = NULL;

    jnl = fopen("_TEST.JNL", "rb");
    TestAssertTrue(jnl != NULL, "journal file exists");

    if (jnl != NULL) {
        TestAssertTrue(
            fread(buf, 1, DB_JOURNAL_ENTRY_SIZE, jnl) == DB_JOURNAL_ENTRY_SIZE,
            "read journal entry");
        DeserializeJournalEntry(buf, &read_entry);

        TestAssertEq((long)JO_ADD, (long)read_entry.operation,
                     "operation matches");
        TestAssertEq(-1, (long)read_entry.page_num,
                     "page_num matches");
        TestAssertEq(42, (long)read_entry.record_id,
                     "record_id matches");

        /* Verify checksum */
        expected_cs = ComputeJournalChecksum(&read_entry);
        TestAssertEq((long)expected_cs, (long)read_entry.checksum,
                     "checksum valid");

        fclose(jnl);
    }

    db.header.journal_pending = FALSE;
    WriteHeaderToDisk(&db);
    CloseDatabase(&db);
    CleanupTestFiles();
}

/* ---- Replay tests ---- */

static void test_replay_add(void)
{
    Database db;
    byte data[100];
    byte found[100];
    int32 record_id;

    CleanupTestFiles();
    CreateDatabase("_TEST", 100);
    OpenDatabase("_TEST", &db);

    /* Add a record within a transaction */
    BeginTransaction(&db);

    memset(data, 0, 100);
    memcpy(data, "JournalAdd", 10);
    AddRecord(&db, data, &record_id);

    CommitTransaction(&db);

    /* Verify record exists */
    memset(found, 0, 100);
    TestAssertTrue(FindRecordByID(&db, record_id, found),
                   "record exists after commit");
    TestAssertTrue(memcmp(found, "JournalAdd", 10) == 0,
                   "data matches after commit");

    CloseDatabase(&db);
    CleanupTestFiles();
}

static void test_replay_delete_with_rollback(void)
{
    Database db;
    byte data[100];
    byte found[100];
    int32 record_id;

    CleanupTestFiles();
    CreateDatabase("_TEST", 100);
    OpenDatabase("_TEST", &db);

    /* Add a record first (no transaction) */
    memset(data, 0, 100);
    memcpy(data, "Keep Me", 7);
    AddRecord(&db, data, &record_id);

    CloseDatabase(&db);
    OpenDatabase("_TEST", &db);

    /* Delete within transaction then rollback */
    BeginTransaction(&db);
    DeleteRecord(&db, record_id);
    RollbackTransaction(&db);

    /*
     * After rollback, the delete was already applied to the data file.
     * Rollback only discards the journal; it does not undo already-applied
     * operations. This matches the spec: rollback is for discarding
     * un-committed journal entries before crash, not for undoing applied ops.
     * The record should still be gone.
     */
    memset(found, 0, 100);
    TestAssertTrue(!FindRecordByID(&db, record_id, found),
                   "deleted record not found after rollback");

    CloseDatabase(&db);
    CleanupTestFiles();
}

/* ---- Replay corrupted entry ---- */

static void test_replay_corrupted_entry(void)
{
    Database db;
    byte buf[DB_JOURNAL_ENTRY_SIZE];
    DBJournalEntry entry;
    FILE *jnl;

    CleanupTestFiles();
    CreateDatabase("_TEST", 100);
    OpenDatabase("_TEST", &db);

    /* Write a corrupted journal entry directly */
    memset(&entry, 0, sizeof(DBJournalEntry));
    entry.operation = JO_ADD;
    entry.page_num = 2;
    entry.record_id = 1;
    entry.data[0] = PS_ACTIVE;
    entry.checksum = 0xFFFF; /* Wrong checksum */

    SerializeJournalEntry(&entry, buf);

    jnl = fopen("_TEST.JNL", "wb");
    fwrite(buf, 1, DB_JOURNAL_ENTRY_SIZE, jnl);
    fclose(jnl);

    /* Set journal_pending flag */
    db.header.journal_pending = TRUE;
    WriteHeaderToDisk(&db);
    CloseDatabase(&db);

    /* Reopen triggers replay */
    TestAssertTrue(OpenDatabase("_TEST", &db),
                   "OpenDatabase succeeds with corrupted journal");
    TestAssertTrue(!db.header.journal_pending,
                   "journal_pending cleared after replay");

    CloseDatabase(&db);
    CleanupTestFiles();
}

/* ---- Journaled record operations ---- */

static void test_journaled_add(void)
{
    Database db;
    byte data[100];
    byte found[100];
    int32 record_id;
    FILE *jnl;
    long jnl_size;

    CleanupTestFiles();
    CreateDatabase("_TEST", 100);
    OpenDatabase("_TEST", &db);

    BeginTransaction(&db);

    memset(data, 0, 100);
    memcpy(data, "Journaled", 9);
    AddRecord(&db, data, &record_id);

    /* Verify journal has entries */
    fflush(db.journal_file);
    jnl = fopen("_TEST.JNL", "rb");
    if (jnl != NULL) {
        fseek(jnl, 0, SEEK_END);
        jnl_size = ftell(jnl);
        TestAssertTrue(jnl_size >= DB_JOURNAL_ENTRY_SIZE,
                       "journal has entries after add");
        fclose(jnl);
    }

    CommitTransaction(&db);

    /* Verify record accessible */
    memset(found, 0, 100);
    TestAssertTrue(FindRecordByID(&db, record_id, found),
                   "journaled record findable");

    CloseDatabase(&db);
    CleanupTestFiles();
}

static void test_crash_recovery_add(void)
{
    Database db;
    byte data[100];
    int32 record_id;

    CleanupTestFiles();
    CreateDatabase("_TEST", 100);
    OpenDatabase("_TEST", &db);

    /* Begin transaction and add record */
    BeginTransaction(&db);

    memset(data, 0, 100);
    memcpy(data, "CrashTest", 9);
    AddRecord(&db, data, &record_id);

    /*
     * Close WITHOUT committing - simulates crash.
     * journal_pending remains TRUE.
     * The data was already written to the DAT file.
     */
    if (db.journal_file != NULL) {
        fclose(db.journal_file);
        db.journal_file = NULL;
    }
    /* Write header with journal_pending=TRUE */
    WriteHeaderToDisk(&db);
    WriteFreeListToDisk(&db);
    CloseBTree(&db.primary_index);
    if (db.data_file != NULL) {
        fclose(db.data_file);
        db.data_file = NULL;
    }
    db.is_open = FALSE;

    /* Reopen - triggers ReplayJournal */
    TestAssertTrue(OpenDatabase("_TEST", &db),
                   "OpenDatabase succeeds after crash");
    TestAssertTrue(!db.header.journal_pending,
                   "journal_pending cleared after recovery");

    /*
     * After replay, the record should still exist because:
     * 1. The data was written to DAT during AddRecord
     * 2. ReplayJournal rebuilds the primary index from DAT
     */
    TestAssertTrue(db.header.record_count > 0,
                   "records exist after recovery");

    CloseDatabase(&db);
    CleanupTestFiles();
}

/* ---- Main ---- */

int main(void)
{
    TestInit("Journal and Transactions");

    TestAdd("BeginTransaction", test_begin_transaction);
    TestAdd("CommitTransaction", test_commit_transaction);
    TestAdd("RollbackTransaction", test_rollback_transaction);
    TestAdd("Journal entry write/read", test_journal_entry_write_read);
    TestAdd("Replay JO_ADD", test_replay_add);
    TestAdd("Replay delete with rollback", test_replay_delete_with_rollback);
    TestAdd("Replay corrupted entry", test_replay_corrupted_entry);
    TestAdd("Journaled AddRecord", test_journaled_add);
    TestAdd("Crash recovery for Add", test_crash_recovery_add);

    return TestRun();
}

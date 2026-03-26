/* Copyright 2026, Andrew C. Young <andrew@vaelen.org> */
/* SPDX-License-Identifier: MIT */

/*
 * journal.c - Journal and transaction operations for vDB
 *
 * Implements write-ahead journaling for crash recovery.
 * Journal entries are written before database modifications.
 * On recovery, entries are replayed to restore consistency.
 */

#include <string.h>
#include "db.h"

bool BeginTransaction(Database *db)
{
    char jnl_name[80];

    if (db == NULL || !db->is_open) {
        return FALSE;
    }

    /* Build journal filename */
    BuildFilename(db->name, "JNL", jnl_name, sizeof(jnl_name));

    /* Open journal file for writing */
    db->journal_file = fopen(jnl_name, "wb");
    if (db->journal_file == NULL) {
        return FALSE;
    }

    /* Set pending flag */
    db->header.journal_pending = TRUE;
    WriteHeaderToDisk(db);

    return TRUE;
}

bool CommitTransaction(Database *db)
{
    char jnl_name[80];

    if (db == NULL || !db->is_open) {
        return FALSE;
    }

    /* Clear pending flag */
    db->header.journal_pending = FALSE;
    WriteHeaderToDisk(db);

    /* Close and truncate journal file */
    if (db->journal_file != NULL) {
        fclose(db->journal_file);
        db->journal_file = NULL;
    }

    /* Truncate by reopening as "wb" then closing */
    BuildFilename(db->name, "JNL", jnl_name, sizeof(jnl_name));
    {
        FILE *f = fopen(jnl_name, "wb");
        if (f != NULL) {
            fclose(f);
        }
    }

    return TRUE;
}

bool RollbackTransaction(Database *db)
{
    char jnl_name[80];

    if (db == NULL || !db->is_open) {
        return FALSE;
    }

    /* Clear pending flag */
    db->header.journal_pending = FALSE;
    WriteHeaderToDisk(db);

    /* Close and truncate journal file */
    if (db->journal_file != NULL) {
        fclose(db->journal_file);
        db->journal_file = NULL;
    }

    BuildFilename(db->name, "JNL", jnl_name, sizeof(jnl_name));
    {
        FILE *f = fopen(jnl_name, "wb");
        if (f != NULL) {
            fclose(f);
        }
    }

    return TRUE;
}

bool WriteJournalEntry(Database *db, const DBJournalEntry *entry)
{
    byte buf[DB_JOURNAL_ENTRY_SIZE];
    DBJournalEntry e;

    if (db == NULL || entry == NULL || db->journal_file == NULL) {
        return FALSE;
    }

    /* Copy entry and compute checksum */
    memcpy(&e, entry, sizeof(DBJournalEntry));
    e.checksum = ComputeJournalChecksum(&e);

    SerializeJournalEntry(&e, buf);

    if (fwrite(buf, 1, DB_JOURNAL_ENTRY_SIZE, db->journal_file) !=
        DB_JOURNAL_ENTRY_SIZE) {
        return FALSE;
    }
    fflush(db->journal_file);

    return TRUE;
}

bool ReplayJournal(Database *db)
{
    char jnl_name[80];
    FILE *jnl;
    byte buf[DB_JOURNAL_ENTRY_SIZE];
    DBJournalEntry entry;
    uint16 expected_cs;
    DBPage page;
    int32 total;
    int32 target_page;
    int32 scan_i;
    int32 active_count;

    if (db == NULL || !db->is_open) {
        return FALSE;
    }

    BuildFilename(db->name, "JNL", jnl_name, sizeof(jnl_name));

    jnl = fopen(jnl_name, "rb");
    if (jnl == NULL) {
        /* No journal file - nothing to replay */
        db->header.journal_pending = FALSE;
        WriteHeaderToDisk(db);
        return TRUE;
    }

    /* Read and replay entries */
    while (fread(buf, 1, DB_JOURNAL_ENTRY_SIZE, jnl) == DB_JOURNAL_ENTRY_SIZE) {
        DeserializeJournalEntry(buf, &entry);

        /* Verify checksum */
        expected_cs = ComputeJournalChecksum(&entry);
        if (entry.checksum != expected_cs) {
            continue; /* Skip corrupted entry */
        }

        switch (entry.operation) {
        case JO_UPDATE:
            /* Write data to specified page */
            if (entry.page_num >= 2) {
                memset(&page, 0, sizeof(DBPage));
                page.id = entry.record_id;
                page.status = entry.data[0];
                memcpy(page.data, entry.data + 1, DB_PAGE_DATA_SIZE);
                WritePageToDisk(db, entry.page_num, &page);
            }
            break;

        case JO_DELETE:
            /* Mark page as empty */
            if (entry.page_num >= 2) {
                memset(&page, 0, sizeof(DBPage));
                page.status = PS_EMPTY;
                WritePageToDisk(db, entry.page_num, &page);
            }
            break;

        case JO_ADD:
            /* Find target page - use page_num if specified, else append */
            if (entry.page_num >= 2) {
                target_page = entry.page_num;
            } else {
                /* Append at end of file */
                total = GetTotalPages(db);
                if (total < 2) {
                    total = 2;
                }
                target_page = total;
            }
            memset(&page, 0, sizeof(DBPage));
            page.id = entry.record_id;
            page.status = entry.data[0];
            memcpy(page.data, entry.data + 1, DB_PAGE_DATA_SIZE);
            WritePageToDisk(db, target_page, &page);
            break;

        default:
            break;
        }
    }

    fclose(jnl);

    /* Rebuild primary index from data file */
    RebuildIndex(db, -1);

    /* Recalculate record_count by scanning active pages */
    total = GetTotalPages(db);
    active_count = 0;
    for (scan_i = 2; scan_i < total; scan_i++) {
        if (ReadPageFromDisk(db, scan_i, &page)) {
            if (page.status == PS_ACTIVE) {
                active_count++;
            }
        }
    }
    db->header.record_count = active_count;

    /* Rebuild free page list */
    UpdateFreePages(db);

    /* Clear journal pending flag */
    db->header.journal_pending = FALSE;
    WriteHeaderToDisk(db);

    /* Truncate journal file */
    {
        FILE *f = fopen(jnl_name, "wb");
        if (f != NULL) {
            fclose(f);
        }
    }

    return TRUE;
}

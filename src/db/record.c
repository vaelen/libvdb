/* Copyright 2026, Andrew C. Young <andrew@vaelen.org> */
/* SPDX-License-Identifier: MIT */

/*
 * record.c - Record CRUD operations for vDB
 *
 * Implements AddRecord, FindRecordByID, FindRecordByString,
 * UpdateRecord, and DeleteRecord.
 */

#include <string.h>
#include "db.h"

bool AddRecord(Database *db, const byte *data, int32 *record_id)
{
    int32 new_id;
    uint16 pages_needed;
    int32 first_page;
    DBPage page;
    uint16 i;
    uint16 copy_len;
    uint16 offset;
    DBJournalEntry entry;

    if (db == NULL || !db->is_open || data == NULL || record_id == NULL) {
        return FALSE;
    }

    /* Assign record ID */
    new_id = db->header.next_record_id;
    db->header.next_record_id++;

    /* Calculate pages needed */
    pages_needed = CalculatePagesNeeded(db->header.record_size);

    /* Allocate pages */
    first_page = AllocatePages(db, pages_needed);
    if (first_page < 0) {
        return FALSE;
    }

    /* Write journal entries if transaction is active */
    if (db->header.journal_pending && db->journal_file != NULL) {
        offset = 0;
        for (i = 0; i < pages_needed; i++) {
            memset(&entry, 0, sizeof(DBJournalEntry));
            entry.operation = JO_ADD;
            entry.page_num = first_page + (int32)i;
            entry.record_id = new_id;

            copy_len = DB_PAGE_DATA_SIZE;
            if (offset + copy_len > db->header.record_size) {
                copy_len = db->header.record_size - offset;
            }

            /* Pack page header + data into journal data field */
            /* data field is 507 bytes; page data is 506 + status byte */
            entry.data[0] = (i == 0) ? PS_ACTIVE : PS_CONTINUATION;
            if (copy_len > 0) {
                memcpy(entry.data + 1, data + offset, copy_len);
            }

            entry.checksum = ComputeJournalChecksum(&entry);
            WriteJournalEntry(db, &entry);
            offset += DB_PAGE_DATA_SIZE;
        }
    }

    /* Write data across pages */
    offset = 0;
    for (i = 0; i < pages_needed; i++) {
        memset(&page, 0, sizeof(DBPage));
        page.id = new_id;
        page.status = (i == 0) ? PS_ACTIVE : PS_CONTINUATION;

        copy_len = DB_PAGE_DATA_SIZE;
        if (offset + copy_len > db->header.record_size) {
            copy_len = db->header.record_size - offset;
        }

        if (copy_len > 0) {
            memcpy(page.data, data + offset, copy_len);
        }

        if (!WritePageToDisk(db, first_page + (int32)i, &page)) {
            return FALSE;
        }

        offset += DB_PAGE_DATA_SIZE;
    }

    /* Insert into primary index */
    if (!BTreeInsert(&db->primary_index, new_id, first_page)) {
        return FALSE;
    }

    /* Update header */
    db->header.record_count++;
    WriteHeaderToDisk(db);

    *record_id = new_id;
    return TRUE;
}

bool FindRecordByID(Database *db, int32 id, byte *data)
{
    int32 page_num;
    int16 count;
    uint16 pages_needed;
    uint16 i;
    uint16 copy_len;
    uint16 offset;
    DBPage page;

    if (db == NULL || !db->is_open || data == NULL) {
        return FALSE;
    }

    /* Look up page number in primary index */
    if (!BTreeFind(&db->primary_index, id, &page_num, 1, &count)) {
        return FALSE;
    }
    if (count == 0) {
        return FALSE;
    }

    pages_needed = CalculatePagesNeeded(db->header.record_size);

    /* Read pages and assemble data */
    offset = 0;
    for (i = 0; i < pages_needed; i++) {
        if (!ReadPageFromDisk(db, page_num + (int32)i, &page)) {
            return FALSE;
        }

        /* Verify page */
        if (i == 0) {
            if (page.status != PS_ACTIVE || page.id != id) {
                return FALSE;
            }
        } else {
            if (page.status != PS_CONTINUATION || page.id != id) {
                return FALSE;
            }
        }

        copy_len = DB_PAGE_DATA_SIZE;
        if (offset + copy_len > db->header.record_size) {
            copy_len = db->header.record_size - offset;
        }

        if (copy_len > 0) {
            memcpy(data + offset, page.data, copy_len);
        }

        offset += DB_PAGE_DATA_SIZE;
    }

    return TRUE;
}

bool FindRecordByString(Database *db, const char *field_name,
                        const char *value, byte *data,
                        int32 *record_id)
{
    int i;
    int idx_found;
    DBIndexInfo *idx_info;
    char idx_ext[4];
    char idx_filename[80];
    BTree sec_index;
    int32 key;
    int32 candidates[16];
    int16 cand_count;
    int16 j;

    if (db == NULL || !db->is_open || field_name == NULL ||
        value == NULL || data == NULL || record_id == NULL) {
        return FALSE;
    }

    /* Find the matching index in header */
    idx_found = -1;
    idx_info = NULL;
    for (i = 0; i < (int)db->header.index_count; i++) {
        if (StrCompareI(db->header.indexes[i].field_name, field_name) == 0) {
            idx_found = i;
            idx_info = &db->header.indexes[i];
            break;
        }
    }

    if (idx_found < 0 || idx_info == NULL) {
        return FALSE;
    }

    /* Build secondary index filename */
    idx_ext[0] = 'I';
    idx_ext[1] = (char)('0' + (idx_info->index_number / 10));
    idx_ext[2] = (char)('0' + (idx_info->index_number % 10));
    idx_ext[3] = '\0';
    BuildFilename(db->name, idx_ext, idx_filename, sizeof(idx_filename));

    /* Open secondary index */
    if (!OpenBTree(&sec_index, idx_filename)) {
        return FALSE;
    }

    /* Generate key and search */
    key = GenerateIndexKey(idx_info->index_type, (const byte *)value);

    if (!BTreeFind(&sec_index, key, candidates, 16, &cand_count)) {
        CloseBTree(&sec_index);
        return FALSE;
    }

    CloseBTree(&sec_index);

    /* For each candidate, try to find the record */
    for (j = 0; j < cand_count; j++) {
        if (FindRecordByID(db, candidates[j], data)) {
            *record_id = candidates[j];
            return TRUE;
        }
    }

    return FALSE;
}

bool UpdateRecord(Database *db, int32 id, const byte *data)
{
    int32 page_num;
    int16 count;
    uint16 pages_needed;
    uint16 i;
    uint16 copy_len;
    uint16 offset;
    DBPage page;
    DBJournalEntry entry;

    if (db == NULL || !db->is_open || data == NULL) {
        return FALSE;
    }

    /* Find existing record */
    if (!BTreeFind(&db->primary_index, id, &page_num, 1, &count)) {
        return FALSE;
    }
    if (count == 0) {
        return FALSE;
    }

    pages_needed = CalculatePagesNeeded(db->header.record_size);

    /* Write journal entries if transaction is active */
    if (db->header.journal_pending && db->journal_file != NULL) {
        offset = 0;
        for (i = 0; i < pages_needed; i++) {
            memset(&entry, 0, sizeof(DBJournalEntry));
            entry.operation = JO_UPDATE;
            entry.page_num = page_num + (int32)i;
            entry.record_id = id;

            copy_len = DB_PAGE_DATA_SIZE;
            if (offset + copy_len > db->header.record_size) {
                copy_len = db->header.record_size - offset;
            }

            entry.data[0] = (i == 0) ? PS_ACTIVE : PS_CONTINUATION;
            if (copy_len > 0) {
                memcpy(entry.data + 1, data + offset, copy_len);
            }

            entry.checksum = ComputeJournalChecksum(&entry);
            WriteJournalEntry(db, &entry);
            offset += DB_PAGE_DATA_SIZE;
        }
    }

    /* Overwrite data in existing pages */
    offset = 0;
    for (i = 0; i < pages_needed; i++) {
        memset(&page, 0, sizeof(DBPage));
        page.id = id;
        page.status = (i == 0) ? PS_ACTIVE : PS_CONTINUATION;

        copy_len = DB_PAGE_DATA_SIZE;
        if (offset + copy_len > db->header.record_size) {
            copy_len = db->header.record_size - offset;
        }

        if (copy_len > 0) {
            memcpy(page.data, data + offset, copy_len);
        }

        if (!WritePageToDisk(db, page_num + (int32)i, &page)) {
            return FALSE;
        }

        offset += DB_PAGE_DATA_SIZE;
    }

    return TRUE;
}

bool DeleteRecord(Database *db, int32 id)
{
    int32 page_num;
    int16 count;
    uint16 pages_needed;
    uint16 i;
    DBPage page;
    DBJournalEntry entry;

    if (db == NULL || !db->is_open) {
        return FALSE;
    }

    /* Find the record */
    if (!BTreeFind(&db->primary_index, id, &page_num, 1, &count)) {
        return FALSE;
    }
    if (count == 0) {
        return FALSE;
    }

    pages_needed = CalculatePagesNeeded(db->header.record_size);

    /* Write journal entries if transaction is active */
    if (db->header.journal_pending && db->journal_file != NULL) {
        for (i = 0; i < pages_needed; i++) {
            memset(&entry, 0, sizeof(DBJournalEntry));
            entry.operation = JO_DELETE;
            entry.page_num = page_num + (int32)i;
            entry.record_id = id;
            entry.checksum = ComputeJournalChecksum(&entry);
            WriteJournalEntry(db, &entry);
        }
    }

    /* Mark all pages as empty */
    for (i = 0; i < pages_needed; i++) {
        memset(&page, 0, sizeof(DBPage));
        page.status = PS_EMPTY;
        if (!WritePageToDisk(db, page_num + (int32)i, &page)) {
            return FALSE;
        }
    }

    /* Remove from primary index */
    BTreeDelete(&db->primary_index, id);

    /* Release pages */
    ReleasePages(db, page_num, pages_needed);

    /* Update header */
    db->header.record_count--;
    WriteHeaderToDisk(db);

    return TRUE;
}

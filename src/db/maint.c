/* Copyright 2026, Andrew C. Young <andrew@vaelen.org> */
/* SPDX-License-Identifier: MIT */

/*
 * maint.c - Maintenance operations for vDB
 *
 * Implements UpdateFreePages, CompactDatabase, ValidateDatabase,
 * AddIndex, and RebuildIndex.
 */

#include <string.h>
#include <time.h>
#include "db.h"

bool UpdateFreePages(Database *db)
{
    int32 total;
    int32 i;
    DBPage page;
    uint16 free_count;
    uint16 list_len;

    if (db == NULL || !db->is_open) {
        return FALSE;
    }

    total = GetTotalPages(db);
    free_count = 0;
    list_len = 0;

    /* Reset free list */
    memset(db->free_list.free_pages, 0, sizeof(db->free_list.free_pages));

    /* Scan data pages starting at page 2 */
    for (i = 2; i < total; i++) {
        if (!ReadPageFromDisk(db, i, &page)) {
            continue;
        }
        if (page.status == PS_EMPTY) {
            free_count++;
            if (list_len < DB_MAX_FREE_PAGES) {
                db->free_list.free_pages[list_len] = i;
                list_len++;
            }
        }
    }

    db->free_list.free_page_count = free_count;
    db->free_list.free_page_list_len = list_len;
    WriteFreeListToDisk(db);

    return TRUE;
}

bool AddIndex(Database *db, const char *field_name, byte index_type)
{
    byte next_num;
    char ext[4];
    char idx_filename[80];
    DBIndexInfo *info;

    if (db == NULL || !db->is_open || field_name == NULL) {
        return FALSE;
    }

    if (db->header.index_count >= DB_MAX_INDEXES) {
        return FALSE;
    }

    /* Assign next index number */
    next_num = db->header.index_count;

    /* Build index filename (.I00, .I01, etc.) */
    ext[0] = 'I';
    ext[1] = (char)('0' + (next_num / 10));
    ext[2] = (char)('0' + (next_num % 10));
    ext[3] = '\0';
    BuildFilename(db->name, ext, idx_filename, sizeof(idx_filename));

    /* Create new B-Tree file */
    if (!CreateBTree(idx_filename)) {
        return FALSE;
    }

    /* Add index info to header */
    info = &db->header.indexes[db->header.index_count];
    memset(info, 0, sizeof(DBIndexInfo));
    StrNCopy(info->field_name, field_name, 30);
    info->index_type = index_type;
    info->index_number = next_num;

    db->header.index_count++;
    WriteHeaderToDisk(db);

    return TRUE;
}

bool RebuildIndex(Database *db, int16 index_number)
{
    int32 total;
    int32 i;
    DBPage page;
    char idx_name[80];
    char ext[4];

    if (db == NULL || !db->is_open) {
        return FALSE;
    }

    if (index_number == -1) {
        /* Rebuild primary index */
        BuildFilename(db->name, "IDX", idx_name, sizeof(idx_name));

        /* Close current primary index */
        CloseBTree(&db->primary_index);

        /* Recreate the B-Tree file */
        if (!CreateBTree(idx_name)) {
            return FALSE;
        }

        /* Reopen */
        if (!OpenBTree(&db->primary_index, idx_name)) {
            return FALSE;
        }

        /* Scan all pages and rebuild */
        total = GetTotalPages(db);
        for (i = 2; i < total; i++) {
            if (!ReadPageFromDisk(db, i, &page)) {
                continue;
            }
            if (page.status == PS_ACTIVE) {
                BTreeInsert(&db->primary_index, page.id, i);
            }
        }

        return TRUE;
    }

    /* Rebuild secondary index */
    if (index_number < 0 || index_number >= (int16)db->header.index_count) {
        return FALSE;
    }

    ext[0] = 'I';
    ext[1] = (char)('0' + (db->header.indexes[index_number].index_number / 10));
    ext[2] = (char)('0' + (db->header.indexes[index_number].index_number % 10));
    ext[3] = '\0';
    BuildFilename(db->name, ext, idx_name, sizeof(idx_name));

    /* Recreate as empty - caller must repopulate */
    if (!CreateBTree(idx_name)) {
        return FALSE;
    }

    return TRUE;
}

bool CompactDatabase(Database *db)
{
    char dat_name[80];
    char tmp_name[80];
    FILE *tmp_file;
    int32 total;
    int32 i;
    int32 next_page;
    uint16 pages_needed;
    uint16 j;
    DBPage page;
    DBPage out_page;
    byte buf[DB_PAGE_SIZE];

    if (db == NULL || !db->is_open) {
        return FALSE;
    }

    BuildFilename(db->name, "DAT", dat_name, sizeof(dat_name));
    BuildFilename(db->name, "TMP", tmp_name, sizeof(tmp_name));

    pages_needed = CalculatePagesNeeded(db->header.record_size);

    /* Create temp file */
    tmp_file = fopen(tmp_name, "wb");
    if (tmp_file == NULL) {
        return FALSE;
    }

    /* Write header (page 0) and free list (page 1) placeholder */
    SerializeHeader(&db->header, buf);
    fwrite(buf, 1, DB_PAGE_SIZE, tmp_file);

    memset(buf, 0, DB_PAGE_SIZE);
    fwrite(buf, 1, DB_PAGE_SIZE, tmp_file);

    /* Scan original file and copy active records contiguously */
    total = GetTotalPages(db);
    next_page = 2;

    for (i = 2; i < total; i++) {
        if (!ReadPageFromDisk(db, i, &page)) {
            continue;
        }
        if (page.status != PS_ACTIVE) {
            continue;
        }

        /* Copy all pages for this record */
        for (j = 0; j < pages_needed; j++) {
            if (j == 0) {
                /* Already read the first page */
                memcpy(&out_page, &page, sizeof(DBPage));
            } else {
                if (!ReadPageFromDisk(db, i + (int32)j, &out_page)) {
                    memset(&out_page, 0, sizeof(DBPage));
                    out_page.id = page.id;
                    out_page.status = PS_CONTINUATION;
                }
            }

            SerializePage(&out_page, buf);
            if (fseek(tmp_file, (long)(next_page + j) * DB_PAGE_SIZE,
                       SEEK_SET) != 0) {
                fclose(tmp_file);
                remove(tmp_name);
                return FALSE;
            }
            fwrite(buf, 1, DB_PAGE_SIZE, tmp_file);
        }

        next_page += pages_needed;

        /* Skip continuation pages in the scan */
        i += (int32)(pages_needed - 1);
    }

    fclose(tmp_file);

    /* Close data file, replace with temp */
    fclose(db->data_file);
    db->data_file = NULL;

    remove(dat_name);
    if (rename(tmp_name, dat_name) != 0) {
        return FALSE;
    }

    /* Reopen data file */
    db->data_file = fopen(dat_name, "r+b");
    if (db->data_file == NULL) {
        return FALSE;
    }

    /* Update header timestamp */
    /* Note: time_t is cast to int32 (Unix epoch seconds).
     * This will overflow in 2038 on systems with 32-bit time_t. */
    db->header.last_compacted = (int32)time(NULL);
    WriteHeaderToDisk(db);

    /* Rebuild primary index by scanning the compacted file */
    RebuildIndex(db, -1);

    /* Reset free list (compacted file has no gaps) */
    UpdateFreePages(db);

    return TRUE;
}

bool ValidateDatabase(Database *db)
{
    int32 total;
    int32 i;
    DBPage page;
    int32 active_count;
    int32 page_num;
    int16 count;

    if (db == NULL || !db->is_open) {
        return FALSE;
    }

    /* Verify header signature and version */
    if (memcmp(db->header.signature, DB_SIGNATURE, 4) != 0) {
        return FALSE;
    }
    if (db->header.version != DB_VERSION) {
        return FALSE;
    }

    /* Count active records and verify index entries */
    total = GetTotalPages(db);
    active_count = 0;

    for (i = 2; i < total; i++) {
        if (!ReadPageFromDisk(db, i, &page)) {
            return FALSE;
        }

        if (page.status == PS_ACTIVE) {
            active_count++;

            /* Verify primary index has this record */
            if (!BTreeFind(&db->primary_index, page.id, &page_num, 1, &count)) {
                return FALSE;
            }
            if (count == 0) {
                return FALSE;
            }

            /* Verify index points to correct page */
            if (page_num != i) {
                return FALSE;
            }
        }
    }

    /* Verify record count matches */
    if (active_count != db->header.record_count) {
        return FALSE;
    }

    return TRUE;
}

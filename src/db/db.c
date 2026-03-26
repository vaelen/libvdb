/* Copyright 2026, Andrew C. Young <andrew@vaelen.org> */
/* SPDX-License-Identifier: MIT */

/*
 * db.c - Database lifecycle and page I/O for vDB
 *
 * Implements CreateDatabase, OpenDatabase, CloseDatabase,
 * and internal page I/O helper functions.
 */

#include <string.h>
#include "db.h"

/* ---- Page I/O Helpers ---- */

bool WriteHeaderToDisk(Database *db)
{
    byte buf[DB_PAGE_SIZE];

    if (db == NULL || db->data_file == NULL) {
        return FALSE;
    }

    SerializeHeader(&db->header, buf);

    if (fseek(db->data_file, 0L, SEEK_SET) != 0) {
        return FALSE;
    }
    if (fwrite(buf, 1, DB_PAGE_SIZE, db->data_file) != DB_PAGE_SIZE) {
        return FALSE;
    }
    fflush(db->data_file);
    return TRUE;
}

bool WriteFreeListToDisk(Database *db)
{
    byte buf[DB_PAGE_SIZE];

    if (db == NULL || db->data_file == NULL) {
        return FALSE;
    }

    SerializeFreeList(&db->free_list, buf);

    if (fseek(db->data_file, (long)DB_PAGE_SIZE, SEEK_SET) != 0) {
        return FALSE;
    }
    if (fwrite(buf, 1, DB_PAGE_SIZE, db->data_file) != DB_PAGE_SIZE) {
        return FALSE;
    }
    fflush(db->data_file);
    return TRUE;
}

bool WritePageToDisk(Database *db, int32 page_num, const DBPage *page)
{
    byte buf[DB_PAGE_SIZE];
    long offset;

    if (db == NULL || db->data_file == NULL || page == NULL) {
        return FALSE;
    }

    offset = (long)page_num * DB_PAGE_SIZE;
    SerializePage(page, buf);

    if (fseek(db->data_file, offset, SEEK_SET) != 0) {
        return FALSE;
    }
    if (fwrite(buf, 1, DB_PAGE_SIZE, db->data_file) != DB_PAGE_SIZE) {
        return FALSE;
    }
    fflush(db->data_file);
    return TRUE;
}

bool ReadPageFromDisk(Database *db, int32 page_num, DBPage *page)
{
    byte buf[DB_PAGE_SIZE];
    long offset;

    if (db == NULL || db->data_file == NULL || page == NULL) {
        return FALSE;
    }

    offset = (long)page_num * DB_PAGE_SIZE;

    if (fseek(db->data_file, offset, SEEK_SET) != 0) {
        return FALSE;
    }
    if (fread(buf, 1, DB_PAGE_SIZE, db->data_file) != DB_PAGE_SIZE) {
        return FALSE;
    }

    DeserializePage(buf, page);
    return TRUE;
}

int32 GetTotalPages(Database *db)
{
    long file_size;

    if (db == NULL || db->data_file == NULL) {
        return 0;
    }

    if (fseek(db->data_file, 0L, SEEK_END) != 0) {
        return 0;
    }
    file_size = ftell(db->data_file);
    if (file_size < 0) {
        return 0;
    }

    return (int32)(file_size / DB_PAGE_SIZE);
}

/* ---- Free Page Management ---- */

int32 AllocatePages(Database *db, uint16 pages_needed)
{
    int32 total;
    uint16 i;
    uint16 j;
    int found;
    int32 first_page;
    DBPage empty_page;

    if (db == NULL || pages_needed == 0) {
        return -1;
    }

    /*
     * Strategy 1: If there are free pages in the list, search for
     * a consecutive run of pages_needed pages.
     */
    if (db->free_list.free_page_count >= pages_needed &&
        db->free_list.free_page_list_len > 0) {

        /* For single page, just pop from the list (LIFO) */
        if (pages_needed == 1) {
            db->free_list.free_page_list_len--;
            first_page = db->free_list.free_pages[db->free_list.free_page_list_len];
            db->free_list.free_page_count--;
            WriteFreeListToDisk(db);
            return first_page;
        }

        /* For multi-page, search for consecutive run */
        for (i = 0; i < db->free_list.free_page_list_len; i++) {
            first_page = db->free_list.free_pages[i];
            found = 1;

            /* Check if next pages_needed-1 consecutive pages are also free */
            for (j = 1; j < pages_needed; j++) {
                uint16 k;
                int page_found = 0;
                for (k = 0; k < db->free_list.free_page_list_len; k++) {
                    if (db->free_list.free_pages[k] == first_page + j) {
                        page_found = 1;
                        break;
                    }
                }
                if (!page_found) {
                    found = 0;
                    break;
                }
            }

            if (found) {
                /* Remove all pages from free list */
                for (j = 0; j < pages_needed; j++) {
                    uint16 k;
                    for (k = 0; k < db->free_list.free_page_list_len; k++) {
                        if (db->free_list.free_pages[k] == first_page + j) {
                            /* Shift remaining entries down */
                            uint16 m;
                            db->free_list.free_page_list_len--;
                            for (m = k; m < db->free_list.free_page_list_len; m++) {
                                db->free_list.free_pages[m] = db->free_list.free_pages[m + 1];
                            }
                            break;
                        }
                    }
                }
                db->free_list.free_page_count -= pages_needed;
                WriteFreeListToDisk(db);
                return first_page;
            }
        }
    }

    /*
     * Strategy 2: If free_page_count > 0 but list is empty,
     * try to refill the list.
     */
    if (db->free_list.free_page_count >= pages_needed &&
        db->free_list.free_page_list_len == 0) {
        UpdateFreePages(db);
        if (db->free_list.free_page_list_len > 0) {
            return AllocatePages(db, pages_needed);
        }
    }

    /*
     * Strategy 3: Append new pages at end of file.
     */
    total = GetTotalPages(db);
    if (total < 2) {
        total = 2;
    }

    /* Write empty pages to extend the file */
    memset(&empty_page, 0, sizeof(DBPage));
    empty_page.status = PS_EMPTY;
    for (i = 0; i < pages_needed; i++) {
        WritePageToDisk(db, total + i, &empty_page);
    }

    return total;
}

void ReleasePages(Database *db, int32 first_page, uint16 pages_count)
{
    if (db == NULL || pages_count == 0) {
        return;
    }

    db->free_list.free_page_count += pages_count;

    if (db->free_list.free_page_list_len < DB_MAX_FREE_PAGES) {
        db->free_list.free_pages[db->free_list.free_page_list_len] = first_page;
        db->free_list.free_page_list_len++;
    }

    WriteFreeListToDisk(db);
}

/* ---- Database Operations ---- */

bool CreateDatabase(const char *name, uint16 record_size)
{
    char dat_name[80];
    char idx_name[80];
    FILE *dat_file;
    DBHeader header;
    DBFreeList free_list;
    byte buf[DB_PAGE_SIZE];

    if (name == NULL || record_size == 0) {
        return FALSE;
    }

    BuildFilename(name, "DAT", dat_name, sizeof(dat_name));
    BuildFilename(name, "IDX", idx_name, sizeof(idx_name));

    /* Initialize header */
    memset(&header, 0, sizeof(DBHeader));
    memcpy(header.signature, DB_SIGNATURE, 4);
    header.version = DB_VERSION;
    header.page_size = DB_PAGE_SIZE;
    header.record_size = record_size;
    header.record_count = 0;
    header.next_record_id = 1;
    header.last_compacted = 0;
    header.journal_pending = FALSE;
    header.index_count = 0;

    /* Initialize empty free list */
    memset(&free_list, 0, sizeof(DBFreeList));

    /* Create .DAT file with header (page 0) and free list (page 1) */
    dat_file = fopen(dat_name, "wb");
    if (dat_file == NULL) {
        return FALSE;
    }

    SerializeHeader(&header, buf);
    if (fwrite(buf, 1, DB_PAGE_SIZE, dat_file) != DB_PAGE_SIZE) {
        fclose(dat_file);
        return FALSE;
    }

    SerializeFreeList(&free_list, buf);
    if (fwrite(buf, 1, DB_PAGE_SIZE, dat_file) != DB_PAGE_SIZE) {
        fclose(dat_file);
        return FALSE;
    }

    fclose(dat_file);

    /* Create .IDX file (primary index) */
    if (!CreateBTree(idx_name)) {
        return FALSE;
    }

    return TRUE;
}

bool OpenDatabase(const char *name, Database *db)
{
    char dat_name[80];
    char idx_name[80];
    byte buf[DB_PAGE_SIZE];

    if (name == NULL || db == NULL) {
        return FALSE;
    }

    memset(db, 0, sizeof(Database));
    StrNCopy(db->name, name, sizeof(db->name));

    BuildFilename(name, "DAT", dat_name, sizeof(dat_name));
    BuildFilename(name, "IDX", idx_name, sizeof(idx_name));

    /* Open .DAT file */
    db->data_file = fopen(dat_name, "r+b");
    if (db->data_file == NULL) {
        return FALSE;
    }

    /* Read page 0: header */
    if (fread(buf, 1, DB_PAGE_SIZE, db->data_file) != DB_PAGE_SIZE) {
        fclose(db->data_file);
        db->data_file = NULL;
        return FALSE;
    }
    DeserializeHeader(buf, &db->header);

    /* Validate signature */
    if (memcmp(db->header.signature, DB_SIGNATURE, 4) != 0) {
        fclose(db->data_file);
        db->data_file = NULL;
        return FALSE;
    }

    /* Read page 1: free list */
    if (fread(buf, 1, DB_PAGE_SIZE, db->data_file) != DB_PAGE_SIZE) {
        fclose(db->data_file);
        db->data_file = NULL;
        return FALSE;
    }
    DeserializeFreeList(buf, &db->free_list);

    /* Open primary index */
    if (!OpenBTree(&db->primary_index, idx_name)) {
        fclose(db->data_file);
        db->data_file = NULL;
        return FALSE;
    }

    db->journal_file = NULL;
    db->is_open = TRUE;

    /* Check for pending journal recovery */
    if (db->header.journal_pending) {
        ReplayJournal(db);
    }

    return TRUE;
}

void CloseDatabase(Database *db)
{
    if (db == NULL || !db->is_open) {
        return;
    }

    /* Write header and free list to disk */
    WriteHeaderToDisk(db);
    WriteFreeListToDisk(db);

    /* Close primary index */
    CloseBTree(&db->primary_index);

    /* Close data file */
    if (db->data_file != NULL) {
        fclose(db->data_file);
        db->data_file = NULL;
    }

    /* Close journal file if open */
    if (db->journal_file != NULL) {
        fclose(db->journal_file);
        db->journal_file = NULL;
    }

    db->is_open = FALSE;
}

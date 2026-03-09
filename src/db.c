/*
 * VDB Database Engine
 *
 * File-based database with fixed-size records, B-Tree indexes,
 * journaling, and maintenance operations. Ported from db.pas.
 */

#include "internal.h"
#include "db.h"

#define JOURNAL_ENTRY_SIZE 518

static int16 minInt(int16 a, int16 b) {
    return a < b ? a : b;
}

static long getFileSize(FILE *fp) {
    long cur, size;
    cur = ftell(fp);
    fseek(fp, 0L, SEEK_END);
    size = ftell(fp);
    fseek(fp, cur, SEEK_SET);
    return size;
}

/* Low-level page I/O */

int ReadPage(Database *db, int32 page_num, DBPage *page) {
    byte buf[DB_PAGE_SIZE];

    if (!db->is_open)
        return 0;

    fseek(db->data_file, (long)page_num * DB_PAGE_SIZE, SEEK_SET);
    if (fread(buf, 1, DB_PAGE_SIZE, db->data_file) != DB_PAGE_SIZE)
        return 0;

    page->id = (int32)GET_LE32(buf);
    page->status = buf[4];
    page->reserved = buf[5];
    memcpy(page->data, buf + 6, DB_PAGE_DATA_SIZE);

    return 1;
}

int WritePage(Database *db, int32 page_num, const DBPage *page) {
    byte buf[DB_PAGE_SIZE];

    if (!db->is_open)
        return 0;

    memset(buf, 0, DB_PAGE_SIZE);
    PUT_LE32(buf, page->id);
    buf[4] = page->status;
    buf[5] = page->reserved;
    memcpy(buf + 6, page->data, DB_PAGE_DATA_SIZE);

    fseek(db->data_file, (long)page_num * DB_PAGE_SIZE, SEEK_SET);
    if (fwrite(buf, 1, DB_PAGE_SIZE, db->data_file) != DB_PAGE_SIZE)
        return 0;
    fflush(db->data_file);

    return 1;
}

int ReadHeader(Database *db) {
    byte page[DB_PAGE_SIZE];
    int i;

    if (!db->is_open)
        return 0;

    fseek(db->data_file, 0L, SEEK_SET);
    if (fread(page, 1, DB_PAGE_SIZE, db->data_file) != DB_PAGE_SIZE)
        return 0;

    memcpy(db->header.signature, page, 8);
    db->header.version = GET_LE16(page + 8);
    db->header.page_size = GET_LE16(page + 10);
    db->header.record_size = GET_LE16(page + 12);
    db->header.record_count = (int32)GET_LE32(page + 14);
    db->header.next_record_id = (int32)GET_LE32(page + 18);
    db->header.last_compacted = (int32)GET_LE32(page + 22);
    db->header.journal_pending = page[26];
    db->header.index_count = page[27];
    memcpy(db->header.reserved, page + 28, 4);

    /* Read index info array */
    for (i = 0; i < DB_MAX_INDEXES; i++) {
        int off = 32 + i * 32;
        memcpy(db->header.indexes[i].field_name, page + off, 30);
        db->header.indexes[i].index_type = page[off + 30];
        db->header.indexes[i].index_number = page[off + 31];
    }

    /* Validate signature */
    if (memcmp(db->header.signature, DB_SIGNATURE, 8) != 0)
        return 0;
    if (db->header.version != DB_VERSION)
        return 0;

    return 1;
}

int WriteHeader(Database *db) {
    byte page[DB_PAGE_SIZE];
    int i;

    if (!db->is_open)
        return 0;

    memset(page, 0, DB_PAGE_SIZE);

    memcpy(page, db->header.signature, 8);
    PUT_LE16(page + 8, db->header.version);
    PUT_LE16(page + 10, db->header.page_size);
    PUT_LE16(page + 12, db->header.record_size);
    PUT_LE32(page + 14, db->header.record_count);
    PUT_LE32(page + 18, db->header.next_record_id);
    PUT_LE32(page + 22, db->header.last_compacted);
    page[26] = db->header.journal_pending;
    page[27] = db->header.index_count;
    memcpy(page + 28, db->header.reserved, 4);

    for (i = 0; i < DB_MAX_INDEXES; i++) {
        int off = 32 + i * 32;
        memcpy(page + off, db->header.indexes[i].field_name, 30);
        page[off + 30] = db->header.indexes[i].index_type;
        page[off + 31] = db->header.indexes[i].index_number;
    }

    fseek(db->data_file, 0L, SEEK_SET);
    if (fwrite(page, 1, DB_PAGE_SIZE, db->data_file) != DB_PAGE_SIZE)
        return 0;
    fflush(db->data_file);

    return 1;
}

int ReadFreeList(Database *db) {
    byte page[DB_PAGE_SIZE];
    int i;

    if (!db->is_open)
        return 0;

    fseek(db->data_file, (long)DB_PAGE_SIZE, SEEK_SET);
    if (fread(page, 1, DB_PAGE_SIZE, db->data_file) != DB_PAGE_SIZE)
        return 0;

    db->free_list.free_page_count = GET_LE16(page);
    db->free_list.free_page_list_len = GET_LE16(page + 2);

    for (i = 0; i < DB_MAX_FREE_PAGES; i++)
        db->free_list.free_pages[i] = (int32)GET_LE32(page + 4 + i * 4);

    return 1;
}

int WriteFreeList(Database *db) {
    byte page[DB_PAGE_SIZE];
    int i;

    if (!db->is_open)
        return 0;

    memset(page, 0, DB_PAGE_SIZE);
    PUT_LE16(page, db->free_list.free_page_count);
    PUT_LE16(page + 2, db->free_list.free_page_list_len);

    for (i = 0; i < DB_MAX_FREE_PAGES; i++)
        PUT_LE32(page + 4 + i * 4, db->free_list.free_pages[i]);

    fseek(db->data_file, (long)DB_PAGE_SIZE, SEEK_SET);
    if (fwrite(page, 1, DB_PAGE_SIZE, db->data_file) != DB_PAGE_SIZE)
        return 0;
    fflush(db->data_file);

    return 1;
}

int16 CalculatePagesNeeded(uint16 record_size) {
    return (int16)((record_size + DB_PAGE_DATA_SIZE - 1) / DB_PAGE_DATA_SIZE);
}

int ReadRecord(Database *db, int32 first_page, byte *data) {
    DBPage page;
    int16 pages_needed, i, bytes_to_copy;
    int offset;
    int32 record_id;

    if (!db->is_open)
        return 0;

    pages_needed = CalculatePagesNeeded(db->header.record_size);

    if (!ReadPage(db, first_page, &page))
        return 0;

    record_id = page.id;

    if (page.status != PS_ACTIVE)
        return 0;

    bytes_to_copy = db->header.record_size <= DB_PAGE_DATA_SIZE
                  ? (int16)db->header.record_size
                  : DB_PAGE_DATA_SIZE;
    memcpy(data, page.data, bytes_to_copy);
    offset = bytes_to_copy;

    for (i = 1; i < pages_needed; i++) {
        if (!ReadPage(db, first_page + i, &page))
            return 0;
        if (page.id != record_id)
            return 0;
        if (page.status != PS_CONTINUATION)
            return 0;

        bytes_to_copy = (int16)(db->header.record_size - offset);
        if (bytes_to_copy > DB_PAGE_DATA_SIZE)
            bytes_to_copy = DB_PAGE_DATA_SIZE;

        memcpy(data + offset, page.data, bytes_to_copy);
        offset += bytes_to_copy;
    }

    return 1;
}

int WriteRecord(Database *db, int32 first_page, int32 record_id, const byte *data) {
    DBPage page;
    int16 pages_needed, i, bytes_to_copy;
    int offset;

    if (!db->is_open)
        return 0;

    pages_needed = CalculatePagesNeeded(db->header.record_size);
    offset = 0;

    for (i = 0; i < pages_needed; i++) {
        memset(&page, 0, sizeof(page));
        page.id = record_id;
        page.status = (i == 0) ? PS_ACTIVE : PS_CONTINUATION;

        bytes_to_copy = (int16)(db->header.record_size - offset);
        if (bytes_to_copy > DB_PAGE_DATA_SIZE)
            bytes_to_copy = DB_PAGE_DATA_SIZE;

        memcpy(page.data, data + offset, bytes_to_copy);

        if (!WritePage(db, first_page + i, &page))
            return 0;

        offset += bytes_to_copy;
    }

    return 1;
}

/* Free space management */

int FindConsecutiveFreePages(Database *db, int16 count, int32 *first_page) {
    int i, j;
    int16 consecutive;
    int32 start_page;

    if (count <= 0)
        return 0;

    if (count == 1) {
        if (db->free_list.free_page_list_len > 0) {
            *first_page = db->free_list.free_pages[0];
            return 1;
        }
        return 0;
    }

    for (i = 0; i < (int)db->free_list.free_page_list_len; i++) {
        start_page = db->free_list.free_pages[i];
        consecutive = 1;

        for (j = i + 1; j < (int)db->free_list.free_page_list_len; j++) {
            if (db->free_list.free_pages[j] == start_page + consecutive) {
                consecutive++;
                if (consecutive == count) {
                    *first_page = start_page;
                    return 1;
                }
            } else {
                break;
            }
        }
    }

    return 0;
}

int AllocatePages(Database *db, int16 count, int32 *first_page) {
    long file_size_pages;
    int i, j;

    if (count <= 0)
        return 0;

    /* Try free list first */
    if (db->free_list.free_page_count >= (uint16)count && db->free_list.free_page_list_len > 0) {
        if (FindConsecutiveFreePages(db, count, first_page)) {
            for (i = 0; i < (int)db->free_list.free_page_list_len; i++) {
                if (db->free_list.free_pages[i] == *first_page) {
                    for (j = i; j < (int)db->free_list.free_page_list_len - count; j++)
                        db->free_list.free_pages[j] = db->free_list.free_pages[j + count];
                    db->free_list.free_page_list_len -= (uint16)count;
                    db->free_list.free_page_count -= (uint16)count;
                    WriteFreeList(db);
                    return 1;
                }
            }
        }
    }

    /* Refresh free list if needed */
    if (db->free_list.free_page_list_len == 0 && db->free_list.free_page_count > 0) {
        if (UpdateFreePages(db)) {
            if (FindConsecutiveFreePages(db, count, first_page)) {
                for (i = 0; i < (int)db->free_list.free_page_list_len; i++) {
                    if (db->free_list.free_pages[i] == *first_page) {
                        for (j = i; j < (int)db->free_list.free_page_list_len - count; j++)
                            db->free_list.free_pages[j] = db->free_list.free_pages[j + count];
                        db->free_list.free_page_list_len -= (uint16)count;
                        db->free_list.free_page_count -= (uint16)count;
                        WriteFreeList(db);
                        return 1;
                    }
                }
            }
        }
    }

    /* Append to end of file */
    file_size_pages = getFileSize(db->data_file) / DB_PAGE_SIZE;
    *first_page = (int32)file_size_pages;
    return 1;
}

int FreePages(Database *db, int32 first_page, int16 count) {
    DBPage page;
    int i;

    if (count <= 0)
        return 0;

    for (i = 0; i < count; i++) {
        if (ReadPage(db, first_page + i, &page)) {
            page.status = PS_EMPTY;
            if (!WritePage(db, first_page + i, &page))
                return 0;
        }
    }

    db->free_list.free_page_count += (uint16)count;

    if (db->free_list.free_page_list_len < DB_MAX_FREE_PAGES) {
        db->free_list.free_pages[db->free_list.free_page_list_len] = first_page;
        db->free_list.free_page_list_len++;
    }

    return WriteFreeList(db);
}

int UpdateFreePages(Database *db) {
    DBPage page;
    int32 page_num;
    long file_size_pages;
    uint16 free_count;

    if (!db->is_open)
        return 0;

    file_size_pages = getFileSize(db->data_file) / DB_PAGE_SIZE;
    free_count = 0;
    db->free_list.free_page_list_len = 0;

    for (page_num = 2; page_num < (int32)file_size_pages; page_num++) {
        if (ReadPage(db, page_num, &page)) {
            if (page.status == PS_EMPTY) {
                free_count++;
                if (db->free_list.free_page_list_len < DB_MAX_FREE_PAGES) {
                    db->free_list.free_pages[db->free_list.free_page_list_len] = page_num;
                    db->free_list.free_page_list_len++;
                }
            }
        }
    }

    db->free_list.free_page_count = free_count;
    return WriteFreeList(db);
}

/* Index management */

void GetIndexFileName(const char *base_name, int16 index_num, char *out, int16 out_size) {
    size_t len;
    if (index_num == -1) {
        if ((int)strlen(base_name) + 5 < out_size) {
            strcpy(out, base_name);
            strcat(out, ".idx");
        } else {
            out[0] = '\0';
        }
    } else if (index_num >= 0 && index_num <= 14) {
        if ((int)strlen(base_name) + 5 < out_size) {
            strcpy(out, base_name);
            if (index_num < 10) {
                strcat(out, ".i0");
            } else {
                strcat(out, ".i1");
                index_num -= 10;
            }
            len = strlen(out);
            out[len] = (char)('0' + index_num);
            out[len + 1] = '\0';
        } else {
            out[0] = '\0';
        }
    } else {
        out[0] = '\0';
    }
}

int OpenIndexFile(BTree *tree, const char *base_name, int16 index_num) {
    char filename[320];
    GetIndexFileName(base_name, index_num, filename, sizeof(filename));
    if (filename[0] == '\0')
        return 0;
    return OpenBTree(tree, filename);
}

int CreateIndexFile(const char *base_name, int16 index_num) {
    char filename[320];
    GetIndexFileName(base_name, index_num, filename, sizeof(filename));
    if (filename[0] == '\0')
        return 0;
    return CreateBTree(filename);
}

int32 GenerateIndexKey(byte index_type, const byte *value) {
    int32 result;
    if (index_type == IT_ID) {
        memcpy(&result, value, 4);
        result = (int32)LE32TOH((uint32)result);
        return result;
    } else {
        /* itString: first byte is length */
        byte len = value[0];
        if (len > 63) len = 63;
        return StringKey((const char *)(value + 1));
    }
}

int InsertIntoIndex(BTree *tree, int32 key, int32 value) {
    return BTreeInsert(tree, key, value);
}

int DeleteFromIndex(BTree *tree, int32 key, int32 value) {
    return BTreeDeleteValue(tree, key, value);
}

int FindInIndex(BTree *tree, int32 key, int32 *values, int16 max_values, int16 *count) {
    return BTreeFind(tree, key, values, max_values, count);
}

/* Journal/transaction system */

uint16 CalculateJournalChecksum(const DBJournalEntry *entry) {
    byte buffer[516];
    buffer[0] = entry->operation;
    PUT_LE32(buffer + 1, entry->page_num);
    PUT_LE32(buffer + 5, entry->record_id);
    memcpy(buffer + 9, entry->data, 507);
    return CRC16(buffer, 516);
}

int WriteJournalEntry(Database *db, const DBJournalEntry *entry) {
    byte buf[JOURNAL_ENTRY_SIZE];
    uint16 checksum;

    if (!db->is_open)
        return 0;

    checksum = CalculateJournalChecksum(entry);

    memset(buf, 0, JOURNAL_ENTRY_SIZE);
    buf[0] = entry->operation;
    PUT_LE32(buf + 1, entry->page_num);
    PUT_LE32(buf + 5, entry->record_id);
    memcpy(buf + 9, entry->data, 507);
    PUT_LE16(buf + 516, checksum);

    fseek(db->journal_file, 0L, SEEK_END);
    if (fwrite(buf, 1, JOURNAL_ENTRY_SIZE, db->journal_file) != JOURNAL_ENTRY_SIZE)
        return 0;
    fflush(db->journal_file);

    return 1;
}

int ReadJournalEntry(Database *db, int16 entry_num, DBJournalEntry *entry) {
    byte buf[JOURNAL_ENTRY_SIZE];

    if (!db->is_open)
        return 0;

    fseek(db->journal_file, (long)entry_num * JOURNAL_ENTRY_SIZE, SEEK_SET);
    if (fread(buf, 1, JOURNAL_ENTRY_SIZE, db->journal_file) != JOURNAL_ENTRY_SIZE)
        return 0;

    entry->operation = buf[0];
    entry->page_num = (int32)GET_LE32(buf + 1);
    entry->record_id = (int32)GET_LE32(buf + 5);
    memcpy(entry->data, buf + 9, 507);
    entry->checksum = GET_LE16(buf + 516);

    return 1;
}

int BeginTransaction(Database *db) {
    if (!db->is_open)
        return 0;
    db->header.journal_pending = 1;
    return WriteHeader(db);
}

int CommitTransaction(Database *db) {
    if (!db->is_open)
        return 0;

    db->header.journal_pending = 0;
    if (!WriteHeader(db))
        return 0;

    /* Truncate journal file */
    return ftruncate_(db->journal_file, 0L);
}

int RollbackTransaction(Database *db) {
    if (!db->is_open)
        return 0;

    db->header.journal_pending = 0;
    if (!WriteHeader(db))
        return 0;

    return ftruncate_(db->journal_file, 0L);
}

int ReplayJournal(Database *db) {
    long journal_size;
    int16 entry_count, i;
    DBJournalEntry entry;
    uint16 checksum;
    DBPage page;

    if (!db->is_open)
        return 0;

    journal_size = getFileSize(db->journal_file);
    entry_count = (int16)(journal_size / JOURNAL_ENTRY_SIZE);

    for (i = 0; i < entry_count; i++) {
        if (ReadJournalEntry(db, i, &entry)) {
            checksum = CalculateJournalChecksum(&entry);
            if (checksum == entry.checksum) {
                switch (entry.operation) {
                case JO_UPDATE:
                case JO_ADD:
                    memset(&page, 0, sizeof(page));
                    page.id = entry.record_id;
                    page.status = PS_ACTIVE;
                    memcpy(page.data, entry.data, DB_PAGE_DATA_SIZE);
                    WritePage(db, entry.page_num, &page);
                    break;
                case JO_DELETE:
                    if (ReadPage(db, entry.page_num, &page)) {
                        page.status = PS_EMPTY;
                        WritePage(db, entry.page_num, &page);
                    }
                    break;
                }
            }
        }
    }

    if (!RebuildAllIndexes(db))
        return 0;

    db->header.journal_pending = 0;
    if (!WriteHeader(db))
        return 0;

    return ftruncate_(db->journal_file, 0L);
}

/* Database operations */

int CreateDatabase(const char *name, uint16 record_size) {
    FILE *f, *jf;
    byte page[DB_PAGE_SIZE];
    char path[320];
    int i;

    if (record_size == 0)
        return 0;

    /* Create data file */
    sprintf(path, "%s.dat", name);
    f = fopen(path, "wb");
    if (!f)
        return 0;

    /* Write header page */
    memset(page, 0, DB_PAGE_SIZE);
    memcpy(page, DB_SIGNATURE, 8);
    PUT_LE16(page + 8, DB_VERSION);
    PUT_LE16(page + 10, DB_PAGE_SIZE);
    PUT_LE16(page + 12, record_size);
    PUT_LE32(page + 14, 0);           /* record_count */
    PUT_LE32(page + 18, 1);           /* next_record_id */
    PUT_LE32(page + 22, 0);           /* last_compacted */
    page[26] = 0;                      /* journal_pending */
    page[27] = 0;                      /* index_count */

    if (fwrite(page, 1, DB_PAGE_SIZE, f) != DB_PAGE_SIZE) {
        fclose(f);
        return 0;
    }

    /* Write free list page */
    memset(page, 0, DB_PAGE_SIZE);
    if (fwrite(page, 1, DB_PAGE_SIZE, f) != DB_PAGE_SIZE) {
        fclose(f);
        return 0;
    }

    fclose(f);

    /* Create primary index */
    if (!CreateIndexFile(name, -1))
        return 0;

    /* Create journal file */
    sprintf(path, "%s.jnl", name);
    jf = fopen(path, "wb");
    if (!jf)
        return 0;
    fclose(jf);

    (void)i;
    return 1;
}

int OpenDatabase(const char *name, Database *db) {
    char path[320];
    size_t len;

    db->primary_index = NULL;
    db->is_open = 0;

    /* Copy name */
    len = strlen(name);
    if (len >= sizeof(db->name))
        len = sizeof(db->name) - 1;
    memcpy(db->name, name, len);
    db->name[len] = '\0';

    /* Open data file */
    sprintf(path, "%s.dat", name);
    db->data_file = fopen(path, "r+b");
    if (!db->data_file)
        return 0;

    db->is_open = 1;

    if (!ReadHeader(db)) {
        fclose(db->data_file);
        db->is_open = 0;
        return 0;
    }

    /* Open journal file */
    sprintf(path, "%s.jnl", name);
    db->journal_file = fopen(path, "r+b");
    if (!db->journal_file) {
        /* Try creating it */
        db->journal_file = fopen(path, "w+b");
        if (!db->journal_file) {
            fclose(db->data_file);
            db->is_open = 0;
            return 0;
        }
    }

    /* Journal recovery */
    if (db->header.journal_pending) {
        if (!ReplayJournal(db)) {
            fclose(db->journal_file);
            fclose(db->data_file);
            db->is_open = 0;
            return 0;
        }
    }

    if (!ReadFreeList(db)) {
        fclose(db->journal_file);
        fclose(db->data_file);
        db->is_open = 0;
        return 0;
    }

    /* Allocate and open primary index */
    db->primary_index = (BTree *)malloc(sizeof(BTree));
    if (!db->primary_index) {
        fclose(db->journal_file);
        fclose(db->data_file);
        db->is_open = 0;
        return 0;
    }

    if (!OpenIndexFile(db->primary_index, name, -1)) {
        free(db->primary_index);
        db->primary_index = NULL;
        fclose(db->journal_file);
        fclose(db->data_file);
        db->is_open = 0;
        return 0;
    }

    return 1;
}

void CloseDatabase(Database *db) {
    if (!db->is_open)
        return;

    WriteHeader(db);
    WriteFreeList(db);

    if (db->primary_index) {
        CloseBTree(db->primary_index);
        free(db->primary_index);
        db->primary_index = NULL;
    }

    fclose(db->journal_file);
    fclose(db->data_file);

    db->is_open = 0;
}

/* Record operations */

int AddRecord(Database *db, const byte *data, int32 *record_id) {
    int16 pages_needed, i;
    int32 first_page;
    DBJournalEntry entry;

    if (!db->is_open)
        return 0;

    if (!BeginTransaction(db))
        return 0;

    *record_id = db->header.next_record_id;
    db->header.next_record_id++;

    pages_needed = CalculatePagesNeeded(db->header.record_size);

    if (!AllocatePages(db, pages_needed, &first_page)) {
        RollbackTransaction(db);
        return 0;
    }

    /* Journal entries */
    for (i = 0; i < pages_needed; i++) {
        memset(&entry, 0, sizeof(entry));
        entry.operation = JO_ADD;
        entry.page_num = first_page + i;
        entry.record_id = *record_id;

        if (i == 0) {
            int16 n = db->header.record_size <= DB_PAGE_DATA_SIZE
                    ? (int16)db->header.record_size : DB_PAGE_DATA_SIZE;
            memcpy(entry.data, data, n);
        } else {
            int16 n = minInt(DB_PAGE_DATA_SIZE,
                             (int16)(db->header.record_size - i * DB_PAGE_DATA_SIZE));
            memcpy(entry.data, data + i * DB_PAGE_DATA_SIZE, n);
        }

        if (!WriteJournalEntry(db, &entry)) {
            RollbackTransaction(db);
            return 0;
        }
    }

    if (!WriteRecord(db, first_page, *record_id, data)) {
        RollbackTransaction(db);
        return 0;
    }

    if (!InsertIntoIndex(db->primary_index, *record_id, first_page)) {
        RollbackTransaction(db);
        return 0;
    }

    db->header.record_count++;

    if (!CommitTransaction(db))
        return 0;

    return 1;
}

int FindRecordByID(Database *db, int32 id, byte *data) {
    int32 values[10];
    int16 count;

    if (!db->is_open)
        return 0;

    if (!FindInIndex(db->primary_index, id, values, 10, &count))
        return 0;

    if (count == 0)
        return 0;

    return ReadRecord(db, values[0], data);
}

int FindRecordByString(Database *db, const char *field_name, const char *value,
                       byte *data, int32 *record_id) {
    /* Not yet implemented */
    (void)db; (void)field_name; (void)value; (void)data; (void)record_id;
    return 0;
}

int UpdateRecord(Database *db, int32 id, const byte *data) {
    byte *old_data;
    int32 values[10];
    int16 count, pages_needed, i;
    int32 first_page;
    DBJournalEntry entry;

    if (!db->is_open)
        return 0;

    old_data = (byte *)malloc(db->header.record_size);
    if (!old_data)
        return 0;

    if (!FindRecordByID(db, id, old_data)) {
        free(old_data);
        return 0;
    }
    free(old_data);

    if (!FindInIndex(db->primary_index, id, values, 10, &count))
        return 0;
    if (count == 0)
        return 0;

    first_page = values[0];

    if (!BeginTransaction(db))
        return 0;

    pages_needed = CalculatePagesNeeded(db->header.record_size);

    for (i = 0; i < pages_needed; i++) {
        memset(&entry, 0, sizeof(entry));
        entry.operation = JO_UPDATE;
        entry.page_num = first_page + i;
        entry.record_id = id;

        if (i == 0) {
            int16 n = db->header.record_size <= DB_PAGE_DATA_SIZE
                    ? (int16)db->header.record_size : DB_PAGE_DATA_SIZE;
            memcpy(entry.data, data, n);
        } else {
            int16 n = minInt(DB_PAGE_DATA_SIZE,
                             (int16)(db->header.record_size - i * DB_PAGE_DATA_SIZE));
            memcpy(entry.data, data + i * DB_PAGE_DATA_SIZE, n);
        }

        if (!WriteJournalEntry(db, &entry)) {
            RollbackTransaction(db);
            return 0;
        }
    }

    if (!WriteRecord(db, first_page, id, data)) {
        RollbackTransaction(db);
        return 0;
    }

    if (!CommitTransaction(db))
        return 0;

    return 1;
}

int DeleteRecord(Database *db, int32 id) {
    byte *data;
    int32 values[10];
    int16 count, pages_needed;
    int32 first_page;
    DBJournalEntry entry;

    if (!db->is_open)
        return 0;

    data = (byte *)malloc(db->header.record_size);
    if (!data)
        return 0;

    if (!FindRecordByID(db, id, data)) {
        free(data);
        return 0;
    }
    free(data);

    if (!FindInIndex(db->primary_index, id, values, 10, &count))
        return 0;
    if (count == 0)
        return 0;

    first_page = values[0];

    if (!BeginTransaction(db))
        return 0;

    pages_needed = CalculatePagesNeeded(db->header.record_size);

    memset(&entry, 0, sizeof(entry));
    entry.operation = JO_DELETE;
    entry.page_num = first_page;
    entry.record_id = id;

    if (!WriteJournalEntry(db, &entry)) {
        RollbackTransaction(db);
        return 0;
    }

    if (!FreePages(db, first_page, pages_needed)) {
        RollbackTransaction(db);
        return 0;
    }

    if (!DeleteFromIndex(db->primary_index, id, first_page)) {
        RollbackTransaction(db);
        return 0;
    }

    db->header.record_count--;

    if (!CommitTransaction(db))
        return 0;

    return 1;
}

/* Index maintenance */

int AddIndex(Database *db, const char *field_name, byte index_type) {
    int16 index_num;
    int i;
    size_t len;

    if (!db->is_open)
        return 0;
    if (db->header.index_count >= DB_MAX_INDEXES)
        return 0;

    len = strlen(field_name);
    if (len > 29)
        return 0;

    /* Find next available index number */
    index_num = 0;
    for (i = 0; i < (int)db->header.index_count; i++) {
        if (db->header.indexes[i].index_number >= (byte)index_num)
            index_num = db->header.indexes[i].index_number + 1;
    }

    if (index_num >= DB_MAX_INDEXES)
        return 0;

    if (!CreateIndexFile(db->name, index_num))
        return 0;

    memset(db->header.indexes[db->header.index_count].field_name, 0, 30);
    memcpy(db->header.indexes[db->header.index_count].field_name, field_name, len);
    db->header.indexes[db->header.index_count].index_type = index_type;
    db->header.indexes[db->header.index_count].index_number = (byte)index_num;
    db->header.index_count++;

    if (!WriteHeader(db)) {
        db->header.index_count--;
        return 0;
    }

    return 1;
}

int RebuildIndex(Database *db, int16 index_number) {
    DBPage page;
    int32 page_num;
    long file_size;
    BTree *tree;
    int is_secondary = 0;

    if (!db->is_open)
        return 0;

    if (index_number == -1) {
        tree = db->primary_index;
    } else if (index_number >= 0 && index_number < DB_MAX_INDEXES) {
        tree = (BTree *)malloc(sizeof(BTree));
        if (!tree)
            return 0;
        if (!OpenIndexFile(tree, db->name, index_number)) {
            free(tree);
            return 0;
        }
        is_secondary = 1;
    } else {
        return 0;
    }

    /* Close and recreate */
    CloseBTree(tree);
    if (!CreateIndexFile(db->name, index_number)) {
        if (is_secondary) free(tree);
        return 0;
    }
    if (!OpenIndexFile(tree, db->name, index_number)) {
        if (is_secondary) free(tree);
        return 0;
    }

    file_size = getFileSize(db->data_file) / DB_PAGE_SIZE;

    for (page_num = 2; page_num < (int32)file_size; page_num++) {
        if (ReadPage(db, page_num, &page)) {
            if (page.status == PS_ACTIVE) {
                if (index_number == -1) {
                    BTreeInsert(tree, page.id, page_num);
                } else {
                    BTreeInsert(tree, page.id, page.id);
                }
            }
        }
    }

    if (is_secondary) {
        CloseBTree(tree);
        free(tree);
    }

    return 1;
}

int RebuildAllIndexes(Database *db) {
    int i;

    if (!RebuildIndex(db, -1))
        return 0;

    for (i = 0; i < (int)db->header.index_count; i++) {
        if (!RebuildIndex(db, (int16)db->header.indexes[i].index_number))
            return 0;
    }

    return 1;
}

/* Maintenance */

int CompactDatabase(Database *db) {
    DBPage page;
    int32 page_num, new_page_num;
    long file_size;
    byte *data;
    int16 pages_needed;

    if (!db->is_open)
        return 0;

    if (!BeginTransaction(db))
        return 0;

    data = (byte *)malloc(db->header.record_size);
    if (!data) {
        RollbackTransaction(db);
        return 0;
    }

    file_size = getFileSize(db->data_file) / DB_PAGE_SIZE;
    new_page_num = 2;
    pages_needed = CalculatePagesNeeded(db->header.record_size);

    page_num = 2;
    while (page_num < (int32)file_size) {
        if (ReadPage(db, page_num, &page) && page.status == PS_ACTIVE) {
            if (ReadRecord(db, page_num, data)) {
                WriteRecord(db, new_page_num, page.id, data);
                new_page_num += pages_needed;
            }
            page_num += pages_needed;
        } else {
            page_num++;
        }
    }

    free(data);

    /* Truncate file */
    ftruncate_(db->data_file, (long)new_page_num * DB_PAGE_SIZE);

    if (!RebuildAllIndexes(db)) {
        RollbackTransaction(db);
        return 0;
    }

    db->free_list.free_page_count = 0;
    db->free_list.free_page_list_len = 0;
    WriteFreeList(db);

    db->header.last_compacted = 0;

    if (!CommitTransaction(db))
        return 0;

    return 1;
}

int ValidateDatabase(Database *db) {
    int valid = 1;
    DBPage page;
    int32 page_num;
    long file_size;
    int32 active_count = 0, free_count = 0;
    int32 values[10];
    int16 count;

    if (!db->is_open)
        return 0;

    if (memcmp(db->header.signature, DB_SIGNATURE, 8) != 0)
        valid = 0;

    if (db->header.version != DB_VERSION)
        valid = 0;

    file_size = getFileSize(db->data_file) / DB_PAGE_SIZE;

    for (page_num = 2; page_num < (int32)file_size; page_num++) {
        if (ReadPage(db, page_num, &page)) {
            if (page.status == PS_ACTIVE)
                active_count++;
            else if (page.status == PS_EMPTY)
                free_count++;
        }
    }

    if (active_count != db->header.record_count * CalculatePagesNeeded(db->header.record_size))
        valid = 0;

    if ((uint16)free_count != db->free_list.free_page_count)
        valid = 0;

    /* Verify primary index */
    for (page_num = 2; page_num < (int32)file_size; page_num++) {
        if (ReadPage(db, page_num, &page)) {
            if (page.status == PS_ACTIVE) {
                if (!FindInIndex(db->primary_index, page.id, values, 10, &count) || count == 0)
                    valid = 0;
            }
        }
    }

    return valid;
}

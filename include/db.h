/*
 * MIT License
 *
 * Copyright 2025, Andrew C. Young <andrew@vaelen.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef DB_H
#define DB_H

#include <stdio.h>

#include "vdbtypes.h"
#include "btree.h"
#include "hash.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Constants */
#define DB_PAGE_SIZE      512
#define DB_PAGE_DATA_SIZE 506
#define DB_SIGNATURE      "RETRODB\0"
#define DB_VERSION        1
#define DB_MAX_INDEXES    15
#define DB_MAX_FREE_PAGES 127

/* Page status */
#define PS_EMPTY        0
#define PS_ACTIVE       1
#define PS_CONTINUATION 2

/* Index type */
#define IT_ID     0
#define IT_STRING 1

/* Journal operation */
#define JO_NONE   0
#define JO_UPDATE 1
#define JO_DELETE 2
#define JO_ADD    3

/* Index info - 32 bytes on disk */
typedef struct {
    char field_name[30];
    byte index_type;
    byte index_number;
} DBIndexInfo;

/* Database header - 512 bytes (page 0) */
typedef struct {
    char        signature[8];
    uint16      version;
    uint16      page_size;
    uint16      record_size;
    int32       record_count;
    int32       next_record_id;
    int32       last_compacted;
    bool        journal_pending;
    byte        index_count;
    byte        reserved[4];
    DBIndexInfo indexes[DB_MAX_INDEXES];
} DBHeader;

/* Free page list - 512 bytes (page 1) */
typedef struct {
    uint16 free_page_count;
    uint16 free_page_list_len;
    int32  free_pages[DB_MAX_FREE_PAGES];
} DBFreeList;

/* Database page - 512 bytes */
typedef struct {
    int32 id;
    byte  status;
    byte  reserved;
    byte  data[DB_PAGE_DATA_SIZE];
} DBPage;

/* Journal entry - 518 bytes */
typedef struct {
    byte   operation;
    int32  page_num;
    int32  record_id;
    byte   data[507];
    uint16 checksum;
} DBJournalEntry;

/* Database handle */
typedef struct {
    char       name[64];
    DBHeader   header;
    DBFreeList free_list;
    FILE      *data_file;
    FILE      *journal_file;
    BTree     *primary_index;
    bool       is_open;
} Database;

/* Low-level file operations */
bool  ReadPage(Database *db, int32 page_num, DBPage *page);
bool  WritePage(Database *db, int32 page_num, const DBPage *page);
bool  ReadHeader(Database *db);
bool  WriteHeader(Database *db);
bool  ReadFreeList(Database *db);
bool  WriteFreeList(Database *db);
int16 CalculatePagesNeeded(uint16 record_size);
bool  ReadRecord(Database *db, int32 first_page, byte *data);
bool  WriteRecord(Database *db, int32 first_page, int32 record_id, const byte *data);

/* Free space management */
bool FindConsecutiveFreePages(Database *db, int16 count, int32 *first_page);
bool AllocatePages(Database *db, int16 count, int32 *first_page);
bool FreePages(Database *db, int32 first_page, int16 count);
bool UpdateFreePages(Database *db);

/* Index management */
void  GetIndexFileName(const char *base_name, int16 index_num, char *out, int16 out_size);
bool  OpenIndexFile(BTree *tree, const char *base_name, int16 index_num);
bool  CreateIndexFile(const char *base_name, int16 index_num);
int32 GenerateIndexKey(byte index_type, const byte *value);
bool  InsertIntoIndex(BTree *tree, int32 key, int32 value);
bool  DeleteFromIndex(BTree *tree, int32 key, int32 value);
bool  FindInIndex(BTree *tree, int32 key, int32 *values, int16 max_values, int16 *count);

/* Journal/transaction system */
uint16 CalculateJournalChecksum(const DBJournalEntry *entry);
bool   WriteJournalEntry(Database *db, const DBJournalEntry *entry);
bool   ReadJournalEntry(Database *db, int16 entry_num, DBJournalEntry *entry);
bool   BeginTransaction(Database *db);
bool   CommitTransaction(Database *db);
bool   RollbackTransaction(Database *db);
bool   ReplayJournal(Database *db);

/* Database operations */
bool CreateDatabase(const char *name, uint16 record_size);
bool OpenDatabase(const char *name, Database *db);
void CloseDatabase(Database *db);

/* Record operations */
bool AddRecord(Database *db, const byte *data, int32 *record_id);
bool FindRecordByID(Database *db, int32 id, byte *data);
bool FindRecordByString(Database *db, const char *field_name, const char *value, byte *data, int32 *record_id);
bool UpdateRecord(Database *db, int32 id, const byte *data);
bool DeleteRecord(Database *db, int32 id);

/* Index maintenance */
bool AddIndex(Database *db, const char *field_name, byte index_type);
bool RebuildIndex(Database *db, int16 index_number);
bool RebuildAllIndexes(Database *db);

/* Maintenance */
bool CompactDatabase(Database *db);
bool ValidateDatabase(Database *db);

#ifdef __cplusplus
}
#endif

#endif /* DB_H */

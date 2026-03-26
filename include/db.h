/* Copyright 2026, Andrew C. Young <andrew@vaelen.org> */
/* SPDX-License-Identifier: MIT */

/*
 * db.h - Page-based database library for vDB
 *
 * Provides a page-based database with journaling and B-Tree indexing.
 * Data is stored in fixed-size 512-byte pages. Records may span
 * multiple consecutive pages. A primary index maps Record IDs to
 * physical page numbers. Secondary indexes map field values to
 * Record IDs via CRC-16 hashing.
 *
 * All multi-byte values are stored in little-endian byte order.
 * On-disk structures use explicit byte-by-byte serialization to
 * avoid compiler alignment and padding differences.
 *
 * Depends on: util (types, strings), crc (CRC-16), btree
 */

#ifndef DB_H
#define DB_H

#include <stdio.h>
#include <stddef.h>
#include "util.h"
#include "crc.h"
#include "btree.h"

/* ---- Constants ---- */

#define DB_PAGE_SIZE        512   /* Bytes per page */
#define DB_PAGE_DATA_SIZE   506   /* Usable data bytes per page (512 - 6) */
#define DB_MAX_INDEXES       15   /* Maximum secondary indexes */
#define DB_MAX_FREE_PAGES   127   /* Maximum entries in free page list */
#define DB_SIGNATURE        "VDB" /* File signature (3 chars + null) */
#define DB_VERSION            1   /* Current format version */
#define DB_JOURNAL_ENTRY_SIZE 518 /* Bytes per journal entry on disk */

/* Page status values */
#define PS_EMPTY          0  /* Page is free / unused */
#define PS_ACTIVE         1  /* First page of a record */
#define PS_CONTINUATION   2  /* Subsequent page of a multi-page record */

/* Journal operation types */
#define JO_NONE           0  /* Empty / unused journal entry */
#define JO_UPDATE         1  /* Update existing page data */
#define JO_DELETE         2  /* Mark page as empty */
#define JO_ADD            3  /* Add new record data */

/* Index type values */
#define IT_ID             0  /* Index on an int32 field */
#define IT_STRING         1  /* Index on a string field (CRC-16 key) */

/* ---- Data Structures ---- */

/*
 * DBIndexInfo - Secondary index definition (32 bytes on disk)
 *
 * Fields:
 *   field_name   - Name of the indexed field (30 bytes, null-padded)
 *   index_type   - IT_ID (0) or IT_STRING (1)
 *   index_number - Maps to .I?? file (0-14)
 */
typedef struct {
    char field_name[30];
    byte index_type;
    byte index_number;
} DBIndexInfo;

/*
 * DBHeader - Database metadata stored in page 0 (512 bytes on disk)
 *
 * The first 32 bytes contain header fields. The remaining 480 bytes
 * hold up to 15 index definitions (15 x 32 = 480).
 *
 * Fields:
 *   signature       - "VDB" + '\0' for file type validation
 *   version         - Format version (currently 1)
 *   page_size       - Size of each page in bytes (always 512)
 *   record_size     - Size of each record in bytes
 *   record_count    - Total number of active records
 *   next_record_id  - Next available Record ID (auto-incrementing)
 *   last_compacted  - Timestamp of last compaction operation
 *   journal_pending - TRUE if journal needs replay on open
 *   index_count     - Number of secondary indexes (0-15)
 *   reserved        - Padding to 32 bytes
 *   indexes         - Secondary index definitions
 */
typedef struct {
    char        signature[4];
    uint16      version;
    uint16      page_size;
    uint16      record_size;
    int32       record_count;
    int32       next_record_id;
    int32       last_compacted;
    bool        journal_pending;
    byte        index_count;
    byte        reserved[8];
    DBIndexInfo indexes[DB_MAX_INDEXES];
} DBHeader;

/*
 * DBFreeList - Free page tracking stored in page 1 (512 bytes on disk)
 *
 * Fields:
 *   free_page_count    - Total count of free pages in the database
 *   free_page_list_len - Number of valid entries in free_pages array
 *   free_pages         - Page numbers of known empty pages (LIFO stack)
 */
typedef struct {
    uint16 free_page_count;
    uint16 free_page_list_len;
    int32  free_pages[DB_MAX_FREE_PAGES];
} DBFreeList;

/*
 * DBPage - A single database page (512 bytes on disk)
 *
 * Fields:
 *   id       - Record ID (shared across all pages of a multi-page record)
 *   status   - PS_EMPTY, PS_ACTIVE, or PS_CONTINUATION
 *   reserved - Padding byte
 *   data     - Record data (506 bytes)
 */
typedef struct {
    int32 id;
    byte  status;
    byte  reserved;
    byte  data[DB_PAGE_DATA_SIZE];
} DBPage;

/*
 * DBJournalEntry - A journal entry for crash recovery (518 bytes on disk)
 *
 * Fields:
 *   operation - JO_NONE, JO_UPDATE, JO_DELETE, or JO_ADD
 *   page_num  - Page being modified (-1 for Add operations)
 *   record_id - Record ID for verification
 *   data      - New record data (for Update and Add)
 *   checksum  - CRC-16 of operation + page_num + record_id + data
 */
typedef struct {
    byte   operation;
    int32  page_num;
    int32  record_id;
    byte   data[507];
    uint16 checksum;
} DBJournalEntry;

/*
 * Database - In-memory handle for an open database
 *
 * The primary_index field is a BTree value (not pointer) to avoid
 * dynamic allocation. OpenBTree(&db->primary_index, ...) fills it in.
 *
 * Fields:
 *   name          - Base name of the database (e.g. "USERS")
 *   header        - Cached copy of page 0 header (includes index list)
 *   free_list     - Cached copy of page 1 free list
 *   data_file     - Open file pointer to .DAT file
 *   journal_file  - Open file pointer to .JNL file (NULL if not in txn)
 *   primary_index - B-Tree handle for .IDX file
 *   is_open       - TRUE if the database is currently open
 */
typedef struct {
    char       name[64];
    DBHeader   header;
    DBFreeList free_list;
    FILE      *data_file;
    FILE      *journal_file;
    BTree      primary_index;
    bool       is_open;
} Database;

/* ---- Serialization Helpers (internal) ---- */

/*
 * WriteUint16LE - Write a uint16 in little-endian byte order
 */
void WriteUint16LE(byte *buf, uint16 val);

/*
 * ReadUint16LE - Read a uint16 from little-endian byte order
 */
uint16 ReadUint16LE(const byte *buf);

/*
 * WriteInt32LE - Write an int32 in little-endian byte order
 */
void WriteInt32LE(byte *buf, int32 val);

/*
 * ReadInt32LE - Read an int32 from little-endian byte order
 */
int32 ReadInt32LE(const byte *buf);

/*
 * SerializeHeader - Serialize DBHeader to a 512-byte buffer
 *
 * Writes all header fields and index definitions in little-endian
 * byte order. The buffer must be at least DB_PAGE_SIZE bytes.
 */
void SerializeHeader(const DBHeader *hdr, byte *buf);

/*
 * DeserializeHeader - Deserialize DBHeader from a 512-byte buffer
 *
 * Reads all header fields and index definitions from little-endian
 * byte order.
 */
void DeserializeHeader(const byte *buf, DBHeader *hdr);

/*
 * SerializeFreeList - Serialize DBFreeList to a 512-byte buffer
 *
 * Writes free page count, list length, and page numbers in
 * little-endian byte order.
 */
void SerializeFreeList(const DBFreeList *fl, byte *buf);

/*
 * DeserializeFreeList - Deserialize DBFreeList from a 512-byte buffer
 */
void DeserializeFreeList(const byte *buf, DBFreeList *fl);

/*
 * SerializePage - Serialize DBPage to a 512-byte buffer
 */
void SerializePage(const DBPage *page, byte *buf);

/*
 * DeserializePage - Deserialize DBPage from a 512-byte buffer
 */
void DeserializePage(const byte *buf, DBPage *page);

/*
 * SerializeJournalEntry - Serialize DBJournalEntry to a 518-byte buffer
 */
void SerializeJournalEntry(const DBJournalEntry *entry, byte *buf);

/*
 * DeserializeJournalEntry - Deserialize DBJournalEntry from a 518-byte buffer
 */
void DeserializeJournalEntry(const byte *buf, DBJournalEntry *entry);

/* ---- Utility Functions ---- */

/*
 * CalculatePagesNeeded - Calculate how many pages a record requires
 *
 * Returns ceiling(record_size / DB_PAGE_DATA_SIZE).
 *
 * Parameters:
 *   record_size - Size of the record in bytes
 *
 * Returns:
 *   Number of pages needed (at least 1)
 */
uint16 CalculatePagesNeeded(uint16 record_size);

/*
 * GenerateIndexKey - Generate a B-Tree key for an index lookup
 *
 * For IT_STRING: converts string to lowercase and returns CRC-16.
 * For IT_ID: casts the first 4 bytes as an int32 value.
 *
 * Parameters:
 *   index_type - IT_ID or IT_STRING
 *   value      - Pointer to the value (string or int32 bytes)
 *
 * Returns:
 *   B-Tree key as int32
 */
int32 GenerateIndexKey(byte index_type, const byte *value);

/*
 * ComputeJournalChecksum - Compute CRC-16 checksum of a journal entry
 *
 * Computes CRC-16 over operation + page_num + record_id + data
 * (516 bytes total, excluding the checksum field itself).
 *
 * Parameters:
 *   entry - Journal entry to checksum
 *
 * Returns:
 *   CRC-16 checksum value
 */
uint16 ComputeJournalChecksum(const DBJournalEntry *entry);

/* ---- Internal Page I/O Helpers ---- */

/*
 * BuildFilename - Construct a filename from base name and extension
 *
 * Concatenates name + "." + ext into the output buffer.
 *
 * Parameters:
 *   name     - Base name (e.g. "USERS")
 *   ext      - Extension without dot (e.g. "DAT")
 *   out      - Output buffer
 *   out_size - Size of output buffer
 */
void BuildFilename(const char *name, const char *ext,
                   char *out, size_t out_size);

/*
 * WriteHeaderToDisk - Write the cached header to page 0 of the .DAT file
 *
 * Returns:
 *   TRUE on success, FALSE on failure
 */
bool WriteHeaderToDisk(Database *db);

/*
 * WriteFreeListToDisk - Write the cached free list to page 1 of .DAT file
 *
 * Returns:
 *   TRUE on success, FALSE on failure
 */
bool WriteFreeListToDisk(Database *db);

/*
 * WritePageToDisk - Write a page to the .DAT file at the given page number
 *
 * Parameters:
 *   db       - Open database handle
 *   page_num - Page number (0-based)
 *   page     - Page data to write
 *
 * Returns:
 *   TRUE on success, FALSE on failure
 */
bool WritePageToDisk(Database *db, int32 page_num, const DBPage *page);

/*
 * ReadPageFromDisk - Read a page from the .DAT file
 *
 * Parameters:
 *   db       - Open database handle
 *   page_num - Page number (0-based)
 *   page     - Output page data
 *
 * Returns:
 *   TRUE on success, FALSE on failure
 */
bool ReadPageFromDisk(Database *db, int32 page_num, DBPage *page);

/*
 * GetTotalPages - Get total number of pages in the .DAT file
 *
 * Parameters:
 *   db - Open database handle
 *
 * Returns:
 *   Total page count based on file size
 */
int32 GetTotalPages(Database *db);

/* ---- Free Page Management (internal) ---- */

/*
 * AllocatePages - Allocate consecutive pages for a new record
 *
 * Tries to find pages from the free list first. If no suitable
 * consecutive run is found, appends new pages at end of file.
 *
 * Parameters:
 *   db           - Open database handle
 *   pages_needed - Number of consecutive pages to allocate
 *
 * Returns:
 *   First page number of the allocated run, or -1 on failure
 */
int32 AllocatePages(Database *db, uint16 pages_needed);

/*
 * ReleasePages - Release pages back to the free list
 *
 * Increments free_page_count and adds the first page to the
 * free list array if there is room.
 *
 * Parameters:
 *   db          - Open database handle
 *   first_page  - First page number of the run
 *   pages_count - Number of consecutive pages to release
 */
void ReleasePages(Database *db, int32 first_page, uint16 pages_count);

/* ---- Database Operations ---- */

/*
 * CreateDatabase - Create a new database with the given record size
 *
 * Creates .DAT and .IDX files. Initializes header with default values
 * and an empty free list. The .DAT file contains 2 pages (header + free list).
 *
 * Parameters:
 *   name        - Base name for database files (e.g. "USERS")
 *   record_size - Size of each record in bytes (1-65535)
 *
 * Returns:
 *   TRUE on success, FALSE on failure
 */
bool CreateDatabase(const char *name, uint16 record_size);

/*
 * OpenDatabase - Open an existing database
 *
 * Opens the .DAT file, reads header and free list, opens the
 * primary index. If journal_pending is TRUE, replays the journal.
 *
 * Parameters:
 *   name - Base name for database files
 *   db   - Caller-allocated Database struct to fill in
 *
 * Returns:
 *   TRUE on success, FALSE on failure
 */
bool OpenDatabase(const char *name, Database *db);

/*
 * CloseDatabase - Close an open database
 *
 * Writes header and free list to disk, closes all files,
 * and marks the database as not open.
 *
 * Parameters:
 *   db - Open database handle
 */
void CloseDatabase(Database *db);

/* ---- Record Operations ---- */

/*
 * AddRecord - Add a new record to the database
 *
 * Allocates pages, writes data, updates the primary index,
 * and increments record_count and next_record_id.
 *
 * The caller is responsible for updating secondary indexes
 * after this call (the db layer does not know field layout).
 *
 * Parameters:
 *   db        - Open database handle
 *   data      - Record data (header.record_size bytes)
 *   record_id - Output: assigned Record ID
 *
 * Returns:
 *   TRUE on success, FALSE on failure
 */
bool AddRecord(Database *db, const byte *data, int32 *record_id);

/*
 * FindRecordByID - Find a record by its Record ID
 *
 * Looks up the page number in the primary index, reads all
 * pages for the record, and assembles the data.
 *
 * Parameters:
 *   db   - Open database handle
 *   id   - Record ID to find
 *   data - Output buffer (must be at least header.record_size bytes)
 *
 * Returns:
 *   TRUE if found, FALSE otherwise
 */
bool FindRecordByID(Database *db, int32 id, byte *data);

/*
 * FindRecordByString - Find a record by a string field value
 *
 * Looks up the secondary index for the given field_name,
 * generates a CRC-16 key from the value, and searches.
 * Returns the first matching record.
 *
 * Parameters:
 *   db         - Open database handle
 *   field_name - Name of the indexed field
 *   value      - String value to search for
 *   data       - Output buffer (header.record_size bytes)
 *   record_id  - Output: Record ID of the found record
 *
 * Returns:
 *   TRUE if found, FALSE otherwise
 */
bool FindRecordByString(Database *db, const char *field_name,
                        const char *value, byte *data,
                        int32 *record_id);

/*
 * UpdateRecord - Update an existing record's data
 *
 * Finds the record by ID and overwrites its data pages.
 * The caller is responsible for updating secondary indexes
 * when indexed fields change.
 *
 * Parameters:
 *   db   - Open database handle
 *   id   - Record ID of the record to update
 *   data - New record data (header.record_size bytes)
 *
 * Returns:
 *   TRUE on success, FALSE if record not found
 */
bool UpdateRecord(Database *db, int32 id, const byte *data);

/*
 * DeleteRecord - Delete a record by its Record ID
 *
 * Marks all pages as empty, removes from primary index,
 * releases pages, and decrements record_count.
 *
 * Parameters:
 *   db - Open database handle
 *   id - Record ID of the record to delete
 *
 * Returns:
 *   TRUE on success, FALSE if record not found
 */
bool DeleteRecord(Database *db, int32 id);

/* ---- Index Operations ---- */

/*
 * AddIndex - Add a secondary index to the database
 *
 * Creates a new .I?? B-Tree file and adds the index definition
 * to the header. Maximum 15 indexes supported.
 *
 * Parameters:
 *   db         - Open database handle
 *   field_name - Name of the field to index
 *   index_type - IT_ID or IT_STRING
 *
 * Returns:
 *   TRUE on success, FALSE on failure
 */
bool AddIndex(Database *db, const char *field_name, byte index_type);

/*
 * RebuildIndex - Rebuild a specified index from scratch
 *
 * For index_number == -1: rebuilds the primary index (.IDX)
 * by scanning all active pages in the .DAT file.
 *
 * For index_number >= 0: recreates the secondary index file.
 * The caller must repopulate it (db layer does not know field layout).
 *
 * Parameters:
 *   db           - Open database handle
 *   index_number - -1 for primary, 0-14 for secondary
 *
 * Returns:
 *   TRUE on success, FALSE on failure
 */
bool RebuildIndex(Database *db, int16 index_number);

/* ---- Maintenance Operations ---- */

/*
 * UpdateFreePages - Scan database and repopulate free page list
 *
 * Scans all data pages starting at page 2 and counts empty pages.
 * Fills the free_pages array with up to DB_MAX_FREE_PAGES entries.
 *
 * Parameters:
 *   db - Open database handle
 *
 * Returns:
 *   TRUE on success, FALSE on failure
 */
bool UpdateFreePages(Database *db);

/*
 * CompactDatabase - Remove empty pages and rebuild indexes
 *
 * Moves all active records to fill gaps left by deleted records.
 * Rebuilds the primary index to reflect new page locations.
 * Updates last_compacted timestamp.
 *
 * Parameters:
 *   db - Open database handle
 *
 * Returns:
 *   TRUE on success, FALSE on failure
 */
bool CompactDatabase(Database *db);

/*
 * ValidateDatabase - Check database integrity
 *
 * Verifies header signature and version, checks that record_count
 * matches actual active pages, and validates primary index entries.
 *
 * Parameters:
 *   db - Open database handle
 *
 * Returns:
 *   TRUE if database is valid, FALSE if corruption detected
 */
bool ValidateDatabase(Database *db);

/* ---- Transaction Operations ---- */

/*
 * BeginTransaction - Start a new transaction
 *
 * Opens the journal file and sets journal_pending=TRUE.
 * Subsequent record operations will write journal entries.
 *
 * Parameters:
 *   db - Open database handle
 *
 * Returns:
 *   TRUE on success, FALSE on failure
 */
bool BeginTransaction(Database *db);

/*
 * CommitTransaction - Commit the current transaction
 *
 * Sets journal_pending=FALSE and truncates the journal file.
 *
 * Parameters:
 *   db - Open database handle
 *
 * Returns:
 *   TRUE on success, FALSE on failure
 */
bool CommitTransaction(Database *db);

/*
 * RollbackTransaction - Roll back the current transaction
 *
 * Discards the journal without applying. Sets journal_pending=FALSE
 * and truncates the journal file.
 *
 * Parameters:
 *   db - Open database handle
 *
 * Returns:
 *   TRUE on success, FALSE on failure
 */
bool RollbackTransaction(Database *db);

/*
 * WriteJournalEntry - Write a single journal entry to the journal file
 *
 * Computes the checksum, serializes, and writes to the journal.
 *
 * Parameters:
 *   db    - Open database handle
 *   entry - Journal entry to write
 *
 * Returns:
 *   TRUE on success, FALSE on failure
 */
bool WriteJournalEntry(Database *db, const DBJournalEntry *entry);

/*
 * ReplayJournal - Replay journal entries to recover from a crash
 *
 * Reads all journal entries, verifies checksums, and applies
 * valid operations. Rebuilds the primary index after replay.
 *
 * Parameters:
 *   db - Open database handle
 *
 * Returns:
 *   TRUE on success, FALSE on failure
 */
bool ReplayJournal(Database *db);

#endif /* DB_H */

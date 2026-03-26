# Database Library Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement the vDB database library providing page-based storage, primary/secondary indexing via B-Trees, journal-based crash recovery, and maintenance operations (compaction, validation).

**Architecture:** The library is split across multiple source files by responsibility: serialization helpers (`dbutil.c`), core lifecycle and page I/O (`db.c`), record CRUD (`record.c`), journal/transactions (`journal.c`), and maintenance (`maint.c`). All on-disk structures use explicit little-endian byte-by-byte serialization (same pattern as btree.c). The library builds on the btree module for indexing and the crc module for checksums and key generation.

**Tech Stack:** ANSI C (C89), vDB util/crc/btree/os libraries, vDB test framework

**Spec reference:** `docs/db.md` — contains complete format specification, data types, function signatures, and usage examples. Implementers should read this document thoroughly.

---

## File Structure

| File                  | Responsibility                                                 |
| --------------------- | -------------------------------------------------------------- |
| `include/db.h`        | Primary header: all constants, types, and function declarations|
| `src/db/dbutil.c`     | LE serialization helpers, CalculatePagesNeeded, GenerateIndexKey|
| `src/db/db.c`         | CreateDatabase, OpenDatabase, CloseDatabase, page I/O helpers  |
| `src/db/record.c`     | AddRecord, FindRecordByID, FindRecordByString, UpdateRecord, DeleteRecord |
| `src/db/journal.c`    | BeginTransaction, CommitTransaction, RollbackTransaction, ReplayJournal |
| `src/db/maint.c`      | UpdateFreePages, CompactDatabase, ValidateDatabase, RebuildIndex, AddIndex |
| `tests/db/dbutil.c`   | Serialization and utility function tests                       |
| `tests/db/db.c`       | Database lifecycle tests (create, open, close)                 |
| `tests/db/record.c`   | Record CRUD tests                                              |
| `tests/db/journal.c`  | Journal and transaction tests                                  |
| `tests/db/maint.c`    | Maintenance operation tests                                    |

---

### Task 1: Header File and Constants

**Files:**
- Create: `include/db.h`

- [ ] **Step 1: Create the database header with all types and declarations**

Create `include/db.h` with all constants, types, and function prototypes from `docs/db.md`. Key elements:

**Constants:**
```c
#define DB_PAGE_SIZE      512
#define DB_PAGE_DATA_SIZE 506
#define DB_MAX_INDEXES     15
#define DB_MAX_FREE_PAGES 127
#define DB_SIGNATURE      "VDB"
#define DB_VERSION          1
#define DB_JOURNAL_ENTRY_SIZE 518

/* Page status */
#define PS_EMPTY        0
#define PS_ACTIVE       1
#define PS_CONTINUATION 2

/* Journal operations */
#define JO_NONE   0
#define JO_UPDATE 1
#define JO_DELETE 2
#define JO_ADD    3

/* Index types */
#define IT_ID     0
#define IT_STRING 1
```

**Types** (from `docs/db.md` Data Types section):
- `DBIndexInfo` (32 bytes: field_name[30], index_type, index_number)
- `DBHeader` (512 bytes: signature, version, page_size, record_size, record_count, next_record_id, last_compacted, journal_pending, index_count, reserved[8], indexes[15])
- `DBFreeList` (512 bytes: free_page_count, free_page_list_len, free_pages[127])
- `DBPage` (512 bytes: id, status, reserved, data[506])
- `DBJournalEntry` (518 bytes: operation, page_num, record_id, data[507], checksum)
- `Database` (handle: name[64], header, free_list, data_file, journal_file, primary_index, is_open)

Note: The `Database` struct's `primary_index` field should be a `BTree` value (not pointer) to avoid dynamic allocation. This works because `OpenBTree(BTree *tree, ...)` fills in a caller-provided struct via pointer — so `OpenBTree(&db->primary_index, ...)` is correct. Similarly `CloseBTree(&db->primary_index)`.

**Spec deviation:** `docs/db.md` declares `BTree *primary_index` (pointer). We use `BTree primary_index` (value) to avoid needing `OsMalloc`/`OsFree` for the handle itself. No behavioral change.

**Function prototypes** — all functions listed in `docs/db.md` "Database Helper Methods" section, plus internal utility functions:
- `uint16 CalculatePagesNeeded(uint16 record_size)` — ceiling(record_size / DB_PAGE_DATA_SIZE)
- `int32 GenerateIndexKey(byte index_type, const byte *value)` — CRC16 for strings, direct cast for IDs
- All database, record, index, maintenance, and transaction operations

Include guards, includes for `util/types.h`, `btree.h`, `<stdio.h>`, `<stddef.h>`.

- [ ] **Step 2: Commit header**

```bash
git add include/db.h
git commit -m "feat(db): add database library header with types and declarations"
```

---

### Task 2: Serialization Helpers and Utility Functions

**Files:**
- Create: `src/db/dbutil.c`
- Create: `tests/db/dbutil.c`
- Modify: `Makefile`

- [ ] **Step 1: Write serialization tests**

Create `tests/db/dbutil.c` with tests for:
1. **CalculatePagesNeeded**: 1 byte → 1 page, 506 bytes → 1 page, 507 bytes → 2 pages, 1012 bytes → 2 pages, 1013 bytes → 3 pages
2. **GenerateIndexKey**: IT_STRING type returns CRC16 of lowercased string (case-insensitive), IT_ID type returns the value directly cast to int32
3. **DBHeader serialization round-trip**: Create a header, serialize to a 512-byte buffer, deserialize back, verify all fields match
4. **DBFreeList serialization round-trip**: Same pattern
5. **DBPage serialization round-trip**: Same pattern
6. **DBJournalEntry serialization round-trip**: Same pattern, verify checksum field preserved
7. **DBIndexInfo serialization**: Verify field_name, index_type, index_number survive round-trip

All serialization helpers use the same little-endian pattern as `src/btree/btree.c` (WriteUint16LE, ReadUint16LE, WriteInt32LE, ReadInt32LE — but defined locally in dbutil.c since they are internal).

- [ ] **Step 2: Implement serialization helpers**

Create `src/db/dbutil.c` with:
- Local LE read/write helpers (same pattern as btree.c)
- `SerializeHeader(const DBHeader *hdr, byte *buf)` — writes 512 bytes
- `DeserializeHeader(const byte *buf, DBHeader *hdr)` — reads 512 bytes
- `SerializeFreeList(const DBFreeList *fl, byte *buf)` — writes 512 bytes
- `DeserializeFreeList(const byte *buf, DBFreeList *fl)` — reads 512 bytes
- `SerializePage(const DBPage *page, byte *buf)` — writes 512 bytes
- `DeserializePage(const byte *buf, DBPage *page)` — reads 512 bytes
- `SerializeJournalEntry(const DBJournalEntry *entry, byte *buf)` — writes 518 bytes
- `DeserializeJournalEntry(const byte *buf, DBJournalEntry *entry)` — reads 518 bytes
- `uint16 CalculatePagesNeeded(uint16 record_size)` — ceiling division
- `int32 GenerateIndexKey(byte index_type, const byte *value)` — uses Crc16String for IT_STRING, casts int32 for IT_ID
- `uint16 ComputeJournalChecksum(const DBJournalEntry *entry)` — CRC16 over operation + page_num + record_id + data (516 bytes, excludes checksum field)

These serialization functions must also be declared in `include/db.h` (or a separate internal header) so other db source files can use them. Since the project pattern uses a single primary header, declare them in `include/db.h`.

- [ ] **Step 3: Add Makefile target**

Add `DB_SRC` variable listing all db source files. Add `test_dbutil` target. Update `all`, `test`, `.PHONY`.

```makefile
DB_SRC = src/db/dbutil.c src/db/db.c src/db/record.c src/db/journal.c src/db/maint.c
```

For the initial test, only `src/db/dbutil.c` needs to compile. Create stubs for the other files (empty or with only `#include "db.h"`).

- [ ] **Step 4: Build and verify tests pass**

Run: `make test_dbutil && ./bin/test_dbutil`

- [ ] **Step 5: Commit**

```bash
git add src/db/dbutil.c tests/db/dbutil.c src/db/db.c src/db/record.c src/db/journal.c src/db/maint.c Makefile
git commit -m "feat(db): add serialization helpers and utility functions"
```

---

### Task 3: Database Create, Open, Close

**Files:**
- Modify: `src/db/db.c`
- Create: `tests/db/db.c`
- Modify: `Makefile`

- [ ] **Step 1: Write lifecycle tests**

Create `tests/db/db.c` with tests for:
1. **CreateDatabase**: Creates `.DAT` and `.IDX` files, verify DAT file has at least 2 pages (1024 bytes), read back header and verify signature="VDB", version=1, page_size=512, record_count=0, next_record_id=1
2. **CreateDatabase with various record sizes**: 100 bytes, 506 bytes, 1012 bytes
3. **OpenDatabase**: Create then open, verify header fields loaded, verify is_open=TRUE
4. **OpenDatabase invalid file**: Returns FALSE for non-existent file
5. **CloseDatabase**: After close, verify is_open=FALSE
6. **OpenDatabase after close**: Create, open, close, reopen — verify header preserved
7. **Header persistence**: Create, open, add some records (if available) or modify header fields, close, reopen, verify persistence

Use temporary file names that are cleaned up after each test. Since we're testing on UNIX, use names like `_test.DAT`, `_test.IDX` etc. and `remove()` them in test cleanup.

- [ ] **Step 2: Implement database lifecycle**

In `src/db/db.c` implement:

**`bool CreateDatabase(const char *name, uint16 record_size)`:**
1. Build filenames: `<name>.DAT`, `<name>.IDX` (8.3 format, caller provides base name like "USERS")
2. Initialize DBHeader with defaults (signature, version=1, page_size=512, record_size, record_count=0, next_record_id=1, journal_pending=FALSE, index_count=0)
3. Initialize empty DBFreeList (all zeros)
4. Write header to page 0, free list to page 1 of .DAT file
5. Create .IDX file using `CreateBTree()`
6. Close files, return TRUE

**`bool OpenDatabase(const char *name, Database *db)`:**
1. Build filenames from name
2. Open .DAT file ("r+b")
3. Read page 0, deserialize header, validate signature
4. Read page 1, deserialize free list
5. Open primary index with `OpenBTree()`
6. Store name, set is_open=TRUE
7. If journal_pending, call ReplayJournal (stub for now — returns TRUE)
8. Return TRUE

**`void CloseDatabase(Database *db)`:**
1. Write header to page 0, free list to page 1
2. Close primary index with `CloseBTree()`
3. Close .DAT file
4. Close journal file if open
5. Set is_open=FALSE

Also implement internal page I/O helpers:
- `bool WriteHeaderToDisk(Database *db)` — serialize header + write page 0
- `bool WriteFreeListToDisk(Database *db)` — serialize free list + write page 1
- `bool WritePageToDisk(Database *db, int32 page_num, const DBPage *page)` — serialize + write at offset
- `bool ReadPageFromDisk(Database *db, int32 page_num, DBPage *page)` — read + deserialize
- `int32 GetTotalPages(Database *db)` — file size / DB_PAGE_SIZE

Helper to build filenames:
- `void BuildFilename(const char *name, const char *ext, char *out, size_t out_size)` — concatenates name + "." + ext into out buffer

- [ ] **Step 3: Add Makefile target for test_db**

- [ ] **Step 4: Build and verify tests pass**

Run: `make test_db && ./bin/test_db`

- [ ] **Step 5: Commit**

```bash
git add src/db/db.c tests/db/db.c Makefile
git commit -m "feat(db): implement CreateDatabase, OpenDatabase, CloseDatabase"
```

---

### Task 4: Free Page Management

**Files:**
- Modify: `src/db/maint.c`
- Modify: `src/db/db.c` (add AllocatePages helper)
- Create: `tests/db/maint.c`
- Modify: `Makefile`

- [ ] **Step 1: Write free page management tests**

Create `tests/db/maint.c` with tests for:
1. **UpdateFreePages on empty database**: No free pages found, counts stay 0
2. **UpdateFreePages with deleted pages**: Create DB, manually write empty pages to .DAT, call UpdateFreePages, verify free_page_count and free_page_list_len updated
3. **AllocatePages when no free pages**: Should append to end of file, return new page number
4. **AllocatePages from free list**: Add pages to free list, allocate, verify they come from free list (LIFO)
5. **AllocatePages multi-page**: For record_size > 506, need consecutive pages

- [ ] **Step 2: Implement free page management**

In `src/db/maint.c`:

**`bool UpdateFreePages(Database *db)`:**
1. Scan .DAT file starting at page 2
2. For each page, read and check status
3. Count all PS_EMPTY pages (set free_page_count)
4. Fill free_pages array with up to 127 empty page numbers
5. Set free_page_list_len
6. Write free list to disk

In `src/db/db.c` (or accessible to record.c):

**`int32 AllocatePages(Database *db, uint16 pages_needed)`:**
1. If free_page_count == 0: append pages at end of file, return first page number
2. If free_page_list_len > 0: search free list for consecutive run of pages_needed
   - If found: remove from free list, decrement counts, return first page number
   - If not found: append at end
3. If free_page_count > 0 but free_page_list_len == 0: call UpdateFreePages, then retry
4. Decrement free_page_count by pages_needed
5. Write free list to disk
6. Return first page number, or -1 on failure

**`void ReleasePages(Database *db, int32 first_page, uint16 pages_count)`:**
1. Increment free_page_count by pages_count
2. If free_page_list_len < DB_MAX_FREE_PAGES: add first_page to array, increment len
3. Write free list to disk

- [ ] **Step 3: Add Makefile target for test_maint**

- [ ] **Step 4: Build and verify tests pass**

- [ ] **Step 5: Commit**

```bash
git add src/db/maint.c src/db/db.c tests/db/maint.c Makefile
git commit -m "feat(db): implement free page management"
```

---

### Task 5: Record Operations — Add and FindByID

**Files:**
- Modify: `src/db/record.c`
- Create: `tests/db/record.c`
- Modify: `Makefile`

- [ ] **Step 1: Write record tests for Add and FindByID**

Create `tests/db/record.c` with tests for:
1. **AddRecord single page**: Add a record with data < 506 bytes, verify record_id returned, verify record_count incremented
2. **FindRecordByID**: Add a record, find it by ID, verify data matches
3. **AddRecord multiple**: Add 3 records, find each by ID
4. **AddRecord multi-page**: Set record_size > 506, add a record, find by ID, verify all data bytes correct across pages
5. **FindRecordByID not found**: Returns FALSE for non-existent ID
6. **Record persistence**: Add records, close DB, reopen, find records still there

- [ ] **Step 2: Implement AddRecord and FindRecordByID**

In `src/db/record.c`:

**Note:** Tasks 5-6 implement record operations WITHOUT journaling initially. Task 8 adds journal infrastructure and then retrofits journal calls into AddRecord, UpdateRecord, and DeleteRecord so that every write goes through the write-ahead protocol.

**`bool AddRecord(Database *db, const byte *data, int32 *record_id)`:**
1. Assign new record ID from header.next_record_id, increment it
2. Calculate pages_needed = CalculatePagesNeeded(header.record_size)
3. Allocate pages using AllocatePages()
4. Write data across pages (first page PS_ACTIVE, rest PS_CONTINUATION)
5. Insert into primary index: BTreeInsert(primary_index, record_id, first_page_num)
6. **Spec deviation:** The spec says AddRecord "Updates all indexes," but the db layer doesn't know field layout (raw byte data). The caller (useradm) is responsible for inserting into secondary indexes after AddRecord, since only the caller knows which bytes correspond to indexed fields. Same rationale as UpdateRecord (see Task 6 notes).
7. Increment header.record_count
7. Write header to disk
8. Return record_id via pointer, return TRUE

**`bool FindRecordByID(Database *db, int32 id, byte *data)`:**
1. Look up in primary index: BTreeFind(primary_index, id, &page_num, 1, &count)
2. If not found, return FALSE
3. Calculate pages_needed
4. Read pages_needed consecutive pages starting at page_num
5. Verify first page status==PS_ACTIVE and id matches
6. Assemble data from all pages into caller's buffer
7. Return TRUE

- [ ] **Step 3: Add Makefile target for test_record**

- [ ] **Step 4: Build and verify tests pass**

- [ ] **Step 5: Commit**

```bash
git add src/db/record.c tests/db/record.c Makefile
git commit -m "feat(db): implement AddRecord and FindRecordByID"
```

---

### Task 6: Record Operations — FindByString, Update, Delete

**Files:**
- Modify: `src/db/record.c`
- Modify: `tests/db/record.c`

- [ ] **Step 1: Write tests for remaining record operations**

Add tests to `tests/db/record.c`:
1. **DeleteRecord**: Add record, delete by ID, verify FindByID returns FALSE, verify record_count decremented
2. **DeleteRecord frees pages**: After delete, free_page_count increases
3. **UpdateRecord**: Add record, update data, find by ID, verify new data
4. **UpdateRecord same size**: Data in existing pages overwritten, no page movement
5. **FindRecordByString**: Requires a secondary index. Add an index with AddIndex (or set up manually), add records, find by string value
6. **FindRecordByString case insensitive**: "Alice" and "alice" find same record
7. **FindRecordByString not found**: Returns FALSE
8. **Delete then add reuses pages**: Delete a record, add new one, verify free_page_count behavior

- [ ] **Step 2: Implement remaining record operations**

**`bool DeleteRecord(Database *db, int32 id)`:**
1. Find page_num via primary index
2. Calculate pages_needed
3. Mark all pages as PS_EMPTY (write to disk)
4. Delete from primary index: BTreeDelete(primary_index, id) — this removes the key entirely
5. Delete from all secondary indexes (need to read record data first to get field values, then delete index entries)
6. Release pages
7. Decrement header.record_count
8. Write header and free list to disk

**`bool UpdateRecord(Database *db, int32 id, const byte *data)`:**
1. Find page_num via primary index
2. Write new data to existing pages (same location)
3. No primary index change needed (same ID, same pages)
4. For secondary index updates: the caller is responsible for managing indexed field changes via DeleteRecord + AddRecord, OR the db module can compare old vs new data at indexed field offsets. For simplicity in v1: just overwrite the pages. The useradm module (caller) handles secondary index updates when it knows which fields changed.

Note: The spec says "Updates indexes if indexed fields changed" but the db layer doesn't know field layout. The simplest correct approach: UpdateRecord overwrites data pages only. The caller (useradm) manages secondary index entries when it changes indexed fields.

**`bool FindRecordByString(Database *db, const char *field_name, const char *value, byte *data, int32 *record_id)`:**
1. Find the matching index in header.indexes[] by field_name
2. If not found, return FALSE
3. Open secondary index file (.I?? based on index_number)
4. Generate key: GenerateIndexKey(index_type, value)
5. BTreeFind to get candidate record_ids (may have CRC16 collisions)
6. For each candidate: FindRecordByID, check if the record's field matches the search value
7. Close secondary index
8. Return first match

The collision-checking requires the caller to provide a comparison function or the db module to know the field offset. For simplicity: FindRecordByString returns the first record found for the hash key. The caller verifies the match. This matches the spec's note about handling hash collisions.

Actually, re-reading the spec more carefully: FindRecordByString should "handle hash collisions" and "return first matching record." Since the db layer doesn't know field layout, it should return ALL candidate record IDs to the caller, or iterate and let the caller's comparison function verify. The simplest approach that matches the spec: return the first record found. If the caller needs collision handling, they can use the btree directly.

- [ ] **Step 3: Build and verify tests pass**

- [ ] **Step 4: Commit**

```bash
git add src/db/record.c tests/db/record.c
git commit -m "feat(db): implement DeleteRecord, UpdateRecord, FindRecordByString"
```

---

### Task 7: Index Operations

**Files:**
- Modify: `src/db/maint.c`
- Modify: `tests/db/maint.c`

- [ ] **Step 1: Write index operation tests**

Add tests to `tests/db/maint.c`:
1. **AddIndex**: Create DB, add a secondary index, verify index_count incremented, verify .I?? file created
2. **AddIndex max**: Can add up to 15 indexes
3. **AddIndex with existing records**: Add records first, then add index — index should be populated by scanning existing records. Note: this requires the db layer to know field offsets, which is application-specific. For v1, AddIndex creates an empty index file. The caller populates it.
4. **RebuildIndex primary**: Rebuild the .IDX file from .DAT file records
5. **RebuildIndex secondary**: Clear and rebuild a secondary index

- [ ] **Step 2: Implement index operations**

**`bool AddIndex(Database *db, const char *field_name, byte index_type)`:**
1. Check index_count < DB_MAX_INDEXES
2. Assign next available index_number (0-14)
3. Build index filename (.I00, .I01, etc.)
4. Create new BTree file: CreateBTree(filename)
5. Add DBIndexInfo to header.indexes[index_count]
6. Increment header.index_count
7. Write header to disk
8. Return TRUE

**`bool RebuildIndex(Database *db, int16 index_number)`:**
1. If index_number == -1, rebuild primary index:
   - Recreate .IDX file
   - Scan all pages in .DAT starting at page 2
   - For each PS_ACTIVE page, insert record_id → page_num into primary index
2. If index_number >= 0, rebuild secondary index:
   - Find index info in header
   - Recreate .I?? file
   - Note: populating secondary indexes requires application-specific field knowledge, so for a generic rebuild, just create an empty index. The caller repopulates.

- [ ] **Step 3: Build and verify tests pass**

- [ ] **Step 4: Commit**

```bash
git add src/db/maint.c tests/db/maint.c
git commit -m "feat(db): implement AddIndex and RebuildIndex"
```

---

### Task 8: Journal and Transaction Operations

**Files:**
- Modify: `src/db/journal.c`
- Create: `tests/db/journal.c`
- Modify: `Makefile`

- [ ] **Step 1: Write journal/transaction tests**

Create `tests/db/journal.c` with tests for:
1. **BeginTransaction**: Sets journal_pending=TRUE, journal file opened
2. **CommitTransaction**: Sets journal_pending=FALSE, truncates journal
3. **RollbackTransaction**: Sets journal_pending=FALSE, truncates journal without applying
4. **Journal entry round-trip**: Write a journal entry, read it back, verify all fields including checksum
5. **ReplayJournal with JO_ADD**: Write a JO_ADD entry to journal, set journal_pending=TRUE, close DB, reopen (triggers replay), verify record exists
6. **ReplayJournal with JO_DELETE**: Similar — write JO_DELETE entry, replay, verify page marked empty
7. **ReplayJournal with JO_UPDATE**: Write JO_UPDATE entry, replay, verify page data updated
8. **ReplayJournal corrupted entry**: Entry with bad checksum is skipped
9. **ReplayJournal rebuilds indexes**: After replay, primary index is consistent with data

- [ ] **Step 2: Implement journal operations**

**`bool BeginTransaction(Database *db)`:**
1. Build journal filename: `<name>.JNL`
2. Open journal file for writing ("wb")
3. Set header.journal_pending = TRUE
4. Write header to disk
5. Return TRUE

**`bool CommitTransaction(Database *db)`:**
1. Set header.journal_pending = FALSE
2. Write header to disk
3. Close and truncate journal file (reopen as "wb" then close, or use ftruncate equivalent)
4. Return TRUE

**`bool RollbackTransaction(Database *db)`:**
1. Set header.journal_pending = FALSE
2. Write header to disk
3. Close and truncate journal file
4. Return TRUE

**`bool WriteJournalEntry(Database *db, const DBJournalEntry *entry)`:**
1. Compute checksum: ComputeJournalChecksum(entry)
2. Serialize entry to buffer (518 bytes)
3. Write buffer to journal file
4. Flush journal file
5. Return TRUE

**`bool ReplayJournal(Database *db)`:**
1. Open journal file for reading ("rb")
2. Read entries one by one (518 bytes each)
3. For each entry:
   - Verify checksum, skip if invalid
   - JO_ADD: Find empty page (or append), write data
   - JO_UPDATE: Write data to specified page_num
   - JO_DELETE: Set page status to PS_EMPTY
4. Close journal file
5. Rebuild primary index (scan all pages, rebuild from scratch). **Note:** Spec says "rebuild all indexes" — for v1 we only rebuild primary, since secondary indexes use record IDs (not page numbers) and are less likely to be corrupted. If secondary indexes are inconsistent, the caller can use RebuildIndex.
6. Set header.journal_pending = FALSE
7. Write header to disk
8. Truncate journal file
9. Return TRUE

- [ ] **Step 3: Add Makefile target for test_journal**

- [ ] **Step 4: Build and verify tests pass**

- [ ] **Step 5: Commit journal infrastructure**

```bash
git add src/db/journal.c tests/db/journal.c Makefile
git commit -m "feat(db): implement journal and transaction operations"
```

- [ ] **Step 6: Retrofit journal calls into record operations**

Now that journal infrastructure exists, modify `src/db/record.c` to use write-ahead journaling per the spec's Transaction Protocol:

**AddRecord** — after allocating pages but BEFORE writing data:
1. Create a JO_ADD journal entry with the page data (one entry per page for multi-page records)
2. Call `WriteJournalEntry(db, &entry)` for each page
3. Then proceed with page writes and index updates

**UpdateRecord** — BEFORE overwriting pages:
1. Create a JO_UPDATE journal entry with the new page data
2. Call `WriteJournalEntry(db, &entry)`
3. Then overwrite the pages

**DeleteRecord** — BEFORE marking pages empty:
1. Create a JO_DELETE journal entry with page_num and record_id
2. Call `WriteJournalEntry(db, &entry)`
3. Then mark pages empty and update indexes

The journal entries are only written when a transaction is active (`journal_pending == TRUE`). If no transaction is active, the record operations work as before (direct writes without journaling). This means the caller controls whether an operation is journaled by calling `BeginTransaction` first.

- [ ] **Step 7: Add tests for journaled record operations**

Add tests to `tests/db/journal.c`:
1. **Journaled AddRecord**: BeginTransaction → AddRecord → Verify journal entry written → CommitTransaction
2. **Journaled Delete with rollback**: BeginTransaction → DeleteRecord → RollbackTransaction → Verify record still exists after reopen
3. **Crash recovery for Add**: BeginTransaction → AddRecord → Close without commit → Reopen (triggers ReplayJournal) → Verify record exists

- [ ] **Step 8: Build and verify all tests pass**

Run: `make test` — all suites must pass, including existing record tests (which should still work without transactions).

- [ ] **Step 9: Commit journal integration**

```bash
git add src/db/record.c tests/db/journal.c
git commit -m "feat(db): integrate write-ahead journaling into record operations"
```

---

### Task 9: Compaction and Validation

**Files:**
- Modify: `src/db/maint.c`
- Modify: `tests/db/maint.c`

- [ ] **Step 1: Write compaction and validation tests**

Add tests to `tests/db/maint.c`:
1. **CompactDatabase empty**: No records, no-op, returns TRUE
2. **CompactDatabase with gaps**: Add 3 records, delete middle one, compact, verify file shrunk, all records still findable
3. **CompactDatabase rebuilds indexes**: After compaction, primary index points to new page locations
4. **CompactDatabase updates timestamp**: last_compacted field updated
5. **ValidateDatabase valid**: Fresh database passes validation
6. **ValidateDatabase after operations**: Add/delete records, validate still passes
7. **ValidateDatabase corrupt index**: Manually corrupt primary index, validate returns FALSE

- [ ] **Step 2: Implement compaction and validation**

**`bool CompactDatabase(Database *db)`:**
1. Create temporary .DAT file
2. Write header (pages 0-1) to temp file
3. Scan original .DAT starting at page 2
4. For each PS_ACTIVE page (start of a record):
   - Read all pages for that record
   - Write to next available position in temp file
   - Track old→new page mapping
5. Close primary index, recreate it
6. For each record written: insert record_id → new_page_num into new index
7. Replace original .DAT with temp file (rename)
8. Reset free list (free_page_count=0, free_page_list_len=0)
9. Update header.last_compacted with current timestamp
10. Write header and free list
11. Return TRUE

**`bool ValidateDatabase(Database *db)`:**
1. Verify header signature and version
2. Verify record_count matches actual active pages
3. For each active record in .DAT:
   - Verify primary index has entry for that record_id
   - Verify primary index page_num matches actual location
4. Return TRUE if all checks pass, FALSE otherwise

- [ ] **Step 3: Build and verify tests pass**

- [ ] **Step 4: Commit**

```bash
git add src/db/maint.c tests/db/maint.c
git commit -m "feat(db): implement CompactDatabase and ValidateDatabase"
```

---

### Task 10: Integration Tests and Final Verification

**Files:**
- Modify: `tests/db/record.c` or create `tests/db/integ.c`

- [ ] **Step 1: Write integration tests**

Add comprehensive integration tests:
1. **Full CRUD cycle**: Create DB → Add 5 records → Find each → Update 2 → Delete 1 → Find remaining → Close → Reopen → Verify all data
2. **Secondary index workflow**: Create DB → AddIndex("Username", IT_STRING) → Add records → FindRecordByString → Delete → Verify index updated
3. **Transaction workflow**: Begin → Add records → Commit → Verify records exist. Begin → Add records → Rollback → Verify records don't exist.
4. **Crash recovery**: Begin → Write journal entries → Close without commit → Reopen (triggers replay) → Verify recovery worked
5. **Compaction workflow**: Add 10 records → Delete 5 → Compact → Verify 5 remain → Validate
6. **Large dataset**: Add 50 records, verify all findable, delete half, compact, verify remainder

- [ ] **Step 2: Run full test suite**

Run: `make clean && make test`
Expected: All test suites compile and pass with zero warnings.

- [ ] **Step 3: Verify C89 compliance**

Run: `gcc -ansi -pedantic -Wall -Werror -Iinclude -fsyntax-only src/db/dbutil.c src/db/db.c src/db/record.c src/db/journal.c src/db/maint.c`

- [ ] **Step 4: Commit**

```bash
git add tests/db/ Makefile
git commit -m "feat(db): add integration tests"
```

- [ ] **Step 5: Submit to QA for review**

Notify QA agent with:
- Files created/modified
- Test results
- C89 compliance verification
- Standards compliance with docs/rules.md

---

## Implementation Notes

### Filename Construction

The `name` parameter to CreateDatabase/OpenDatabase is a base name like `"USERS"` or `"_TEST"`. File extensions are:
- `.DAT` — data file
- `.IDX` — primary index
- `.I00` through `.I14` — secondary indexes
- `.JNL` — journal file

All filenames must be 8.3 DOS-compatible. The library builds full filenames by concatenating: `name + "." + ext`.

### Test File Cleanup

Every test that creates database files must clean them up. Use `remove()` after each test. Use a consistent test database name like `"_TEST"` to make cleanup predictable. Consider a helper function:
```c
static void CleanupTestFiles(void)
{
    remove("_TEST.DAT");
    remove("_TEST.IDX");
    remove("_TEST.JNL");
    remove("_TEST.I00");
    /* etc. */
}
```

### Endian Serialization

Follow the exact same pattern as `src/btree/btree.c`:
```c
static void WriteUint16LE(byte *buf, uint16 val) { ... }
static uint16 ReadUint16LE(const byte *buf) { ... }
static void WriteInt32LE(byte *buf, int32 val) { ... }
static int32 ReadInt32LE(const byte *buf) { ... }
```

Since multiple .c files in the db module need these, either:
- Define them in `dbutil.c` and declare in `db.h`, OR
- Define them as `static` in each file that needs them (DRY violation but simpler)

Recommended: Define once in `dbutil.c`, declare in `db.h` as internal helper functions.

### Cross-File Dependencies Within db Module

The db source files need to call each other's functions:
- `record.c` needs `AllocatePages`, `ReleasePages`, page I/O helpers from `db.c`
- `journal.c` needs page I/O helpers from `db.c`
- `maint.c` needs page I/O helpers from `db.c`

All cross-file functions should be declared in `include/db.h`. Mark internal helpers with a comment like `/* internal */` to distinguish from the public API.

### UpdateRecord and Secondary Indexes

The spec says UpdateRecord "updates indexes if indexed fields changed." However, the db layer doesn't know field layout (it works with raw byte arrays). Two approaches:

1. **Simple (recommended for v1):** UpdateRecord only overwrites data pages. The caller (useradm) is responsible for updating secondary index entries when it changes indexed fields. This matches how the spec's example usage works — the caller explicitly calls DeleteFromIndex/InsertIntoIndex.

2. **Advanced:** Add a callback or field descriptor system. Overkill for this project.

Go with approach 1.

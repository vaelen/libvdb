# Vaelen's Database Design Document

Vaelen's Database (vDB) is a a simple database in the style of database systems commonly used in the 80s and 90s. It is designed to be used in retrocomputing projects where CPU and memory resources are limited. Target systems include DOS, MacOS 6/7/8/9, Linux, and UNIX. It is written in ANSI C for maximum portability.

It's main features include:

1. Data stored in fixed-size blocks (called pages) for efficient random access. 
  - Empty blocks can be reused by future inserts. 
  - Records can use multiple adjacent pages if needed.
2. All changes are first written to a journal file before being written to the database. 
  - Reduces the likelihood of data corruption due to power outage or other system failure.
3. Indexes that speed up searches. 
  - Indexes are stored as B-Trees. 
  - A (required) primary index maps record IDs to physical page numbers. 
  - Additional (optional) indexes use a hash function to map strings to record IDs.
4. Page sizes are set to 512 bytes for more efficient disk access on older computers.
5. Deletion of data only marks the page as empty but does not clean up the data.
  - The database file is regularly compacted, a form of de-fragmentation that relocates active records into free pages to reduce the total size of the data files on disk.

## Data File Format

VDB data files are binary files organized into fixed-size 512-byte pages. This page-based architecture allows for both random access of specific records and easy reuse of freed space when records are removed. The 512-byte page size matches the BTree unit's page size and is optimal for retro system disk I/O.

**Note: Data files are not guaranteed to be readable by a VDB instance running on a different operating system than the one that originally created the data file. This is especially true when moving between 16, 32, and 64 bit systems. Cross-platform compatibility issues include:**
- **Endianness**: Big-endian vs little-endian byte order
- **Record alignment**: Different compilers may pad records differently

**Recommendation**: Always use explicit byte-by-byte serialization (like the BTree unit does) rather than direct record writes for true cross-platform compatibility.

### Files

Each database is actually comprised of a number of files. Let's look at the `Users` database as an example:
- USERS.DAT - The actual data is stored here (includes header in first page)
- USERS.IDX - Primary index mapping Record ID → Page Number (B-Tree)
- USERS.I00 - Secondary index 00 (e.g., username → Record ID)
- USERS.I01 - Secondary index 01 (e.g., email → Record ID)
- USERS.JNL - Journal file for transaction recovery

**Index Architecture**:
- The `.IDX` file is the primary index that maps Record IDs to physical Page Numbers
- All secondary indexes (`.I??` files) map field values to Record IDs
- This indirection allows records to move between pages without updating secondary indexes
- Only the primary `.IDX` index needs updating when a record's physical location changes

### Database Header (Pages 0-1 of .DAT file)

The database header spans the first two pages of the `.DAT` file. Page 0 contains metadata and index definitions, page 1 contains the free page list.

#### Page 0: TDBHeader + Index List

**Header (32 bytes):**
| Name            | Type                          | Notes                                      |
| --------------- | ----------------------------- | ------------------------------------------ |
| Signature       | array[0..4] of Char           | 'VDB' + #0 for file type validation    |
| Version         | TWord                          | Format version (currently 1)               |
| PageSize        | TWord                          | Size of each page in bytes (always 512)    |
| RecordSize      | TWord                          | Size of each record in bytes (1-130050)    |
| RecordCount     | TLong                       | Total number of active records             |
| NextRecordID    | TLong                       | Next available Record ID                   |
| LastCompacted   | TBBSTimestamp                 | Timestamp of last compaction operation     |
| JournalPending  | Boolean                       | True if journal needs replay on open       |
| IndexCount      | Byte                          | Number of secondary indexes (0-15)         |
| Reserved        | array[0..3] of Byte           | Padding to 32 bytes                        |

**Index List (480 bytes):**
| Name            | Type                          | Notes                                      |
| --------------- | ----------------------------- | ------------------------------------------ |
| Indexes         | array[0..14] of TDBIndexInfo  | Secondary index definitions (15 × 32 = 480)|

**Total page 0 size:** 32 + 480 = 512 bytes

#### Page 1: TDBFreeList

| Name            | Type                          | Notes                                      |
| --------------- | ----------------------------- | ------------------------------------------ |
| FreePageCount   | TWord (2 bytes)                | Total count of free pages in database      |
| FreePageListLen | TWord (2 bytes)                | Number of entries in FreePages array (0-127)|
| FreePages       | array[0..126] of TLong      | Page numbers of empty pages (508 bytes)    |

**TDBIndexInfo** (in page 0, after header):
| Name          | Type           | Notes                                           |
| ------------- | -------------- | ----------------------------------------------- |
| FieldName     | String[29]     | The name of the field being indexed (30 bytes)  |
| IndexType     | Byte           | 0 = itID (TLong), 1 = itString (Str63)        |
| IndexNumber   | Byte           | Maps to one of the `I??` files (00-14)          |

**TDBIndexInfo Size**: 30 bytes (String[29] = 1 length byte + 29 data bytes) + 1 byte (IndexType) + 1 byte (IndexNumber) = 32 bytes exactly

**Page 0 Layout**: 32 bytes (header) + 480 bytes (15 indexes × 32 bytes) = 512 bytes

**Index Type Values**:
- **0 (itID)**: Index on a TLong field
- **1 (itString)**: Index on a Str63 field

**Signature**: Used to verify file format. Always 'VDB' followed by null terminator.

**Version**: Format version number. Current version is 1. Future incompatible changes increment this.

**RecordCount**: Tracks active records. Updated on add/delete operations.

**NextRecordID**: Auto-incrementing counter for assigning new Record IDs. Never decreases.

**PageSize**: Always 512 bytes. This constant size matches the BTree unit and simplifies disk I/O.

**RecordSize**: Size of each record in bytes. All records in this database are the same size. This determines how many pages each record occupies:
- **PagesPerRecord** = ceiling(RecordSize / 506)
- Maximum: 65535 bytes (limited by TWord), which is ~130 pages
- Examples: 100 byte record = 1 page, 1000 byte record = 2 pages

**Compaction**: The process of removing empty pages and rebuilding indexes to reclaim disk space. The `LastCompacted` field tracks when this was last done.

**Journal Pending**: Set to `True` when a transaction begins, `False` when committed. If `True` on database open, the journal must be replayed to complete interrupted operations.

**IndexCount**: Number of active secondary indexes (0-15). Maximum 15 indexes supported.

**Reserved**: 4 bytes of padding to bring header to exactly 32 bytes.

**FreePageCount** (in TDBFreeList, page 1): Total count of all free pages in the database (maximum 65535). This counter is incremented when a page is deleted and decremented when a page is allocated. If this value is 0, allocate a new page at end of file. If this value is > 0 but `FreePageListLen = 0`, call `UpdateFreePages` to repopulate the `FreePages` array.

**FreePageListLen** (in TDBFreeList, page 1): Number of valid entries in the `FreePages` array. Maximum 127. The array acts as a stack (LIFO) - new entries are added at index `FreePageListLen`, and pages are allocated by decrementing `FreePageListLen`.

**FreePages** (page 1): Array of up to 127 page numbers that are known to be empty. This provides O(1) free page lookup. When the array is full and more pages are deleted, `FreePageCount` continues to increment but the page numbers are not added to the array. When `FreePages` is empty but `FreePageCount > 0`, `UpdateFreePages` is called to scan the database and refill the array.

**Indexes** (page 0, bytes 32-511): Array of 15 index definitions. Only the first `IndexCount` entries are valid. Each index has a unique `IndexNumber` (0-14) that maps to an `.I??` file.

### Data Page Layout

The `.DAT` file contains a series of pages of type `TDBPage` stored in fixed 512-byte format. Pages are numbered sequentially starting from 0. Pages 0-1 contain the database header (with index list) and free list. Data pages start at page 2.

Records may span multiple consecutive pages depending on `RecordSize`:
- **PagesPerRecord** = ceiling(RecordSize / 506)
- Records are stored in consecutive page runs
- All pages in a multi-page record share the same Record ID
- Only the first page has Status = 1 (Active), additional pages have Status = 2 (Continuation)

| Name   | Type                   | Notes                                    |
| ------ | ---------------------- | ---------------------------------------- |
| ID     | TLong (4 bytes)      | Unique record identifier                 |
| Status | Byte (1 byte)          | 0 = Empty, 1 = Active, 2 = Continuation  |
| Data   | array[0..506] of Byte  | Actual record data (506 bytes per page)  |

**Page Size Calculation**:
- Page size: 512 bytes (constant)
- Overhead: 4 bytes (ID) + 1 byte (Status) = 5 bytes
- Data size: 512 - 5 = 506 bytes available for record data per page (note: changed from 507 to 506 for alignment)

**Free Space Management**:
- When a record is deleted (spanning PagesPerRecord pages):
  - Set all pages `Status` to 0 (Empty)
  - Increment `FreePageCount` by PagesPerRecord
  - Add first page number to free list if room (subsequent pages can be calculated)
- When adding a record:
  - Calculate required pages: `pagesNeeded = ceiling(RecordSize / 506)`
  - If `FreePageCount < pagesNeeded`: Append new pages at end of file
  - Else if `FreePageListLen > 0`:
    - Search free list for consecutive run of pagesNeeded pages
    - Or append to end if no suitable run found
  - Else: Call `UpdateFreePages` to scan and refill array
  - Mark first page Status = 1 (Active), additional pages Status = 2 (Continuation)
  - Decrement `FreePageCount` by pagesNeeded

### Primary Index (.IDX file)

The `.IDX` file is a B-Tree index (as defined by the `BTree` unit) that maps Record IDs to Page Numbers.

**Index Structure**:
- **Keys**: Record IDs (TLong values from `TDBPage.ID`)
- **Values**: Page Numbers (physical page number of the first page where record starts)
- **Purpose**: Translates logical Record IDs to physical page locations

For multi-page records, the index points to the first page only. Subsequent pages are at consecutive positions (pageNum+1, pageNum+2, etc.).

This primary index is updated whenever:
- A record is added (insert RecordID → FirstPageNum mapping)
- A record is moved to a new location during compaction (update RecordID mapping)
- A record is deleted (remove RecordID from index)

### Secondary Indexes (.I?? files)

The `.I??` files contain B-Tree indexes for specific fields. The `??` in the extension should be replaced by the `IndexNumber` value formatted as a 2-digit decimal. For example, `USERS.I00` or `USERS.I01`.

**Index Structure**:
- **Keys**: Field values (generated based on IndexType)
- **Values**: Record IDs (NOT block IDs)
- **Purpose**: Look up records by field values

**Index Key Generation**:
- `itID` type indexes: Use the field value directly as the B-Tree key
- `itString` type indexes: Use the `StringKey()` function to generate a CRC16-based key from the string

**Index Values**: The B-Tree values are Record IDs. To retrieve the actual record, perform a second lookup in the primary `.IDX` index to get the Page Number.

## Journal File Structure

The journal file (`<database>.jnl`) contains pending operations that have not yet been committed. Each journal entry is a fixed-size record that describes an operation to be performed on the database.

### TDBJournalEntry

| Name      | Type                   | Notes                                           |
| --------- | ---------------------- | ----------------------------------------------- |
| Operation | Byte                   | 0 = None/Empty, 1 = Update, 2 = Delete, 3 = Add |
| PageNum   | TLong (4 bytes)      | Page being modified, -1 for Add operations      |
| RecordID  | TLong (4 bytes)      | Record ID for verification                      |
| Data      | array[0..506] of Byte  | New record data (for Update and Add)            |
| Checksum  | TWord (2 bytes)         | CRC16 of Operation + PageNum + RecordID + Data  |

**Journal Entry Size**: 1 + 4 + 4 + 507 + 2 = 518 bytes

**Operation Types**:
- **0 (None/Empty)**: Unused journal entry
- **1 (Update)**: Update existing page at PageNum with new Data
- **2 (Delete)**: Mark page at PageNum as Empty
- **3 (Add)**: Add new record with Data (PageNum is -1, actual page assigned during replay)

**Checksum**: The CRC16 checksum ensures journal entry integrity. Corrupted entries are skipped during replay.

### Example Usage: Find a User by ID (Record ID)

At program start, the program reads the database header from pages 0-1 of the `.DAT` file (header with index list, and free list).

To find a User by their Record ID, the program:

1. **Opens the primary index**: Opens `USERS.IDX` using the `BTree` unit
   ```pascal
   OpenBTree(idxTree, 'USERS.IDX');
   ```

2. **Finds the page number**: Calls `Find(idxTree, recordID, values, count)` where `recordID` is the Record ID to search for
   - The `values` array will contain the physical page number of the first page

3. **Reads the data pages**:
   - Calculate pages needed: `pagesNeeded = ceiling(Header.RecordSize / 506)`
   - For each page i from 0 to pagesNeeded-1:
     - Seeks to position `((pageNum + i) * 512)` in `USERS.DAT`
     - Reads the `TDBPage` record (512 bytes)
     - Verifies `ID = recordID`
     - First page: verify `Status = 1` (Active)
     - Additional pages: verify `Status = 2` (Continuation)
     - Append `Data` to record buffer
   - Returns the assembled record data to the caller

4. **Closes the index**: `CloseBTree(idxTree)`

### Example Usage: Find a User by Username

Finding by a secondary index requires two lookups: secondary index → Record ID, then primary index → Page Number.

1. **Looks up the index**: Search through the index list (page 0, bytes 32-511) for `FieldName = "Username"`
   - Example: `{FieldName: "Username", IndexType: 1 (itString), IndexNumber: 0}`

2. **Opens the secondary index file**: Opens `USERS.I00`
   ```pascal
   OpenBTree(nameTree, 'USERS.I00');
   ```

3. **Generates string key**: `key := StringKey('alice')`
   - This is case-insensitive, so 'alice', 'Alice', 'ALICE' all produce the same key

4. **Finds the Record ID**: `Find(nameTree, key, recordIDs, count)`
   - The `recordIDs` array contains Record IDs (may be multiple due to hash collisions)

5. **Lookup Page Number**: For each Record ID:
   - Open primary index: `OpenBTree(idxTree, 'USERS.IDX')`
   - Find page: `Find(idxTree, recordID, pageNums, count)`
   - Read record from .DAT file (may span multiple consecutive pages)
   - Verify username matches (handle CRC16 hash collisions)
   - Close primary index

6. **Closes the secondary index**: `CloseBTree(nameTree)`

### Example Usage: Adding a New Record

1. **Assign Record ID**: Use `Header.NextRecordID` and increment it

2. **Calculate pages needed**: `pagesNeeded = ceiling(Header.RecordSize / 506)`

3. **Find free pages**:
   - If `FreeList.FreePageCount < pagesNeeded`: Append new pages at end of file
   - Else if `FreeList.FreePageListLen > 0`:
     - Search free list for consecutive run of pagesNeeded pages
     - If found: use those pages and remove from free list
     - If not found: append new pages at end of file
   - Else (count > 0 but list empty):
     - Call `UpdateFreePages` to scan database and refill `FreePages` array
     - Then search for consecutive run

4. **Write data across pages**:
   ```pascal
   offset := 0;
   for i := 0 to pagesNeeded - 1 do begin
     page.ID := newRecordID;
     if i = 0 then
       page.Status := 1  { Active - first page }
     else
       page.Status := 2; { Continuation }

     copyLen := min(506, RecordSize - offset);
     Move(userData[offset], page.Data[0], copyLen);
     { Write to .DAT at (firstPageNum + i) * 512 }
     offset := offset + 506;
   end;
   ```

5. **Update primary index**:
   - Open `USERS.IDX`
   - Insert mapping: `Insert(idxTree, newRecordID, firstPageNum)`
   - Note: Only the first page number is stored

6. **Update all secondary indexes**:
   - For username index: `Insert(nameTree, StringKey(username), newRecordID)`
   - For email index: `Insert(emailTree, StringKey(email), newRecordID)`
   - Note: Secondary indexes store Record IDs, not Page Numbers

7. **Update header and free list**:
   - Increment `Header.RecordCount`
   - Increment `Header.NextRecordID`
   - Decrement `FreeList.FreePageCount` by pagesNeeded
   - Write header back to page 0
   - Write free list back to page 1 (if modified)

### Example Usage: Deleting a Record

1. **Find the record**: Use primary index to get Record ID → First Page Number

2. **Calculate pages to delete**: `pagesNeeded = ceiling(Header.RecordSize / 506)`

3. **Mark all pages as empty**:
   ```pascal
   for i := 0 to pagesNeeded - 1 do begin
     page.Status := 0;  { Empty }
     { Write updated page at (firstPageNum + i) back to .DAT }
   end;
   ```

4. **Update primary index**:
   - `Delete(idxTree, recordID)` - Remove Record ID from primary index

5. **Update secondary indexes**: Remove from all secondary indexes
   - `DeleteValue(nameTree, StringKey(username), recordID)`
   - `DeleteValue(emailTree, StringKey(email), recordID)`

6. **Add to free list**:
   - Increment `FreeList.FreePageCount` by pagesNeeded
   - If `FreeList.FreePageListLen < 127`:
     - Add first page number to `FreePages[FreePageListLen]`
     - Increment `FreePageListLen`
     - Note: Only store first page; consecutive pages can be calculated
   - Else: Page numbers not added to array, but count still incremented

7. **Update header and free list**:
   - Decrement `Header.RecordCount`
   - Write header back to page 0
   - Write free list back to page 1

8. **Compaction**: Eventually run compaction to consolidate pages and rebuild free list accurately

### Example Usage: Updating a Record

**Non-indexed field changes**:
- Simply overwrite the data in the existing pages (all pagesNeeded pages)
- No index updates needed

**Indexed field changes** (e.g., username change):
1. **Delete old secondary index entry**: `DeleteValue(nameTree, StringKey(oldUsername), recordID)`
2. **Update the data pages**: Write new data to existing pages
3. **Insert new secondary index entry**: `Insert(nameTree, StringKey(newUsername), recordID)`
4. **Primary index unchanged**: Record ID stays the same, so primary index is not modified

**Moving a record to a new location** (during compaction):
1. **Write data to new pages**: Copy record to new physical location (pagesNeeded consecutive pages)
2. **Update primary index**: Change mapping from `recordID → oldFirstPageNum` to `recordID → newFirstPageNum`
3. **Mark old pages empty**: Set all old pages `Status = 0`
4. **Secondary indexes unchanged**: They map to Record IDs, so no updates needed

## Transaction Protocol

The journal file provides crash recovery by recording operations before they are committed to the database. This ensures that partial writes due to power loss or crashes can be detected and completed.

### Write Operation Sequence

When updating, deleting, or adding records, follow this sequence:

1. **Append to journal**: Write a `TDBJournalEntry` to the journal file with the operation details
   - For Update: Operation=1, PageNum=target page, RecordID=record ID, Data=new data
   - For Delete: Operation=2, PageNum=target page, RecordID=record ID
   - For Add: Operation=3, PageNum=-1, RecordID=new ID, Data=new data

2. **Set pending flag**: Update `Header.JournalPending = True` in page 0 of .DAT file

3. **Flush journal**: Ensure journal is written to disk (fsync/flush)

4. **Perform database update**: Execute the actual operation on the .DAT file

5. **Update indexes**: Modify all affected indexes (.I?? files)

6. **Clear pending flag**: Set `Header.JournalPending = False` in page 0 of .DAT file

7. **Truncate journal**: Set journal file size to 0 bytes

**Multiple Operations**: For operations affecting multiple records (e.g., bulk updates), write all journal entries before setting the `JournalPending` flag. This creates a multi-operation transaction.

### Recovery on Database Open

When opening a database, check the `JournalPending` flag to determine if recovery is needed:

1. **Check flag**: Read `Header.JournalPending` from page 0 of .DAT file

2. **If false**: Open database normally, no recovery needed

3. **If true**: Recovery is required:
   - Open journal file (.JNL)
   - For each journal entry:
     - Read entry and verify checksum using CRC16
     - If checksum valid, replay operation:
       - **Operation 1 (Update)**: Write `Data` to page `PageNum` in .DAT file
       - **Operation 2 (Delete)**: Set page `PageNum` status to 0 (Empty)
       - **Operation 3 (Add)**: Find first empty page (or append), write `Data`
     - If checksum invalid, skip entry (corrupted)
   - Rebuild all indexes from .DAT file (safer than trusting partial index updates)
   - Set `Header.JournalPending = False` in page 0 of .DAT file
   - Truncate journal file to 0 bytes

**Index Rebuild**: After crash recovery, always rebuild all indexes to ensure consistency. This is safer than attempting to replay partial index updates.

## Database Helper Methods

The `DB` unit should provide high-level helper methods to encapsulate these operations:

### Database Operations

**`OpenDatabase(name: Str63; var db: TDatabase): Boolean`**
- Opens all database files (.INF, .DAT, .IDX, .I??)
- Loads metadata into memory
- Returns true on success

**`CloseDatabase(var db: TDatabase)`**
- Closes all open files
- Writes any cached metadata

**`CreateDatabase(name: Str63; blockSize: TWord): Boolean`**
- Creates new .INF, .DAT, and .IDX files
- Initializes with default values
- Returns true on success

### Record Operations

**`AddRecord(var db: TDatabase; data: array of Byte; var recordID: TLong): Boolean`**
- Finds free block or allocates new one
- Writes data to .DAT file
- Updates all indexes
- Returns new record ID

**`FindRecordByID(var db: TDatabase; id: TLong; var data: array of Byte): Boolean`**
- Uses ID index to locate block
- Reads and returns record data
- Returns true if found

**`FindRecordByString(var db: TDatabase; fieldName: Str63; value: Str63; var data: array of Byte): Boolean`**
- Generates key using StringKey()
- Uses appropriate string index
- Handles hash collisions
- Returns first matching record

**`UpdateRecord(var db: TDatabase; id: TLong; data: array of Byte): Boolean`**
- Finds record by ID
- Overwrites data in existing block
- Updates indexes if indexed fields changed
- Returns true on success

**`DeleteRecord(var db: TDatabase; id: TLong): Boolean`**
- Marks block as empty
- Removes from all indexes
- Returns true on success

### Index Operations

**`AddIndex(var db: TDatabase; fieldName: String[29]; indexType: Byte): Boolean`**
- Creates new .I?? file
- Scans existing records to build index
- Updates index list in page 0 (increment `IndexCount`, add to `Indexes` array)
- Returns true on success
- Maximum 15 indexes supported

**`RebuildIndex(var db: TDatabase; indexNumber: Byte): Boolean`**
- Clears and rebuilds specified index
- Useful for corruption recovery
- Returns true on success

### Maintenance Operations

**`CompactDatabase(var db: TDatabase): Boolean`**
- Removes empty pages by moving active records
- Rebuilds all indexes
- Rebuilds free list with accurate `FreePageCount` and `FreePageListLen`
- Updates LastCompacted timestamp
- Returns true on success

**`UpdateFreePages(var db: TDatabase): Boolean`**
- Scans .DAT file for pages with `Status = 0` (Empty)
- Fills `FreePages` array with up to 127 empty page numbers
- Sets `FreePageListLen` to actual number found
- Sets `FreePageCount` to actual total count of empty pages (for accuracy)
- Called automatically when `FreePageCount > 0` but `FreePageListLen = 0`
- Returns true on success

**`ValidateDatabase(var db: TDatabase): Boolean`**
- Checks for corruption
- Validates indexes against data
- Returns true if database is valid

### Transaction Operations

**`BeginTransaction(var db: TDatabase)`**
- Sets `Header.JournalPending = True`
- Prepares journal file for writing
- All subsequent operations will be journaled

**`CommitTransaction(var db: TDatabase)`**
- Sets `Header.JournalPending = False`
- Truncates journal file to 0 bytes
- Finalizes all pending operations

**`RollbackTransaction(var db: TDatabase)`**
- Discards journal without applying
- Sets `Header.JournalPending = False`
- Truncates journal file to 0 bytes
- Use when aborting an operation

**`ReplayJournal(var db: TDatabase): Boolean`**
- Reads and validates all journal entries
- Applies each operation to database
- Rebuilds all indexes
- Returns true on success
- Called automatically during `OpenDatabase` if `JournalPending = True`

## Data Types

```pascal
type
  TDBIndexInfo = record
    FieldName: String[29];               { Field name (30 bytes total) }
    IndexType: Byte;                     { 0 = itID, 1 = itString }
    IndexNumber: Byte;                   { Index number 0-14 }
  end;

  TDBHeader = record
    Signature: array[0..7] of Char;      { 'RETRODB' + #0 }
    Version: TWord;                        { Format version (1) }
    PageSize: TWord;                       { Size of each page (always 512) }
    RecordSize: TWord;                     { Size of each record in bytes }
    RecordCount: TLong;                 { Total active records }
    NextRecordID: TLong;                { Next available Record ID }
    LastCompacted: TBBSTimestamp;         { Timestamp of last compaction }
    JournalPending: Boolean;              { True if journal needs replay }
    IndexCount: Byte;                     { Number of secondary indexes (0-15) }
    Reserved: array[0..3] of Byte;        { Padding to 32 bytes }
    Indexes: array[0..14] of TDBIndexInfo; { Secondary index definitions }
  end;

  TDBFreeList = record
    FreePageCount: TWord;                  { Total count of free pages in DB }
    FreePageListLen: TWord;                { Number of entries in FreePages array }
    FreePages: array[0..126] of TLong;  { Page numbers of empty pages }
  end;

  TDBPage = record
    ID: TLong;       { Record ID }
    Status: Byte;      { 0 = Empty, 1 = Active, 2 = Continuation }
    Data: array[0..506] of Byte;
  end;

  TDBJournalEntry = record
    Operation: Byte;  { 0 = None, 1 = Update, 2 = Delete, 3 = Add }
    PageNum: TLong;
    RecordID: TLong;
    Data: array[0..506] of Byte;
    Checksum: TWord;
  end;

  TDatabase = record
    Header: TDBHeader;                  { Includes index list }
    FreeList: TDBFreeList;
    DataFile: File;
    JournalFile: File;
    PrimaryIndex: TBTree;               { .IDX - RecordID → PageNum }
    SecondaryIndexes: array of TBTree;  { .I?? - FieldValue → RecordID }
    IsOpen: Boolean;
  end;
```

## Performance Considerations

- **Primary index lookups**: O(log n) via B-Tree - fast
- **Secondary index lookups**: O(log n) + O(log n) = two B-Tree lookups (secondary → primary)
  - Trade-off: Slower reads for faster compaction and updates
- **Finding free pages**:
  - O(1) when `FreePageListLen > 0` (use array)
  - O(1) when `FreePageCount = 0` (append new page)
  - O(n) when `FreePageCount > 0` but `FreePageListLen = 0` (scan to refill array)
    - After scan, next 127 allocations are O(1)
  - `FreePageCount` tracks total free pages for quick "are there any?" check
  - Array holds up to 127 page numbers for fast access
- **Multiple indexes**: Each insert/update/delete must update primary + all secondary indexes
- **Hash collisions**: StringKey uses CRC16, so collisions are possible (1 in 65536)
  - Always verify string match after hash lookup
- **Journal overhead**: Each write operation requires:
  - One journal file append (518 bytes per entry, fsync)
  - One header update (set JournalPending flag in page 0)
  - Actual database modification
  - Second header update (clear flag)
  - Journal truncate
- **Recovery time**: Proportional to number of journal entries and total records (for index rebuild)
- **Batch operations**: Use multi-entry transactions to amortize journal overhead across multiple operations
- **Compaction advantage**: With indirect indexing, only primary index needs updating when moving records
  - Secondary indexes are unchanged since Record IDs don't change
- **Page alignment**: 512-byte pages align well with disk sector sizes on retro systems
- **Free list size**: 4 bytes header + 127 entries × 4 bytes = 512 bytes (fits in one 512-byte page perfectly)

## Future Enhancements

1. **Extended free list**: Linked list of free list pages for tracking more than 128 empty pages
2. **Multi-operation transactions**: Group related operations into atomic units
3. **Record-level checksums**: Detect data corruption in .DAT file
4. **Compression**: For large text fields
5. **Variable-length records**: More space-efficient storage
6. **Index caching**: Keep frequently-used index nodes in memory
7. **Incremental index updates**: During recovery, replay index changes from journal instead of full rebuild

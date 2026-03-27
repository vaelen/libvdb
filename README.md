# vDB - Vaelen's Database

A portable ANSI C89 database system targeting retro and modern platforms
(DOS, Mac OS 6-9, Linux, UNIX, Windows).

## Architecture

The system is built in layers, each depending only on layers below it:

```
   db       Page-based database with journaling and indexing
   |
  btree     File-based B-Tree for indexes
   |
  crc       CRC-16 and SHA-1 hashing
   |
  util      Portable types, safe string helpers
   |
  test      Minimal unit testing framework
```

## Libraries

### test

Minimal unit testing framework with no dynamic memory allocation.
Supports up to 256 tests per suite.

**Header:** `include/test.h`

```c
#include "test.h"

void test_example(void) {
    TestAssertTrue(1 + 1 == 2, "basic arithmetic");
    TestAssertEq(4, 2 + 2, "addition");
    TestAssertStrEq("hello", "hello", "string match");
}

int main(void) {
    TestInit("Example Suite");
    TestAdd("example", test_example);
    return TestRun();
}
```

**Functions:**

| Function           | Description                                 |
|--------------------|---------------------------------------------|
| `TestInit`         | Initialize a test suite by name             |
| `TestAdd`          | Register a test function                    |
| `TestRun`          | Run all tests, return 0 on success          |
| `TestAssertTrue`   | Assert a condition is true                  |
| `TestAssertEq`     | Assert two `long` values are equal          |
| `TestAssertNeq`    | Assert two `long` values are not equal      |
| `TestAssertStrEq`  | Assert two strings are equal                |
| `TestAssertStrNeq` | Assert two strings are not equal            |
| `TestResetFail`    | Reset failure state (for framework testing) |

### util

Portable type definitions and safe string helpers. Provides fixed-width
integer types that work correctly on 16-bit (DOS), 32-bit, and 64-bit
platforms.

**Headers:** `include/util.h`, `include/util/types.h`,
`include/util/string.h`

**Types:**

| Type     | Size   | Description                                  |
|----------|--------|----------------------------------------------|
| `byte`   | 8-bit  | Unsigned byte (`unsigned char`)              |
| `int16`  | 16-bit | Signed integer                               |
| `uint16` | 16-bit | Unsigned integer                             |
| `int32`  | 32-bit | Signed integer (portable across DOS/modern)  |
| `uint32` | 32-bit | Unsigned integer (portable across DOS/modern)|
| `bool`   | 8-bit  | Boolean (`TRUE` / `FALSE`)                   |

**String functions:**

| Function      | Description                                       |
|---------------|---------------------------------------------------|
| `StrToLower`  | Copy string to lowercase, always null-terminates  |
| `StrNCopy`    | Safe string copy, always null-terminates          |
| `StrCompareI` | Case-insensitive string comparison                |

### crc

CRC-16 (CCITT, polynomial 0x1021) and SHA-1 (FIPS 180-4) hashing.
CRC-16 is used for index key generation and journal checksums.
SHA-1 is used for password hashing.

**Header:** `include/crc.h`

```c
#include "crc.h"

/* CRC-16 */
uint16 checksum = Crc16String("hello");

/* SHA-1 */
byte digest[SHA1_DIGEST_SIZE];
char hex[41];
Sha1HashString("password", digest);
Sha1ToHex(digest, hex);
/* hex now contains the 40-character hex digest */
```

**Functions:**

| Function         | Description                                  |
|------------------|----------------------------------------------|
| `Crc16`          | Compute CRC-16 over a byte buffer            |
| `Crc16String`    | Compute CRC-16 of a null-terminated string   |
| `Sha1Init`       | Initialize incremental SHA-1 context         |
| `Sha1Update`     | Feed data into incremental SHA-1             |
| `Sha1Final`      | Finalize SHA-1 and produce 20-byte digest    |
| `Sha1Hash`       | Hash a buffer in one call                    |
| `Sha1HashString` | Hash a null-terminated string                |
| `Sha1ToHex`      | Convert 20-byte digest to 40-char hex string |

### btree

File-based B-Tree where both keys and values are `int32`. Data is stored
in 512-byte pages with little-endian byte order. Keys within a leaf are
sorted. A key may map to multiple values (multi-value index).

Currently uses a simplified single-leaf implementation (no node
splitting), suitable for small to medium datasets.

**Header:** `include/btree.h`

```c
#include "btree.h"

BTree tree;
int32 values[10];
int16 count;

CreateBTree("INDEX.IDX");
OpenBTree(&tree, "INDEX.IDX");

BTreeInsert(&tree, 42, 100);    /* key=42, value=100 */
BTreeInsert(&tree, 42, 200);    /* second value for key 42 */

BTreeFind(&tree, 42, values, 10, &count);
/* count == 2, values == {100, 200} */

BTreeDelete(&tree, 42);         /* remove key and all values */
CloseBTree(&tree);
```

**Functions:**

| Function           | Description                                         |
|--------------------|-----------------------------------------------------|
| `CreateBTree`      | Create a new B-Tree file                            |
| `OpenBTree`        | Open an existing B-Tree file                        |
| `CloseBTree`       | Flush header and close file                         |
| `BTreeInsert`      | Insert a key-value pair                             |
| `BTreeFind`        | Find all values for a key                           |
| `BTreeDelete`      | Delete a key and all its values                     |
| `BTreeDeleteValue` | Delete a specific value from a key                  |
| `StringKey`        | Generate a CRC-16 key from a string (case-insensitive) |

### db

Page-based database with journaling and B-Tree indexing. Records are
stored in fixed-size 512-byte pages and may span multiple pages. A
primary index maps Record IDs to page numbers. Secondary indexes map
field values to Record IDs via CRC-16 hashing. All multi-byte values
are little-endian.

**Header:** `include/db.h`

**File format:**

| File          | Purpose                                          |
|---------------|--------------------------------------------------|
| `.DAT`        | Page data (page 0 = header, page 1 = free list)  |
| `.IDX`        | Primary index (Record ID to page number)         |
| `.I00`-`.I14` | Secondary indexes (up to 15)                     |
| `.JNL`        | Write-ahead journal for crash recovery           |

```c
#include "db.h"

Database db;
byte record[128];
int32 id;

CreateDatabase("MYDATA", 128);
OpenDatabase("MYDATA", &db);

/* Add a record */
memset(record, 0, 128);
memcpy(record, "Hello", 5);
AddRecord(&db, record, &id);

/* Find by ID */
FindRecordByID(&db, id, record);

/* Update */
memcpy(record, "World", 5);
UpdateRecord(&db, id, record);

/* Delete */
DeleteRecord(&db, id);

CloseDatabase(&db);
```

**Database operations:**

| Function         | Description                                  |
|------------------|----------------------------------------------|
| `CreateDatabase` | Create a new database with given record size  |
| `OpenDatabase`   | Open an existing database                    |
| `CloseDatabase`  | Flush and close the database                 |

**Record operations:**

| Function             | Description                            |
|----------------------|----------------------------------------|
| `AddRecord`          | Add a new record, returns assigned ID  |
| `FindRecordByID`     | Find a record by its Record ID         |
| `FindRecordByString` | Find a record via secondary string index |
| `UpdateRecord`       | Update an existing record's data       |
| `DeleteRecord`       | Delete a record by ID                  |

**Index operations:**

| Function       | Description                                       |
|----------------|---------------------------------------------------|
| `AddIndex`     | Add a secondary index to the database             |
| `RebuildIndex` | Rebuild primary (-1) or secondary (0-14) index    |

**Maintenance:**

| Function           | Description                               |
|--------------------|-------------------------------------------|
| `UpdateFreePages`  | Scan and repopulate the free page list    |
| `CompactDatabase`  | Remove gaps and rebuild indexes           |
| `ValidateDatabase` | Check database integrity                  |

**Transactions:**

| Function              | Description                            |
|-----------------------|----------------------------------------|
| `BeginTransaction`    | Start a journal-backed transaction     |
| `CommitTransaction`   | Commit and clear the journal           |
| `RollbackTransaction` | Discard journal without applying       |
| `ReplayJournal`       | Replay journal entries after a crash   |

## Building

Requires a C compiler with ANSI C89 support (e.g., GCC, Clang, Turbo C).

```bash
make            # Build all test binaries and the useradm example
make test       # Build and run the full test suite
make clean      # Remove compiled binaries
```

The compiler is invoked with strict C89 flags:

```
gcc -ansi -pedantic -Wall -Werror -Iinclude
```

### Running individual tests

```bash
./bin/test_test       # Test framework self-tests
./bin/test_types      # Portable type tests
./bin/test_string     # String utility tests
./bin/test_crc16      # CRC-16 tests
./bin/test_sha1       # SHA-1 tests
./bin/test_btree      # B-Tree tests
./bin/test_dbutil     # Database utility tests
./bin/test_db         # Database core tests
./bin/test_record     # Record serialization tests
./bin/test_journal    # Journaling tests
./bin/test_maint      # Maintenance/compaction tests
./bin/test_useradm    # User admin record tests
```

## Examples

### useradm

A menu-driven CLI program for user database administration. Uses ANSI
escape codes for display (no external screen-drawing libraries). Stores
user records in `USERS.DAT` with secondary indexes on username
(`USERS.I00`) and email (`USERS.I01`).

**Source:** `examples/useradm/`

```bash
./bin/useradm
```

**UserRecord fields:**

| Field           | Size     | Description                              |
|-----------------|----------|------------------------------------------|
| `username`      | 32 bytes | Unique login name (case-insensitive)     |
| `real_name`     | 64 bytes | Display name                             |
| `email`         | 64 bytes | Email address (unique, case-insensitive) |
| `password_hash` | 41 bytes | SHA-1 hex string of password             |
| `created_date`  |  4 bytes | Unix timestamp                           |
| `updated_date`  |  4 bytes | Unix timestamp                           |
| `last_seen`     |  4 bytes | Unix timestamp                           |
| `access_level`  |  1 byte  | Permission level (0-255)                 |
| `locked`        |  1 byte  | Account locked flag                      |

## Project Structure

```
include/              Public headers
  test.h              Test framework
  util.h              Utility umbrella header
  util/types.h        Portable type definitions
  util/string.h       Safe string helpers
  crc.h               CRC-16 and SHA-1
  btree.h             B-Tree file format
  db.h                Database API
  useradm.h           User admin structures

src/                  Library source files
  test/test.c         Test framework
  util/string.c       String helpers
  crc/crc.c           CRC-16 implementation
  crc/sha1.c          SHA-1 implementation
  btree/btree.c       B-Tree implementation
  db/dbutil.c         Database utilities
  db/db.c             Database core
  db/record.c         Record serialization
  db/journal.c        Journaling / crash recovery
  db/maint.c          Maintenance and compaction

examples/             Example programs
  useradm/useradm.h   User admin header
  useradm/useradm.c   CLI main program
  useradm/user.c      User record operations
  useradm/search.c    User search / index operations
  useradm/test.c      User admin tests

tests/                Test files
docs/                 Specifications and design documents
```

## Design Principles

- **ANSI C89 only** - No C99+ features, `/* */` comments, variables
  declared at top of functions.
- **8.3 filenames** - All source filenames are DOS-compatible.
- **No external libraries** - Only the standard C library.
- **Caller-allocated memory** - Libraries avoid `malloc`/`free` where
  possible. Callers pass in pointers.
- **Portable byte order** - All on-disk values use explicit little-endian
  serialization, avoiding compiler alignment and padding differences.
- **Platform abstraction** - OS-specific code is isolated behind shared
  header interfaces.

## License

MIT License. Copyright 2026, Andrew C. Young.

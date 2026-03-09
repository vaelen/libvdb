# libvdb

A portable, file-based database library written in C89. Ported from the Pascal BBS database system ([retrobbs-pascal](https://github.com/vaelen/retrobbs-pascal)).

## Target Platforms

- Classic Mac OS 7/8/9 (Metrowerks CodeWarrior, THINK C)
- Linux and FreeBSD (GCC, Clang)
- DOS (Turbo C, Watcom C)
- Windows (MSVC)

## Features

- **Record-oriented database** with fixed-size records, journaling, and free-page management
- **File-backed B-Tree indexes** with duplicate key support and overflow pages
- **Hash functions**: CRC-16/Kermit, CRC-16/XMODEM, CRC-32/CKSUM, SHA-1
- Little-endian on-disk format with 512-byte pages
- Minimal heap allocation — nearly everything lives on the stack or is caller-provided
- Each header (`hash.h`, `btree.h`, `db.h`) can be included independently, or use `vdb.h` for all

## Building

Requires only `make` and a C compiler.

```bash
make            # Build lib/libvdb.a and test binaries
make test       # Build and run all tests
make debug      # Build with debug symbols (-g -DDEBUG)
make release    # Build optimized (-O2 -DNDEBUG)
make unix       # Build without -std=c89 for older Unix systems
make valgrind   # Run tests under valgrind
make clean      # Remove build artifacts
```

## Usage

### Database

```c
#include "vdb.h"

Database db;
int32 record_id;
byte record[128];

/* Create and open a database with 128-byte records */
CreateDatabase("mydb", 128);
OpenDatabase("mydb", &db);

/* Add a record */
memset(record, 0, sizeof(record));
strcpy((char *)record, "Hello, world!");
AddRecord(&db, record, &record_id);

/* Retrieve by ID */
FindRecordByID(&db, record_id, record);

/* Update and delete */
UpdateRecord(&db, record_id, record);
DeleteRecord(&db, record_id);

CloseDatabase(&db);
```

### B-Tree

```c
#include "btree.h"

BTree tree;
int32 values[10];
int16 count;

CreateBTree("index.bt");
OpenBTree(&tree, "index.bt");

BTreeInsert(&tree, 42, 100);
BTreeFind(&tree, 42, values, 10, &count);
BTreeDelete(&tree, 42);

CloseBTree(&tree);
```

### Hash Functions

```c
#include "hash.h"

uint16 crc16  = CRC16("data", 4);
uint16 crc16x = CRC16X("data", 4);
uint32 crc32  = CRC32("data", 4);

byte digest[SHA1_DIGEST_SIZE];
SHA1("data", 4, digest);
```

All hash functions support incremental computation via `Start`/`Add`/`End` variants (e.g. `CRC16Start`, `CRC16Add`, `CRC16End`).

### Secondary Indexes

```c
/* Add a string index on a field */
AddIndex(&db, "username", IT_STRING);

/* Search by indexed field */
byte record[128];
int32 record_id;
FindRecordByString(&db, "username", "alice", record, &record_id);

/* Rebuild indexes after bulk operations */
RebuildAllIndexes(&db);
```

### Transactions

```c
BeginTransaction(&db);
AddRecord(&db, record, &record_id);
CommitTransaction(&db);
/* or RollbackTransaction(&db); */
```

## On-Disk Format

All multi-byte values are stored little-endian. Pages are 512 bytes.

| Page | Contents                          |
| ---- | --------------------------------- |
| 0    | Database header (signature, version, record size, index info) |
| 1    | Free page list                    |
| 2+   | Data pages (records span consecutive pages) |

B-Tree index files use a separate 512-byte page format with header, internal, leaf, and overflow page types.

## License

MIT License. See [LICENSE](LICENSE) for details.

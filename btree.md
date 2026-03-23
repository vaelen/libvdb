# A file based B-Tree implementation

The B-Tree module (`btree.h`/`btree.c`) implements a file-based B-Tree used by the `db` module for indexing. The module is separated out for ease of testing and reuse. The file that the B-Tree is stored in is broken up into 512 byte pages, which aligns with common disk block sizes for 16-bit and 32-bit operating systems. Both keys and values are `int32`. If a key has more values than can be stored in its node, then an overflow page is allocated for them. Overflow pages are stored in a separate page in the same file. The very first page of the file contains a header with various information about the tree.

To create a B-Tree you need to provide the path to the file the tree will be stored in.

## Usage Example

```c
#include "btree.h"

int main(void) {
    BTree tree;
    int32 values[100];
    int16 count;
    int32 userKey;

    /* Create a new B-Tree */
    CreateBTree("index.btree");

    /* Open the B-Tree */
    OpenBTree(&tree, "index.btree");

    /* Insert numeric key-value pairs */
    BTreeInsert(&tree, 100, 200);    /* Key: 100, Value: 200 */
    BTreeInsert(&tree, 100, 201);    /* Multiple values for same key */
    BTreeInsert(&tree, 150, 300);    /* Different key */

    /* Insert using string-based keys */
    userKey = StringKey("alice");
    BTreeInsert(&tree, userKey, 1);  /* User ID 1 for username 'alice' */

    /* Find values for a key */
    if (BTreeFind(&tree, 100, values, 100, &count))
        printf("Found %d values for key 100\n", count);

    /* Find by username (case-insensitive) */
    userKey = StringKey("ALICE");  /* Same key as 'alice' */
    if (BTreeFind(&tree, userKey, values, 100, &count))
        printf("User alice has ID: %ld\n", (long)values[0]);

    /* Delete a specific value */
    BTreeDeleteValue(&tree, 100, 200);

    /* Delete all values for a key */
    BTreeDelete(&tree, 150);

    /* Close the tree */
    CloseBTree(&tree);

    return 0;
}
```

## Implementation Details

### File Structure

The B-Tree file is organized into 512-byte pages with little-endian byte order:

- **Page 0**: Header page containing tree metadata
- **Page 1+**: Node pages (internal nodes, leaf nodes, or overflow pages)

### Page Types

1. **Header Page (Page 0)**
   - Magic number: `"BTRE"` (4 bytes)
   - Format version: `uint16` (2 bytes)
   - Tree order: `uint16` (2 bytes) - maximum children per node
   - Root page number: `int32` (4 bytes)
   - Next free page: `int32` (4 bytes)
   - Total page count: `int32` (4 bytes)

2. **Leaf Node Pages**
   - Page type: `byte` (3 = leaf)
   - Key count: `uint16` (2 bytes)
   - Next leaf pointer: `int32` (4 bytes) - for range queries
   - Entries: Array of leaf entries
     - Each entry contains:
       - Key: `int32` (4 bytes)
       - Value count: `uint16` (2 bytes)
       - Values: Array of `int32` (up to 60 values)
       - Overflow page: `int32` (4 bytes, 0 if none)

3. **Internal Node Pages** (for future expansion)
   - Page type: `byte` (2 = internal)
   - Key count: `uint16`
   - Keys and child page pointers

4. **Overflow Pages** (for future expansion)
   - Page type: `byte` (4 = overflow)
   - Value count: `uint16`
   - Next overflow page: `int32`
   - Values: Array of `int32` (up to 120 values)

### Capacity

With 512-byte pages:
- **Maximum keys per leaf node**: 60
- **Maximum values per key (in-node)**: 60
- **Maximum values in overflow page**: 120
- **Tree order**: 61 (max 60 keys, 61 children)

### Current Limitations

The current implementation is simplified and suitable for small to medium datasets:

1. **No node splitting**: The root is always a leaf node. Once `BT_MAX_KEYS` is reached, inserts fail.
2. **No internal nodes**: Tree does not grow in height, limiting capacity.
3. **Basic overflow**: Overflow pages are allocated but not yet implemented.
4. **Linear search**: Uses linear search within nodes (good for small nodes).

These limitations make the implementation simple and suitable for learning, testing, and small-scale use. Future enhancements can add full B-Tree splitting, balancing, and multi-level trees.

## Data Structures

```c
/* Constants */
#define BT_PAGE_SIZE    512
#define BT_MAX_KEYS      60   /* Maximum keys per node */
#define BT_MAX_VALUES    60   /* Maximum values per key in a leaf */
#define BT_MAX_OVERFLOW  120  /* Maximum values in an overflow page */

/* Page type constants */
#define PT_NONE      0
#define PT_HEADER    1
#define PT_INTERNAL  2
#define PT_LEAF      3
#define PT_OVERFLOW  4

typedef struct {
    char   magic[4];
    uint16 version;
    uint16 order;
    int32  root_page;
    int32  next_free_page;
    int32  page_count;
} BTreeHeader;

typedef struct {
    int32  key;
    uint16 value_count;
    int32  values[BT_MAX_VALUES];
    int32  overflow_page;
} LeafEntry;

typedef struct {
    byte   page_type;
    uint16 key_count;
    int32  next_leaf;
    LeafEntry entries[BT_MAX_KEYS];
} LeafNode;

typedef struct {
    char        filename[256];
    FILE       *fp;
    BTreeHeader header;
    bool        is_open;
} BTree;
```

## Functions

### File Operations

**`bool CreateBTree(const char *filename)`**
- Creates a new B-Tree file
- Initializes header with magic number and metadata
- Creates empty root leaf node
- Returns `true` on success

**`bool OpenBTree(BTree *tree, const char *filename)`**
- Opens an existing B-Tree file
- Reads and validates header (checks magic number)
- Returns `true` on success

**`void CloseBTree(BTree *tree)`**
- Writes updated header to disk
- Closes the file
- Marks tree as not open

### Data Operations

**`bool BTreeInsert(BTree *tree, int32 key, int32 value)`**
- Inserts a key-value pair
- If key exists, adds value to that key's value list
- If key doesn't exist, creates new entry (keys are kept sorted)
- Returns `true` on success

**`bool BTreeFind(BTree *tree, int32 key, int32 *values, int16 max_values, int16 *count)`**
- Finds all values for a given key
- Returns values in the provided array, up to `max_values`
- Sets `count` to number of values found
- Returns `true` if key exists

**`bool BTreeDelete(BTree *tree, int32 key)`**
- Deletes a key and all its associated values
- Shifts remaining entries to fill gap
- Returns `true` on success

**`bool BTreeDeleteValue(BTree *tree, int32 key, int32 value)`**
- Deletes a specific key-value pair
- If no values remain for key, deletes the key entirely
- Returns `true` on success

### Utility Functions

**`int32 StringKey(const char *s)`**
- Generates a B-Tree key from a string
- Converts string to lowercase for case-insensitive lookups
- Computes CRC-16 hash of the lowercase string
- Returns the CRC-16 value as an `int32` key
- Use this to create keys for indexing string values like usernames

Example:
```c
int32 key;
key = StringKey("Username");  /* Same as StringKey("username") */
BTreeInsert(&tree, key, 12345);  /* Index user ID 12345 under this name */
```

## Testing

Run the B-Tree test suite:

```bash
make && ./bin/test_btree
```

The test suite covers:
- Type size verification (catches LP64 issues)
- Header round-trip serialization
- File creation and opening
- Single and multiple key-value insertions
- Multiple values per key
- Key lookups (existing and non-existent)
- Value and key deletion
- Data persistence across close/reopen
- StringKey utility function (consistency, case-insensitivity, uniqueness)

All 10 tests pass successfully.

## Future Enhancements

For production use with larger datasets, consider adding:

1. **Node splitting and merging**: Allow tree to grow beyond single leaf
2. **Internal nodes**: Multi-level B-Tree for better scalability
3. **Overflow page implementation**: Support unlimited values per key
4. **Binary search**: Within nodes for better performance
5. **Range queries**: Leverage next-leaf pointers for scans
6. **Bulk loading**: Efficient initial tree construction
7. **Defragmentation**: Reclaim deleted page space

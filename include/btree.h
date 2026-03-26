/* Copyright 2026, Andrew C. Young <andrew@vaelen.org> */
/* SPDX-License-Identifier: MIT */

/*
 * btree.h - File-based B-Tree for vDB indexes
 *
 * Implements a single-file B-Tree where both keys and values are int32.
 * The file is divided into 512-byte pages with little-endian byte order.
 * Page 0 is the header; page 1+ are leaf/internal/overflow nodes.
 *
 * Current implementation is simplified: the root is always a single
 * leaf node (no splitting), suitable for small to medium datasets.
 *
 * Keys within a leaf are kept in sorted order (linear search).
 * A key may map to multiple values (multi-value index).
 *
 * Depends on: util (types, strings), crc (CRC-16)
 */

#ifndef BTREE_H
#define BTREE_H

#include <stdio.h>
#include "util.h"
#include "crc.h"

/* ---- Constants ---- */

#define BT_PAGE_SIZE    512  /* Bytes per page (matches common disk blocks) */

/*
 * BT_MAX_KEYS - Maximum keys per leaf node (in-memory limit).
 *
 * Note: The on-disk compact serialization format limits the actual
 * number of keys that fit in a single 512-byte page to roughly 36
 * (with 1 value each). SerializeLeaf returns 0 on page overflow,
 * so the on-disk limit is enforced at write time. The in-memory
 * limit being higher than the on-disk limit is acceptable.
 */
#define BT_MAX_KEYS      60  /* Maximum keys per leaf node */
#define BT_MAX_VALUES    60  /* Maximum values per key in a leaf */
#define BT_MAX_OVERFLOW  120 /* Maximum values in an overflow page */

/* ---- Page type constants ---- */

#define PT_NONE      0  /* Unused / free page */
#define PT_HEADER    1  /* Header page (page 0) */
#define PT_INTERNAL  2  /* Internal node (future expansion) */
#define PT_LEAF      3  /* Leaf node */
#define PT_OVERFLOW  4  /* Overflow page (future expansion) */

/* ---- Data Structures ---- */

/*
 * BTreeHeader - Metadata stored in page 0
 *
 * Fields:
 *   magic          - File identifier "BTRE" (4 bytes)
 *   version        - Format version (currently 1)
 *   order          - Maximum children per node (BT_MAX_KEYS + 1)
 *   root_page      - Page number of the root node
 *   next_free_page - Next available page number for allocation
 *   page_count     - Total number of pages in the file
 */
typedef struct {
    char   magic[4];
    uint16 version;
    uint16 order;
    int32  root_page;
    int32  next_free_page;
    int32  page_count;
} BTreeHeader;

/*
 * LeafEntry - A single key and its associated values
 *
 * Fields:
 *   key           - The int32 key
 *   value_count   - Number of values stored for this key
 *   values        - Array of int32 values (up to BT_MAX_VALUES)
 *   overflow_page - Page number of overflow values (0 if none)
 */
typedef struct {
    int32  key;
    uint16 value_count;
    int32  values[BT_MAX_VALUES];
    int32  overflow_page;
} LeafEntry;

/*
 * LeafNode - A leaf page containing sorted key-value entries
 *
 * Fields:
 *   page_type - Must be PT_LEAF
 *   key_count - Number of entries currently stored
 *   next_leaf - Page number of next leaf (0 if none, for range scans)
 *   entries   - Array of leaf entries, sorted by key
 */
typedef struct {
    byte      page_type;
    uint16    key_count;
    int32     next_leaf;
    LeafEntry entries[BT_MAX_KEYS];
} LeafNode;

/*
 * BTree - In-memory handle for an open B-Tree file
 *
 * Fields:
 *   filename - Path to the B-Tree file
 *   fp       - Open file pointer
 *   header   - Cached copy of page 0 header
 *   is_open  - TRUE if the tree is currently open
 */
typedef struct {
    char        filename[256];
    FILE       *fp;
    BTreeHeader header;
    bool        is_open;
} BTree;

/* ---- File Operations ---- */

/*
 * CreateBTree - Create a new B-Tree file
 *
 * Creates a new file with a header page (page 0) and an empty
 * root leaf node (page 1). If the file already exists, it is
 * overwritten.
 *
 * Parameters:
 *   filename - Path to the file to create
 *
 * Returns:
 *   TRUE on success, FALSE on failure
 */
bool CreateBTree(const char *filename);

/*
 * OpenBTree - Open an existing B-Tree file
 *
 * Reads and validates the header (checks magic number "BTRE").
 * The caller provides the BTree struct; this function fills it in.
 *
 * Parameters:
 *   tree     - Pointer to caller-allocated BTree struct
 *   filename - Path to the B-Tree file
 *
 * Returns:
 *   TRUE on success, FALSE if file cannot be opened or magic is invalid
 */
bool OpenBTree(BTree *tree, const char *filename);

/*
 * CloseBTree - Flush header and close the B-Tree file
 *
 * Writes the in-memory header back to page 0, closes the file,
 * and marks the tree as not open.
 *
 * Parameters:
 *   tree - Pointer to an open BTree
 */
void CloseBTree(BTree *tree);

/* ---- Data Operations ---- */

/*
 * BTreeInsert - Insert a key-value pair
 *
 * If the key already exists, the value is added to that key's
 * value list. If the key does not exist, a new entry is created
 * in sorted order. Fails if the leaf is full (BT_MAX_KEYS reached
 * for new keys, or BT_MAX_VALUES reached for existing keys).
 *
 * Parameters:
 *   tree  - Pointer to an open BTree
 *   key   - The key to insert
 *   value - The value to associate with the key
 *
 * Returns:
 *   TRUE on success, FALSE on failure
 */
bool BTreeInsert(BTree *tree, int32 key, int32 value);

/*
 * BTreeFind - Find all values for a given key
 *
 * Searches the tree for the specified key and copies its values
 * into the caller-provided array, up to max_values entries.
 *
 * Parameters:
 *   tree       - Pointer to an open BTree
 *   key        - The key to search for
 *   values     - Caller-provided array to receive values
 *   max_values - Maximum number of values to return
 *   count      - Output: number of values actually found
 *
 * Returns:
 *   TRUE if the key was found, FALSE otherwise
 */
bool BTreeFind(BTree *tree, int32 key, int32 *values,
               int16 max_values, int16 *count);

/*
 * BTreeDelete - Delete a key and all its values
 *
 * Removes the entry for the specified key. Remaining entries
 * are shifted to fill the gap.
 *
 * Parameters:
 *   tree - Pointer to an open BTree
 *   key  - The key to delete
 *
 * Returns:
 *   TRUE if the key was found and deleted, FALSE if not found
 */
bool BTreeDelete(BTree *tree, int32 key);

/*
 * BTreeDeleteValue - Delete a specific value from a key
 *
 * Removes one value from the key's value list. If the key has
 * no remaining values after removal, the key itself is deleted.
 *
 * Parameters:
 *   tree  - Pointer to an open BTree
 *   key   - The key containing the value
 *   value - The specific value to remove
 *
 * Returns:
 *   TRUE if the value was found and deleted, FALSE otherwise
 */
bool BTreeDeleteValue(BTree *tree, int32 key, int32 value);

/* ---- Utility Functions ---- */

/*
 * StringKey - Generate a B-Tree key from a string
 *
 * Converts the string to lowercase, then computes its CRC-16.
 * The result is returned as an int32, providing case-insensitive
 * key generation for indexing string fields like usernames.
 *
 * Note: Uses an internal 256-byte buffer, so the input string
 * is limited to 255 characters. Longer strings are truncated.
 *
 * Parameters:
 *   s - The string to hash (NULL returns 0, max 255 characters)
 *
 * Returns:
 *   CRC-16 of the lowercased string, cast to int32
 */
int32 StringKey(const char *s);

#endif /* BTREE_H */

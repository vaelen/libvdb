/* Copyright 2026, Andrew C. Young <andrew@vaelen.org> */
/* SPDX-License-Identifier: MIT */

/*
 * btree.c - File-based B-Tree implementation for vDB
 *
 * Implements a simplified single-leaf B-Tree stored in a file
 * of 512-byte pages with little-endian byte order.
 *
 * Page 0: header (BTreeHeader serialized into first 20 bytes)
 * Page 1: root leaf node (variable-length compact serialization)
 *
 * On-disk leaf format (variable length, max 512 bytes):
 *   [1] page_type
 *   [2] key_count (uint16 LE)
 *   [4] next_leaf (int32 LE)
 *   For each of key_count entries:
 *     [4] key (int32 LE)
 *     [2] value_count (uint16 LE)
 *     [4 * value_count] values (int32 LE each)
 *     [4] overflow_page (int32 LE)
 */

#include <stdio.h>
#include <string.h>
#include "btree.h"

/* ---- Little-endian serialization helpers ---- */

static void WriteUint16LE(byte *buf, uint16 val)
{
    buf[0] = (byte)(val & 0xFF);
    buf[1] = (byte)((val >> 8) & 0xFF);
}

static uint16 ReadUint16LE(const byte *buf)
{
    return (uint16)((uint16)buf[0] | ((uint16)buf[1] << 8));
}

static void WriteInt32LE(byte *buf, int32 val)
{
    uint32 u = (uint32)val;
    buf[0] = (byte)(u & 0xFF);
    buf[1] = (byte)((u >> 8) & 0xFF);
    buf[2] = (byte)((u >> 16) & 0xFF);
    buf[3] = (byte)((u >> 24) & 0xFF);
}

static int32 ReadInt32LE(const byte *buf)
{
    uint32 u;
    u = (uint32)buf[0]
      | ((uint32)buf[1] << 8)
      | ((uint32)buf[2] << 16)
      | ((uint32)buf[3] << 24);
    return (int32)u;
}

/* ---- Header serialization ---- */

/*
 * SerializeHeader - Write BTreeHeader to a 512-byte page buffer
 *
 * Layout (20 bytes used, rest zeroed):
 *   Bytes 0-3:   magic[4]
 *   Bytes 4-5:   version (uint16 LE)
 *   Bytes 6-7:   order (uint16 LE)
 *   Bytes 8-11:  root_page (int32 LE)
 *   Bytes 12-15: next_free_page (int32 LE)
 *   Bytes 16-19: page_count (int32 LE)
 */
static void SerializeHeader(byte *page, const BTreeHeader *hdr)
{
    memset(page, 0, BT_PAGE_SIZE);
    memcpy(page, hdr->magic, 4);
    WriteUint16LE(page + 4, hdr->version);
    WriteUint16LE(page + 6, hdr->order);
    WriteInt32LE(page + 8, hdr->root_page);
    WriteInt32LE(page + 12, hdr->next_free_page);
    WriteInt32LE(page + 16, hdr->page_count);
}

static void DeserializeHeader(const byte *page, BTreeHeader *hdr)
{
    memcpy(hdr->magic, page, 4);
    hdr->version        = ReadUint16LE(page + 4);
    hdr->order          = ReadUint16LE(page + 6);
    hdr->root_page      = ReadInt32LE(page + 8);
    hdr->next_free_page = ReadInt32LE(page + 12);
    hdr->page_count     = ReadInt32LE(page + 16);
}

/* ---- Leaf node serialization ---- */

/*
 * SerializeLeaf - Write LeafNode to a 512-byte page buffer
 *
 * Uses compact variable-length format: only key_count entries
 * are written, each with only value_count values.
 *
 * Returns the number of bytes written, or 0 on overflow.
 */
static int SerializeLeaf(byte *page, const LeafNode *leaf)
{
    int pos;
    uint16 i;
    uint16 j;

    memset(page, 0, BT_PAGE_SIZE);

    page[0] = leaf->page_type;
    WriteUint16LE(page + 1, leaf->key_count);
    WriteInt32LE(page + 3, leaf->next_leaf);

    pos = 7;
    for (i = 0; i < leaf->key_count; i++) {
        const LeafEntry *e = &leaf->entries[i];
        int entry_size;

        /* 4 (key) + 2 (value_count) + 4*value_count + 4 (overflow) */
        entry_size = 4 + 2 + (4 * (int)e->value_count) + 4;
        if (pos + entry_size > BT_PAGE_SIZE) {
            return 0; /* page overflow */
        }

        WriteInt32LE(page + pos, e->key);
        pos += 4;
        WriteUint16LE(page + pos, e->value_count);
        pos += 2;
        for (j = 0; j < e->value_count; j++) {
            WriteInt32LE(page + pos, e->values[j]);
            pos += 4;
        }
        WriteInt32LE(page + pos, e->overflow_page);
        pos += 4;
    }

    return pos;
}

static bool DeserializeLeaf(const byte *page, LeafNode *leaf)
{
    int pos;
    uint16 i;
    uint16 j;

    memset(leaf, 0, sizeof(LeafNode));

    leaf->page_type = page[0];
    leaf->key_count = ReadUint16LE(page + 1);
    leaf->next_leaf = ReadInt32LE(page + 3);

    /* Validate key_count against in-memory array bounds */
    if (leaf->key_count > BT_MAX_KEYS) {
        leaf->key_count = 0;
        return FALSE;
    }

    pos = 7;
    for (i = 0; i < leaf->key_count; i++) {
        LeafEntry *e = &leaf->entries[i];

        if (pos + 10 > BT_PAGE_SIZE) {
            return FALSE; /* corrupt data */
        }

        e->key = ReadInt32LE(page + pos);
        pos += 4;
        e->value_count = ReadUint16LE(page + pos);
        pos += 2;

        /* Validate value_count against in-memory array bounds */
        if (e->value_count > BT_MAX_VALUES) {
            return FALSE;
        }

        if (pos + (int)e->value_count * 4 + 4 > BT_PAGE_SIZE) {
            return FALSE;
        }

        for (j = 0; j < e->value_count; j++) {
            e->values[j] = ReadInt32LE(page + pos);
            pos += 4;
        }
        e->overflow_page = ReadInt32LE(page + pos);
        pos += 4;
    }

    return TRUE;
}

/* ---- Page I/O helpers ---- */

static bool WritePage(FILE *fp, int32 page_num, const byte *page)
{
    long offset;

    offset = (long)page_num * BT_PAGE_SIZE;
    if (fseek(fp, offset, SEEK_SET) != 0) {
        return FALSE;
    }
    if (fwrite(page, 1, BT_PAGE_SIZE, fp) != BT_PAGE_SIZE) {
        return FALSE;
    }
    fflush(fp);
    return TRUE;
}

static bool ReadPage(FILE *fp, int32 page_num, byte *page)
{
    long offset;

    offset = (long)page_num * BT_PAGE_SIZE;
    if (fseek(fp, offset, SEEK_SET) != 0) {
        return FALSE;
    }
    if (fread(page, 1, BT_PAGE_SIZE, fp) != BT_PAGE_SIZE) {
        return FALSE;
    }
    return TRUE;
}

/* ---- File Operations ---- */

bool CreateBTree(const char *filename)
{
    FILE *fp;
    BTreeHeader hdr;
    LeafNode root;
    byte page[BT_PAGE_SIZE];

    if (filename == NULL) {
        return FALSE;
    }

    fp = fopen(filename, "wb");
    if (fp == NULL) {
        return FALSE;
    }

    /* Initialize header */
    memcpy(hdr.magic, "BTRE", 4);
    hdr.version        = 1;
    hdr.order          = BT_MAX_KEYS + 1;
    hdr.root_page      = 1;
    hdr.next_free_page = 2;
    hdr.page_count     = 2;

    /* Write header page (page 0) */
    SerializeHeader(page, &hdr);
    if (!WritePage(fp, 0, page)) {
        fclose(fp);
        return FALSE;
    }

    /* Initialize empty root leaf (page 1) */
    memset(&root, 0, sizeof(LeafNode));
    root.page_type = PT_LEAF;
    root.key_count = 0;
    root.next_leaf = 0;

    if (SerializeLeaf(page, &root) == 0) {
        fclose(fp);
        return FALSE;
    }
    if (!WritePage(fp, 1, page)) {
        fclose(fp);
        return FALSE;
    }

    fclose(fp);
    return TRUE;
}

bool OpenBTree(BTree *tree, const char *filename)
{
    byte page[BT_PAGE_SIZE];

    if (tree == NULL || filename == NULL) {
        return FALSE;
    }

    memset(tree, 0, sizeof(BTree));

    tree->fp = fopen(filename, "r+b");
    if (tree->fp == NULL) {
        return FALSE;
    }

    /* Read and validate header */
    if (!ReadPage(tree->fp, 0, page)) {
        fclose(tree->fp);
        return FALSE;
    }

    DeserializeHeader(page, &tree->header);

    if (memcmp(tree->header.magic, "BTRE", 4) != 0) {
        fclose(tree->fp);
        return FALSE;
    }

    StrNCopy(tree->filename, filename, sizeof(tree->filename));
    tree->is_open = TRUE;

    return TRUE;
}

void CloseBTree(BTree *tree)
{
    byte page[BT_PAGE_SIZE];

    if (tree == NULL || !tree->is_open) {
        return;
    }

    /* Write updated header back to disk */
    SerializeHeader(page, &tree->header);
    WritePage(tree->fp, 0, page);

    fclose(tree->fp);
    tree->fp = NULL;
    tree->is_open = FALSE;
}

/* ---- Data Operations ---- */

/*
 * ReadRootLeaf - Read the root leaf node from disk
 */
static bool ReadRootLeaf(BTree *tree, LeafNode *leaf)
{
    byte page[BT_PAGE_SIZE];

    if (!ReadPage(tree->fp, tree->header.root_page, page)) {
        return FALSE;
    }
    return DeserializeLeaf(page, leaf);
}

/*
 * WriteRootLeaf - Write the root leaf node to disk
 */
static bool WriteRootLeaf(BTree *tree, const LeafNode *leaf)
{
    byte page[BT_PAGE_SIZE];

    if (SerializeLeaf(page, leaf) == 0) {
        return FALSE;
    }
    return WritePage(tree->fp, tree->header.root_page, page);
}

/*
 * FindEntryIndex - Find the index of a key in the leaf
 *
 * Returns the index if found, or -1 if not found.
 */
static int FindEntryIndex(const LeafNode *leaf, int32 key)
{
    uint16 i;

    for (i = 0; i < leaf->key_count; i++) {
        if (leaf->entries[i].key == key) {
            return (int)i;
        }
        /* Keys are sorted; if we've passed it, stop early */
        if (leaf->entries[i].key > key) {
            return -1;
        }
    }
    return -1;
}

bool BTreeInsert(BTree *tree, int32 key, int32 value)
{
    LeafNode leaf;
    int idx;
    LeafEntry *entry;
    uint16 i;
    uint16 insert_pos;

    if (tree == NULL || !tree->is_open) {
        return FALSE;
    }

    if (!ReadRootLeaf(tree, &leaf)) {
        return FALSE;
    }

    /* Check if key already exists */
    idx = FindEntryIndex(&leaf, key);
    if (idx >= 0) {
        /* Key exists: add value to its list */
        entry = &leaf.entries[idx];

        /* Check for duplicate value -- no-op if already present */
        for (i = 0; i < entry->value_count; i++) {
            if (entry->values[i] == value) {
                return TRUE;
            }
        }

        if (entry->value_count >= BT_MAX_VALUES) {
            return FALSE; /* value list full */
        }
        entry->values[entry->value_count] = value;
        entry->value_count++;
        return WriteRootLeaf(tree, &leaf);
    }

    /* Key does not exist: insert new entry in sorted position */
    if (leaf.key_count >= BT_MAX_KEYS) {
        return FALSE; /* leaf full */
    }

    /* Find insertion position to keep keys sorted */
    insert_pos = leaf.key_count;
    for (i = 0; i < leaf.key_count; i++) {
        if (leaf.entries[i].key > key) {
            insert_pos = i;
            break;
        }
    }

    /* Shift entries to make room */
    for (i = leaf.key_count; i > insert_pos; i--) {
        leaf.entries[i] = leaf.entries[i - 1];
    }

    /* Initialize the new entry */
    memset(&leaf.entries[insert_pos], 0, sizeof(LeafEntry));
    leaf.entries[insert_pos].key = key;
    leaf.entries[insert_pos].value_count = 1;
    leaf.entries[insert_pos].values[0] = value;
    leaf.entries[insert_pos].overflow_page = 0;

    leaf.key_count++;

    return WriteRootLeaf(tree, &leaf);
}

bool BTreeFind(BTree *tree, int32 key, int32 *values,
               int16 max_values, int16 *count)
{
    LeafNode leaf;
    int idx;
    const LeafEntry *entry;
    int16 n;
    int16 i;

    if (tree == NULL || !tree->is_open || values == NULL || count == NULL) {
        return FALSE;
    }

    *count = 0;

    if (!ReadRootLeaf(tree, &leaf)) {
        return FALSE;
    }

    idx = FindEntryIndex(&leaf, key);
    if (idx < 0) {
        return FALSE;
    }

    entry = &leaf.entries[idx];
    n = (int16)entry->value_count;
    if (n > max_values) {
        n = max_values;
    }

    for (i = 0; i < n; i++) {
        values[i] = entry->values[i];
    }
    *count = n;

    return TRUE;
}

bool BTreeDelete(BTree *tree, int32 key)
{
    LeafNode leaf;
    int idx;
    uint16 i;

    if (tree == NULL || !tree->is_open) {
        return FALSE;
    }

    if (!ReadRootLeaf(tree, &leaf)) {
        return FALSE;
    }

    idx = FindEntryIndex(&leaf, key);
    if (idx < 0) {
        return FALSE;
    }

    /* Shift entries to fill the gap */
    for (i = (uint16)idx; i < leaf.key_count - 1; i++) {
        leaf.entries[i] = leaf.entries[i + 1];
    }
    leaf.key_count--;

    /* Clear the last slot */
    memset(&leaf.entries[leaf.key_count], 0, sizeof(LeafEntry));

    return WriteRootLeaf(tree, &leaf);
}

bool BTreeDeleteValue(BTree *tree, int32 key, int32 value)
{
    LeafNode leaf;
    int idx;
    LeafEntry *entry;
    uint16 i;
    bool found;

    if (tree == NULL || !tree->is_open) {
        return FALSE;
    }

    if (!ReadRootLeaf(tree, &leaf)) {
        return FALSE;
    }

    idx = FindEntryIndex(&leaf, key);
    if (idx < 0) {
        return FALSE;
    }

    entry = &leaf.entries[idx];

    /* Find the value in the entry's value list */
    found = FALSE;
    for (i = 0; i < entry->value_count; i++) {
        if (entry->values[i] == value) {
            found = TRUE;
            break;
        }
    }

    if (!found) {
        return FALSE;
    }

    /* Shift values to fill the gap */
    for (; i < entry->value_count - 1; i++) {
        entry->values[i] = entry->values[i + 1];
    }
    entry->value_count--;

    /* If no values remain, delete the entire key */
    if (entry->value_count == 0) {
        for (i = (uint16)idx; i < leaf.key_count - 1; i++) {
            leaf.entries[i] = leaf.entries[i + 1];
        }
        leaf.key_count--;
        memset(&leaf.entries[leaf.key_count], 0, sizeof(LeafEntry));
    }

    return WriteRootLeaf(tree, &leaf);
}

/* ---- Utility Functions ---- */

int32 StringKey(const char *s)
{
    char lower[256];

    if (s == NULL) {
        return 0;
    }

    StrToLower(lower, s, sizeof(lower));
    return (int32)Crc16String(lower);
}

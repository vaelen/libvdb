/*
 * MIT License
 *
 * Copyright 2026, Andrew C. Young <andrew@vaelen.org>
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

#ifndef BTREE_H
#define BTREE_H

#include <stdio.h>

#include "vdbtypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Constants */
#define BT_PAGE_SIZE  512
#define BT_MAX_KEYS    60
#define BT_MAX_VALUES  60
#define BT_MAX_OVERFLOW 120

/* Page types */
#define PT_NONE      0
#define PT_HEADER    1
#define PT_INTERNAL  2
#define PT_LEAF      3
#define PT_OVERFLOW  4

/* B-Tree header (stored in page 0) */
typedef struct {
    char   magic[4];
    uint16 version;
    uint16 order;
    int32  root_page;
    int32  next_free_page;
    int32  page_count;
} BTreeHeader;

/* Leaf entry with overflow support */
typedef struct {
    int32  key;
    uint16 value_count;
    int32  values[BT_MAX_VALUES];
    int32  overflow_page;
} LeafEntry;

/* Leaf node in memory */
typedef struct {
    byte   page_type;
    uint16 key_count;
    int32  next_leaf;
    LeafEntry entries[BT_MAX_KEYS];
} LeafNode;

/* B-Tree handle */
typedef struct {
    char        filename[256];
    FILE       *fp;
    BTreeHeader header;
    bool        is_open;
} BTree;

/* Tree operations */
bool CreateBTree(const char *filename);
bool OpenBTree(BTree *tree, const char *filename);
void CloseBTree(BTree *tree);

bool BTreeInsert(BTree *tree, int32 key, int32 value);
bool BTreeFind(BTree *tree, int32 key, int32 *values, int16 max_values, int16 *count);
bool BTreeDelete(BTree *tree, int32 key);
bool BTreeDeleteValue(BTree *tree, int32 key, int32 value);

/* Utility */
int32 StringKey(const char *s);

#ifdef __cplusplus
}
#endif

#endif /* BTREE_H */

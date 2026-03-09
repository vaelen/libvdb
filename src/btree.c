/*
 * VDB File-Based B-Tree Index
 *
 * Keys and values are int32. Supports multi-value keys.
 * Stored in 512-byte pages. Ported from btree.pas.
 */

#include "internal.h"
#include "btree.h"
#include "hash.h"

#define BTREE_MAGIC "BTRE"
#define BTREE_VERSION 1
#define BTREE_ORDER 61

/* Write header to page 0 */
static void writeHeader(BTree *tree) {
    byte page[BT_PAGE_SIZE];
    memset(page, 0, BT_PAGE_SIZE);

    memcpy(page, tree->header.magic, 4);
    PUT_LE16(page + 4, tree->header.version);
    PUT_LE16(page + 6, tree->header.order);
    PUT_LE32(page + 8, tree->header.root_page);
    PUT_LE32(page + 12, tree->header.next_free_page);
    PUT_LE32(page + 16, tree->header.page_count);

    fseek(tree->fp, 0L, SEEK_SET);
    fwrite(page, 1, BT_PAGE_SIZE, tree->fp);
    fflush(tree->fp);
}

/* Read header from page 0 */
static int readHeader(BTree *tree) {
    byte page[BT_PAGE_SIZE];

    fseek(tree->fp, 0L, SEEK_SET);
    if (fread(page, 1, BT_PAGE_SIZE, tree->fp) != BT_PAGE_SIZE)
        return 0;

    memcpy(tree->header.magic, page, 4);
    tree->header.version = GET_LE16(page + 4);
    tree->header.order = GET_LE16(page + 6);
    tree->header.root_page = (int32)GET_LE32(page + 8);
    tree->header.next_free_page = (int32)GET_LE32(page + 12);
    tree->header.page_count = (int32)GET_LE32(page + 16);

    return 1;
}

/* Write a leaf node to disk */
static void writeLeafNode(BTree *tree, int32 page_num, LeafNode *node) {
    byte page[BT_PAGE_SIZE];
    int offset, i, j;

    memset(page, 0, BT_PAGE_SIZE);
    page[0] = PT_LEAF;
    PUT_LE16(page + 1, node->key_count);
    PUT_LE32(page + 3, node->next_leaf);

    offset = 7;
    for (i = 0; i < (int)node->key_count; i++) {
        PUT_LE32(page + offset, node->entries[i].key);
        offset += 4;
        PUT_LE16(page + offset, node->entries[i].value_count);
        offset += 2;
        for (j = 0; j < (int)node->entries[i].value_count; j++) {
            PUT_LE32(page + offset, node->entries[i].values[j]);
            offset += 4;
        }
        PUT_LE32(page + offset, node->entries[i].overflow_page);
        offset += 4;
    }

    fseek(tree->fp, (long)page_num * BT_PAGE_SIZE, SEEK_SET);
    fwrite(page, 1, BT_PAGE_SIZE, tree->fp);
    fflush(tree->fp);
}

/* Read a leaf node from disk */
static int readLeafNode(BTree *tree, int32 page_num, LeafNode *node) {
    byte page[BT_PAGE_SIZE];
    int offset, i, j;

    fseek(tree->fp, (long)page_num * BT_PAGE_SIZE, SEEK_SET);
    if (fread(page, 1, BT_PAGE_SIZE, tree->fp) != BT_PAGE_SIZE)
        return 0;

    node->page_type = page[0];
    node->key_count = GET_LE16(page + 1);
    node->next_leaf = (int32)GET_LE32(page + 3);

    offset = 7;
    for (i = 0; i < (int)node->key_count; i++) {
        node->entries[i].key = (int32)GET_LE32(page + offset);
        offset += 4;
        node->entries[i].value_count = GET_LE16(page + offset);
        offset += 2;
        for (j = 0; j < (int)node->entries[i].value_count; j++) {
            node->entries[i].values[j] = (int32)GET_LE32(page + offset);
            offset += 4;
        }
        node->entries[i].overflow_page = (int32)GET_LE32(page + offset);
        offset += 4;
    }

    return 1;
}

/* Public API */

int CreateBTree(const char *filename) {
    FILE *fp;
    BTree tree;
    LeafNode root_node;

    fp = fopen(filename, "wb");
    if (!fp)
        return 0;

    tree.fp = fp;

    memcpy(tree.header.magic, BTREE_MAGIC, 4);
    tree.header.version = BTREE_VERSION;
    tree.header.order = BTREE_ORDER;
    tree.header.root_page = 1;
    tree.header.next_free_page = 2;
    tree.header.page_count = 2;

    writeHeader(&tree);

    memset(&root_node, 0, sizeof(root_node));
    root_node.page_type = PT_LEAF;
    root_node.key_count = 0;
    root_node.next_leaf = 0;

    writeLeafNode(&tree, 1, &root_node);

    fclose(fp);
    return 1;
}

int OpenBTree(BTree *tree, const char *filename) {
    FILE *fp;
    size_t len;

    fp = fopen(filename, "r+b");
    if (!fp)
        return 0;

    tree->fp = fp;
    len = strlen(filename);
    if (len >= sizeof(tree->filename))
        len = sizeof(tree->filename) - 1;
    memcpy(tree->filename, filename, len);
    tree->filename[len] = '\0';
    tree->is_open = 1;

    if (!readHeader(tree)) {
        fclose(fp);
        tree->is_open = 0;
        return 0;
    }

    if (memcmp(tree->header.magic, BTREE_MAGIC, 4) != 0) {
        fclose(fp);
        tree->is_open = 0;
        return 0;
    }

    return 1;
}

void CloseBTree(BTree *tree) {
    if (tree->is_open) {
        writeHeader(tree);
        fclose(tree->fp);
        tree->fp = NULL;
        tree->is_open = 0;
    }
}

int BTreeInsert(BTree *tree, int32 key, int32 value) {
    LeafNode root_node;
    int i, j;

    if (!tree->is_open)
        return 0;

    if (!readLeafNode(tree, tree->header.root_page, &root_node))
        return 0;

    /* Find key or insertion point */
    i = 0;
    while (i < (int)root_node.key_count && root_node.entries[i].key < key)
        i++;

    if (i < (int)root_node.key_count && root_node.entries[i].key == key) {
        /* Key exists, add value */
        if (root_node.entries[i].value_count >= BT_MAX_VALUES)
            return 0; /* overflow not implemented */
        root_node.entries[i].values[root_node.entries[i].value_count] = value;
        root_node.entries[i].value_count++;
    } else {
        /* Insert new key */
        if (root_node.key_count >= BT_MAX_KEYS)
            return 0; /* split not implemented */

        for (j = (int)root_node.key_count; j > i; j--)
            root_node.entries[j] = root_node.entries[j - 1];

        root_node.entries[i].key = key;
        root_node.entries[i].value_count = 1;
        root_node.entries[i].values[0] = value;
        root_node.entries[i].overflow_page = 0;
        root_node.key_count++;
    }

    writeLeafNode(tree, tree->header.root_page, &root_node);
    return 1;
}

int BTreeFind(BTree *tree, int32 key, int32 *values, int16 max_values, int16 *count) {
    LeafNode root_node;
    int i, j;

    *count = 0;

    if (!tree->is_open)
        return 0;

    if (!readLeafNode(tree, tree->header.root_page, &root_node))
        return 0;

    for (i = 0; i < (int)root_node.key_count; i++) {
        if (root_node.entries[i].key == key) {
            for (j = 0; j < (int)root_node.entries[i].value_count; j++) {
                if (*count < max_values) {
                    values[*count] = root_node.entries[i].values[j];
                    (*count)++;
                }
            }
            return 1;
        }
    }

    return 0;
}

int BTreeDelete(BTree *tree, int32 key) {
    LeafNode root_node;
    int i, j;

    if (!tree->is_open)
        return 0;

    if (!readLeafNode(tree, tree->header.root_page, &root_node))
        return 0;

    for (i = 0; i < (int)root_node.key_count; i++) {
        if (root_node.entries[i].key == key) {
            for (j = i; j < (int)root_node.key_count - 1; j++)
                root_node.entries[j] = root_node.entries[j + 1];
            root_node.key_count--;
            writeLeafNode(tree, tree->header.root_page, &root_node);
            return 1;
        }
    }

    return 0;
}

int BTreeDeleteValue(BTree *tree, int32 key, int32 value) {
    LeafNode root_node;
    int i, j, k;

    if (!tree->is_open)
        return 0;

    if (!readLeafNode(tree, tree->header.root_page, &root_node))
        return 0;

    for (i = 0; i < (int)root_node.key_count; i++) {
        if (root_node.entries[i].key == key) {
            for (j = 0; j < (int)root_node.entries[i].value_count; j++) {
                if (root_node.entries[i].values[j] == value) {
                    /* Shift values left */
                    for (k = j; k < (int)root_node.entries[i].value_count - 1; k++)
                        root_node.entries[i].values[k] = root_node.entries[i].values[k + 1];
                    root_node.entries[i].value_count--;

                    /* If no values left, delete the key */
                    if (root_node.entries[i].value_count == 0) {
                        for (k = i; k < (int)root_node.key_count - 1; k++)
                            root_node.entries[k] = root_node.entries[k + 1];
                        root_node.key_count--;
                    }

                    writeLeafNode(tree, tree->header.root_page, &root_node);
                    return 1;
                }
            }
            return 0; /* value not found */
        }
    }

    return 0;
}

int32 StringKey(const char *s) {
    char lower[256];
    int i, len;
    uint16 crc;

    len = (int)strlen(s);
    if (len > 255)
        len = 255;

    for (i = 0; i < len; i++) {
        char c = s[i];
        if (c >= 'A' && c <= 'Z')
            c = c + ('a' - 'A');
        lower[i] = c;
    }

    crc = CRC16(lower, (int16)len);
    return (int32)crc;
}

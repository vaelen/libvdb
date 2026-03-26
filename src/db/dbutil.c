/* Copyright 2026, Andrew C. Young <andrew@vaelen.org> */
/* SPDX-License-Identifier: MIT */

/*
 * dbutil.c - Serialization helpers and utility functions for vDB database
 *
 * Provides little-endian serialization/deserialization for all on-disk
 * structures (DBHeader, DBFreeList, DBPage, DBJournalEntry) and utility
 * functions for page calculation, index key generation, and checksums.
 *
 * All multi-byte values are stored in little-endian byte order.
 */

#include <string.h>
#include "db.h"

/* ---- Little-endian serialization helpers ---- */

void WriteUint16LE(byte *buf, uint16 val)
{
    buf[0] = (byte)(val & 0xFF);
    buf[1] = (byte)((val >> 8) & 0xFF);
}

uint16 ReadUint16LE(const byte *buf)
{
    return (uint16)((uint16)buf[0] | ((uint16)buf[1] << 8));
}

void WriteInt32LE(byte *buf, int32 val)
{
    uint32 u = (uint32)val;
    buf[0] = (byte)(u & 0xFF);
    buf[1] = (byte)((u >> 8) & 0xFF);
    buf[2] = (byte)((u >> 16) & 0xFF);
    buf[3] = (byte)((u >> 24) & 0xFF);
}

int32 ReadInt32LE(const byte *buf)
{
    uint32 u;
    u = (uint32)buf[0]
      | ((uint32)buf[1] << 8)
      | ((uint32)buf[2] << 16)
      | ((uint32)buf[3] << 24);
    return (int32)u;
}

/* ---- DBHeader serialization ---- */

/*
 * Page 0 layout (512 bytes):
 *   Bytes  0-3:   signature[4]
 *   Bytes  4-5:   version (uint16 LE)
 *   Bytes  6-7:   page_size (uint16 LE)
 *   Bytes  8-9:   record_size (uint16 LE)
 *   Bytes 10-13:  record_count (int32 LE)
 *   Bytes 14-17:  next_record_id (int32 LE)
 *   Bytes 18-21:  last_compacted (int32 LE)
 *   Byte  22:     journal_pending (bool)
 *   Byte  23:     index_count
 *   Bytes 24-31:  reserved[8]
 *   Bytes 32-511: indexes[15] (15 x 32 = 480 bytes)
 *     Each index (32 bytes):
 *       Bytes 0-29:  field_name[30]
 *       Byte  30:    index_type
 *       Byte  31:    index_number
 */
void SerializeHeader(const DBHeader *hdr, byte *buf)
{
    int i;
    byte *p;

    memset(buf, 0, DB_PAGE_SIZE);
    memcpy(buf, hdr->signature, 4);
    WriteUint16LE(buf + 4, hdr->version);
    WriteUint16LE(buf + 6, hdr->page_size);
    WriteUint16LE(buf + 8, hdr->record_size);
    WriteInt32LE(buf + 10, hdr->record_count);
    WriteInt32LE(buf + 14, hdr->next_record_id);
    WriteInt32LE(buf + 18, hdr->last_compacted);
    buf[22] = hdr->journal_pending ? 1 : 0;
    buf[23] = hdr->index_count;
    memcpy(buf + 24, hdr->reserved, 8);

    /* Serialize index definitions starting at byte 32 */
    for (i = 0; i < DB_MAX_INDEXES; i++) {
        p = buf + 32 + (i * 32);
        memcpy(p, hdr->indexes[i].field_name, 30);
        p[30] = hdr->indexes[i].index_type;
        p[31] = hdr->indexes[i].index_number;
    }
}

void DeserializeHeader(const byte *buf, DBHeader *hdr)
{
    int i;
    const byte *p;

    memset(hdr, 0, sizeof(DBHeader));
    memcpy(hdr->signature, buf, 4);
    hdr->version = ReadUint16LE(buf + 4);
    hdr->page_size = ReadUint16LE(buf + 6);
    hdr->record_size = ReadUint16LE(buf + 8);
    hdr->record_count = ReadInt32LE(buf + 10);
    hdr->next_record_id = ReadInt32LE(buf + 14);
    hdr->last_compacted = ReadInt32LE(buf + 18);
    hdr->journal_pending = buf[22] ? TRUE : FALSE;
    hdr->index_count = buf[23];
    memcpy(hdr->reserved, buf + 24, 8);

    for (i = 0; i < DB_MAX_INDEXES; i++) {
        p = buf + 32 + (i * 32);
        memcpy(hdr->indexes[i].field_name, p, 30);
        hdr->indexes[i].index_type = p[30];
        hdr->indexes[i].index_number = p[31];
    }
}

/* ---- DBFreeList serialization ---- */

/*
 * Page 1 layout (512 bytes):
 *   Bytes 0-1:   free_page_count (uint16 LE)
 *   Bytes 2-3:   free_page_list_len (uint16 LE)
 *   Bytes 4-511: free_pages[127] (127 x 4 = 508 bytes)
 */
void SerializeFreeList(const DBFreeList *fl, byte *buf)
{
    int i;

    memset(buf, 0, DB_PAGE_SIZE);
    WriteUint16LE(buf, fl->free_page_count);
    WriteUint16LE(buf + 2, fl->free_page_list_len);

    for (i = 0; i < DB_MAX_FREE_PAGES; i++) {
        WriteInt32LE(buf + 4 + (i * 4), fl->free_pages[i]);
    }
}

void DeserializeFreeList(const byte *buf, DBFreeList *fl)
{
    int i;

    memset(fl, 0, sizeof(DBFreeList));
    fl->free_page_count = ReadUint16LE(buf);
    fl->free_page_list_len = ReadUint16LE(buf + 2);

    for (i = 0; i < DB_MAX_FREE_PAGES; i++) {
        fl->free_pages[i] = ReadInt32LE(buf + 4 + (i * 4));
    }
}

/* ---- DBPage serialization ---- */

/*
 * Page layout (512 bytes):
 *   Bytes 0-3:   id (int32 LE)
 *   Byte  4:     status
 *   Byte  5:     reserved
 *   Bytes 6-511: data[506]
 */
void SerializePage(const DBPage *page, byte *buf)
{
    memset(buf, 0, DB_PAGE_SIZE);
    WriteInt32LE(buf, page->id);
    buf[4] = page->status;
    buf[5] = page->reserved;
    memcpy(buf + 6, page->data, DB_PAGE_DATA_SIZE);
}

void DeserializePage(const byte *buf, DBPage *page)
{
    memset(page, 0, sizeof(DBPage));
    page->id = ReadInt32LE(buf);
    page->status = buf[4];
    page->reserved = buf[5];
    memcpy(page->data, buf + 6, DB_PAGE_DATA_SIZE);
}

/* ---- DBJournalEntry serialization ---- */

/*
 * Journal entry layout (518 bytes):
 *   Byte  0:       operation
 *   Bytes 1-4:     page_num (int32 LE)
 *   Bytes 5-8:     record_id (int32 LE)
 *   Bytes 9-515:   data[507]
 *   Bytes 516-517:  checksum (uint16 LE)
 */
void SerializeJournalEntry(const DBJournalEntry *entry, byte *buf)
{
    memset(buf, 0, DB_JOURNAL_ENTRY_SIZE);
    buf[0] = entry->operation;
    WriteInt32LE(buf + 1, entry->page_num);
    WriteInt32LE(buf + 5, entry->record_id);
    memcpy(buf + 9, entry->data, 507);
    WriteUint16LE(buf + 516, entry->checksum);
}

void DeserializeJournalEntry(const byte *buf, DBJournalEntry *entry)
{
    memset(entry, 0, sizeof(DBJournalEntry));
    entry->operation = buf[0];
    entry->page_num = ReadInt32LE(buf + 1);
    entry->record_id = ReadInt32LE(buf + 5);
    memcpy(entry->data, buf + 9, 507);
    entry->checksum = ReadUint16LE(buf + 516);
}

/* ---- Utility Functions ---- */

uint16 CalculatePagesNeeded(uint16 record_size)
{
    if (record_size == 0) {
        return 1;
    }
    return (uint16)((record_size + DB_PAGE_DATA_SIZE - 1) / DB_PAGE_DATA_SIZE);
}

int32 GenerateIndexKey(byte index_type, const byte *value)
{
    if (value == NULL) {
        return 0;
    }

    if (index_type == IT_STRING) {
        return StringKey((const char *)value);
    }

    /* IT_ID: interpret first 4 bytes as int32 */
    return ReadInt32LE(value);
}

void BuildFilename(const char *name, const char *ext,
                   char *out, size_t out_size)
{
    size_t name_len;
    size_t ext_len;

    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    if (name == NULL || ext == NULL) {
        return;
    }

    name_len = strlen(name);
    ext_len = strlen(ext);

    /* Need room for name + '.' + ext + '\0' */
    if (name_len + 1 + ext_len + 1 > out_size) {
        return;
    }

    memcpy(out, name, name_len);
    out[name_len] = '.';
    memcpy(out + name_len + 1, ext, ext_len);
    out[name_len + 1 + ext_len] = '\0';
}

uint16 ComputeJournalChecksum(const DBJournalEntry *entry)
{
    /*
     * Checksum covers: operation (1) + page_num (4) + record_id (4) + data (507) = 516 bytes.
     * We serialize these fields to a temporary buffer and compute CRC-16.
     */
    byte buf[516];

    buf[0] = entry->operation;
    WriteInt32LE(buf + 1, entry->page_num);
    WriteInt32LE(buf + 5, entry->record_id);
    memcpy(buf + 9, entry->data, 507);

    return Crc16(buf, 516);
}

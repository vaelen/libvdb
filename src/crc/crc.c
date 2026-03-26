/* Copyright 2026, Andrew C. Young <andrew@vaelen.org> */
/* SPDX-License-Identifier: MIT */

/*
 * crc.c - CRC-16/CCITT implementation
 *
 * Uses polynomial 0x1021 with initial value 0xFFFF.
 * Lookup table is computed at first use and cached.
 */

#include <stddef.h>
#include <string.h>
#include "crc.h"

/* CRC-16/CCITT lookup table (256 entries) */
static uint16 crc_table[256];
static int    table_ready = 0;

/* Build the CRC-16 lookup table */
static void BuildTable(void)
{
    int i;
    int j;
    uint16 crc;

    for (i = 0; i < 256; i++) {
        crc = (uint16)(i << 8);
        for (j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (uint16)((crc << 1) ^ 0x1021);
            } else {
                crc = (uint16)(crc << 1);
            }
        }
        crc_table[i] = crc;
    }
    table_ready = 1;
}

uint16 Crc16(const byte *data, size_t len)
{
    uint16 crc;
    size_t i;

    if (!table_ready) {
        BuildTable();
    }

    crc = 0xFFFF;
    for (i = 0; i < len; i++) {
        crc = (uint16)((crc << 8) ^
               crc_table[(byte)((crc >> 8) ^ data[i])]);
    }
    return crc;
}

uint16 Crc16String(const char *str)
{
    if (str == NULL) {
        return 0;
    }
    return Crc16((const byte *)str, strlen(str));
}

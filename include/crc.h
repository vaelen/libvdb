/* Copyright 2026, Andrew C. Young <andrew@vaelen.org> */
/* SPDX-License-Identifier: MIT */

/*
 * crc.h - CRC and hashing library for vDB
 *
 * Provides CRC-16 (CCITT) and SHA-1 hashing.
 * CRC-16 is used for index key generation and journal checksums.
 * SHA-1 is used for password hashing.
 */

#ifndef CRC_H
#define CRC_H

#include "util/types.h"
#include <stddef.h>

/* ---- CRC-16 (CCITT, polynomial 0x1021) ---- */

/*
 * Crc16 - Compute CRC-16 over a byte buffer
 *
 * Calculates CRC-16/CCITT over len bytes starting at data.
 * Uses polynomial 0x1021 with initial value 0xFFFF.
 * Returns the 16-bit CRC value.
 */
uint16 Crc16(const byte *data, size_t len);

/*
 * Crc16String - Compute CRC-16 of a null-terminated string
 *
 * Convenience wrapper: computes CRC-16 of strlen(str) bytes.
 * Returns 0 if str is NULL.
 */
uint16 Crc16String(const char *str);

/* ---- SHA-1 (FIPS 180-4) ---- */

/* SHA-1 digest size in bytes */
#define SHA1_DIGEST_SIZE 20

/* SHA-1 block size in bytes */
#define SHA1_BLOCK_SIZE  64

/*
 * Sha1Context - Internal state for incremental SHA-1 hashing
 *
 * Initialize with Sha1Init, feed data with Sha1Update,
 * finalize with Sha1Final. All buffers are caller-provided.
 */
typedef struct {
    uint32 state[5];                /* H0..H4 */
    uint32 count[2];               /* bit count, low/high */
    byte   buffer[SHA1_BLOCK_SIZE]; /* partial block buffer */
} Sha1Context;

/*
 * Sha1Init - Initialize a SHA-1 context
 *
 * Must be called before Sha1Update / Sha1Final.
 */
void Sha1Init(Sha1Context *ctx);

/*
 * Sha1Update - Feed data into the hash
 *
 * May be called multiple times to hash data incrementally.
 */
void Sha1Update(Sha1Context *ctx, const byte *data, size_t len);

/*
 * Sha1Final - Finalize hash and produce the 20-byte digest
 *
 * Writes SHA1_DIGEST_SIZE bytes into digest.
 * The context should not be reused after this call.
 */
void Sha1Final(Sha1Context *ctx, byte *digest);

/*
 * Sha1Hash - Convenience: hash a buffer in one call
 *
 * Computes SHA-1 of len bytes at data, writes 20-byte digest.
 */
void Sha1Hash(const byte *data, size_t len, byte *digest);

/*
 * Sha1HashString - Convenience: hash a null-terminated string
 *
 * Computes SHA-1 of the string (excluding null terminator).
 * Writes 20-byte digest. Does nothing if str is NULL.
 */
void Sha1HashString(const char *str, byte *digest);

/*
 * Sha1ToHex - Convert a 20-byte digest to a 40-char hex string
 *
 * Writes 41 bytes to hex_out (40 hex chars + null terminator).
 * Uses lowercase hex digits.
 */
void Sha1ToHex(const byte *digest, char *hex_out);

#endif /* CRC_H */

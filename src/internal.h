/*
 * VDB Internal Header
 *
 * Endian detection, byte-swap macros, and platform abstractions.
 * This file is NOT part of the public API.
 */

#ifndef VDB_INTERNAL_H
#define VDB_INTERNAL_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "vdb.h"

/* Big-endian detection */
#if defined(__BIG_ENDIAN__) || defined(__ARMEB__) || defined(__THUMBEB__) || \
    defined(__AARCH64EB__) || defined(_MIPSEB) || defined(__MIPSEB) || \
    defined(__MIPSEB__) || defined(__POWERPC__) || defined(__ppc__) || \
    defined(__PPC__) || defined(__MC68K__) || defined(__mc68000__) || \
    defined(THINK_C) || defined(__MWERKS__)
  #define VDB_BIG_ENDIAN
#elif defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__)
  #if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    #define VDB_BIG_ENDIAN
  #endif
#endif

/* Byte-swap macros */
#define SWAP16(x) ((uint16)(((uint16)(x) >> 8) | ((uint16)(x) << 8)))
#define SWAP32(x) ((uint32)( \
    (((uint32)(x) >> 24) & 0x000000FFUL) | \
    (((uint32)(x) >>  8) & 0x0000FF00UL) | \
    (((uint32)(x) <<  8) & 0x00FF0000UL) | \
    (((uint32)(x) << 24) & 0xFF000000UL)))

/* Host <-> little-endian conversion */
#ifdef VDB_BIG_ENDIAN
  #define HTOLE16(x) SWAP16(x)
  #define LE16TOH(x) SWAP16(x)
  #define HTOLE32(x) SWAP32(x)
  #define LE32TOH(x) SWAP32(x)
#else
  #define HTOLE16(x) (x)
  #define LE16TOH(x) (x)
  #define HTOLE32(x) (x)
  #define LE32TOH(x) (x)
#endif

/* Write a little-endian uint16 to a byte buffer */
#define PUT_LE16(buf, val) do { \
    uint16 _v = HTOLE16(val); \
    memcpy((buf), &_v, 2); \
} while(0)

/* Write a little-endian int32/uint32 to a byte buffer */
#define PUT_LE32(buf, val) do { \
    uint32 _v = HTOLE32((uint32)(val)); \
    memcpy((buf), &_v, 4); \
} while(0)

/* Read a little-endian uint16 from a byte buffer */
/* Inline helpers for reading LE values from byte buffers.
   Marked to suppress unused warnings in TUs that only write. */
#if defined(__GNUC__) || defined(__clang__)
  #define VDB_MAYBE_UNUSED __attribute__((unused))
#else
  #define VDB_MAYBE_UNUSED
#endif

static VDB_MAYBE_UNUSED uint16 getLE16(const byte *buf) {
    uint16 v;
    memcpy(&v, buf, 2);
    return LE16TOH(v);
}

static VDB_MAYBE_UNUSED uint32 getLE32(const byte *buf) {
    uint32 v;
    memcpy(&v, buf, 4);
    return LE32TOH(v);
}

#define GET_LE16(buf) getLE16((const byte *)(buf))
#define GET_LE32(buf) getLE32((const byte *)(buf))

/* Platform-specific file truncation */
int ftruncate_(FILE *fp, long size);

/* Epoch conversion */
int32 UnixToMacEpoch(int32 unix_time);
int32 MacToUnixEpoch(int32 mac_time);

#endif /* VDB_INTERNAL_H */

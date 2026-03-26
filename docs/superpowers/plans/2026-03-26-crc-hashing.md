# CRC/Hashing Library Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement a CRC/hashing library providing CRC-16 and SHA-1 algorithms for use by btree (StringKey), database (journal checksums, GenerateIndexKey), and useradm (password hashing).

**Architecture:** Single library in `src/crc/` with primary header `include/crc.h`. CRC-16 uses the CCITT polynomial (0x1021) with a lookup table for performance. SHA-1 follows FIPS 180-4 with all operations on caller-provided buffers (no dynamic allocation). Both algorithms operate on byte arrays and return fixed-size results via caller-provided output parameters.

**Tech Stack:** ANSI C (C89), vDB util library (types), vDB test framework

---

## File Structure

| File                | Responsibility                                      |
| ------------------- | --------------------------------------------------- |
| `include/crc.h`     | Primary header: CRC-16 and SHA-1 public API         |
| `src/crc/crc.c`     | CRC-16 implementation (table + computation)         |
| `src/crc/sha1.c`    | SHA-1 implementation (context, update, final)       |
| `tests/crc/crc16.c` | CRC-16 test suite                                   |
| `tests/crc/sha1.c`  | SHA-1 test suite                                    |

---

### Task 1: Create CRC-16 Header and Failing Tests

**Files:**
- Create: `include/crc.h`
- Create: `tests/crc/crc16.c`

- [ ] **Step 1: Create the public header with CRC-16 declarations**

Create `include/crc.h`:

```c
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

#endif /* CRC_H */
```

- [ ] **Step 2: Write the CRC-16 test suite**

Create `tests/crc/crc16.c`:

```c
/*
 * tests/crc/crc16.c - CRC-16 test suite
 *
 * Tests CRC-16/CCITT implementation against known values
 * and verifies expected properties.
 */

#include <stdio.h>
#include <string.h>
#include "test.h"
#include "crc.h"

/* Test CRC-16 of a known string "123456789" */
static void TestCrc16KnownValue(void)
{
    const byte data[] = {
        '1','2','3','4','5','6','7','8','9'
    };

    /*
     * CRC-16/CCITT with poly 0x1021, init 0xFFFF
     * Known check value for "123456789" is 0x29B1
     */
    TestAssertEq(0x29B1, (long)Crc16(data, 9),
                 "CRC-16 of '123456789' should be 0x29B1");
}

/* Test CRC-16 of empty data */
static void TestCrc16Empty(void)
{
    byte data[1];
    uint16 result;

    result = Crc16(data, 0);
    TestAssertEq(0xFFFF, (long)result,
                 "CRC-16 of empty data should be initial value 0xFFFF");
}

/* Test CRC-16 of a single byte */
static void TestCrc16SingleByte(void)
{
    byte data[1];
    uint16 result;

    data[0] = 0x41; /* 'A' */
    result = Crc16(data, 1);
    TestAssertNeq(0, (long)result, "CRC-16 of single byte should not be 0");
    TestAssertNeq(0xFFFF, (long)result,
                  "CRC-16 of single byte should differ from init");
}

/* Test CRC-16 of NULL returns safe value via Crc16String */
static void TestCrc16Null(void)
{
    TestAssertEq(0, (long)Crc16String(NULL),
                 "CRC-16 of NULL string should return 0");
}

/* Test that different data produces different CRCs */
static void TestCrc16Uniqueness(void)
{
    uint16 crc_a;
    uint16 crc_b;
    uint16 crc_c;

    crc_a = Crc16String("alice");
    crc_b = Crc16String("bob");
    crc_c = Crc16String("carol");

    TestAssertNeq((long)crc_a, (long)crc_b, "alice != bob");
    TestAssertNeq((long)crc_a, (long)crc_c, "alice != carol");
    TestAssertNeq((long)crc_b, (long)crc_c, "bob != carol");
}

/* Test that same input always produces same CRC */
static void TestCrc16Consistency(void)
{
    uint16 first;
    uint16 second;

    first = Crc16String("test data");
    second = Crc16String("test data");
    TestAssertEq((long)first, (long)second,
                 "same input should produce same CRC");
}

/* Test Crc16String convenience wrapper matches Crc16 */
static void TestCrc16StringWrapper(void)
{
    const char *str = "hello world";
    uint16 from_bytes;
    uint16 from_string;

    from_bytes = Crc16((const byte *)str, strlen(str));
    from_string = Crc16String(str);

    TestAssertEq((long)from_bytes, (long)from_string,
                 "Crc16String should match Crc16 on same data");
}

/* Test CRC-16 of empty string */
static void TestCrc16EmptyString(void)
{
    uint16 result;

    result = Crc16String("");
    TestAssertEq(0xFFFF, (long)result,
                 "CRC-16 of empty string should be initial value");
}

int main(void)
{
    TestInit("CRC-16 Tests");

    TestAdd("known value (123456789)", TestCrc16KnownValue);
    TestAdd("empty data", TestCrc16Empty);
    TestAdd("single byte", TestCrc16SingleByte);
    TestAdd("null string", TestCrc16Null);
    TestAdd("uniqueness", TestCrc16Uniqueness);
    TestAdd("consistency", TestCrc16Consistency);
    TestAdd("string wrapper", TestCrc16StringWrapper);
    TestAdd("empty string", TestCrc16EmptyString);

    return TestRun();
}
```

- [ ] **Step 3: Add CRC-16 test target to Makefile**

Add to `Makefile` before the convenience targets section:

```makefile
CRC_SRC  = src/crc/crc.c

test_crc16: $(BINDIR)/test_crc16

$(BINDIR)/test_crc16: tests/crc/crc16.c $(TEST_SRC) $(UTIL_SRC) $(CRC_SRC) include/crc.h
	@mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) -o $(BINDIR)/test_crc16 tests/crc/crc16.c $(TEST_SRC) $(UTIL_SRC) $(CRC_SRC)
```

Update the `all`, `test`, and `.PHONY` targets to include `test_crc16`.

- [ ] **Step 4: Create a stub `src/crc/crc.c` so it compiles but tests fail**

Create `src/crc/crc.c`:

```c
/*
 * crc.c - CRC-16 implementation
 */

#include <stddef.h>
#include <string.h>
#include "crc.h"

uint16 Crc16(const byte *data, size_t len)
{
    (void)data;
    (void)len;
    return 0;
}

uint16 Crc16String(const char *str)
{
    (void)str;
    return 0;
}
```

- [ ] **Step 5: Build and run to verify tests fail**

Run: `make test_crc16 && ./bin/test_crc16`
Expected: Compiles successfully, most tests FAIL (stub returns 0).

---

### Task 2: Implement CRC-16

**Files:**
- Modify: `src/crc/crc.c`

- [ ] **Step 1: Implement CRC-16 with lookup table**

Replace the stub in `src/crc/crc.c` with the full implementation:

```c
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
```

- [ ] **Step 2: Build and run CRC-16 tests**

Run: `make test_crc16 && ./bin/test_crc16`
Expected: All 8 tests PASS.

- [ ] **Step 3: Commit**

```bash
git add include/crc.h src/crc/crc.c tests/crc/crc16.c Makefile
git commit -m "feat: add CRC-16/CCITT implementation with tests"
```

---

### Task 3: Write SHA-1 Failing Tests

**Files:**
- Modify: `include/crc.h` (add SHA-1 declarations)
- Create: `tests/crc/sha1.c`

- [ ] **Step 1: Add SHA-1 declarations to the header**

Append the following to `include/crc.h` before the closing `#endif`:

```c
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
```

- [ ] **Step 2: Write the SHA-1 test suite**

Create `tests/crc/sha1.c`:

```c
/*
 * tests/crc/sha1.c - SHA-1 test suite
 *
 * Tests SHA-1 against FIPS 180-4 test vectors and verifies
 * incremental hashing, hex conversion, and edge cases.
 */

#include <stdio.h>
#include <string.h>
#include "test.h"
#include "crc.h"

/* Helper: compute SHA-1 of a string and return hex */
static void HashToHex(const char *str, char *hex)
{
    byte digest[SHA1_DIGEST_SIZE];
    Sha1HashString(str, digest);
    Sha1ToHex(digest, hex);
}

/*
 * FIPS 180-4 Test Vector 1: "abc"
 * Expected: a9993e364706816aba3e25717850c26c9cd0d89d
 */
static void TestSha1Abc(void)
{
    char hex[41];
    HashToHex("abc", hex);
    TestAssertStrEq("a9993e364706816aba3e25717850c26c9cd0d89d",
                    hex, "SHA-1 of 'abc'");
}

/*
 * FIPS 180-4 Test Vector 2:
 * "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"
 * Expected: 84983e441c3bd26ebaae4aa1f95129e5e54670f1
 */
static void TestSha1LongMsg(void)
{
    char hex[41];
    HashToHex(
        "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
        hex);
    TestAssertStrEq("84983e441c3bd26ebaae4aa1f95129e5e54670f1",
                    hex, "SHA-1 of 448-bit message");
}

/* Test empty string */
static void TestSha1Empty(void)
{
    char hex[41];
    HashToHex("", hex);
    TestAssertStrEq("da39a3ee5e6b4b0d3255bfef95601890afd80709",
                    hex, "SHA-1 of empty string");
}

/* Test incremental hashing produces same result as one-shot */
static void TestSha1Incremental(void)
{
    Sha1Context ctx;
    byte digest_inc[SHA1_DIGEST_SIZE];
    byte digest_one[SHA1_DIGEST_SIZE];
    char hex_inc[41];
    char hex_one[41];

    /* Incremental: "abc" in two parts */
    Sha1Init(&ctx);
    Sha1Update(&ctx, (const byte *)"ab", 2);
    Sha1Update(&ctx, (const byte *)"c", 1);
    Sha1Final(&ctx, digest_inc);
    Sha1ToHex(digest_inc, hex_inc);

    /* One-shot */
    Sha1HashString("abc", digest_one);
    Sha1ToHex(digest_one, hex_one);

    TestAssertStrEq(hex_one, hex_inc,
                    "incremental should match one-shot");
}

/* Test that different inputs produce different hashes */
static void TestSha1Uniqueness(void)
{
    char hex_a[41];
    char hex_b[41];

    HashToHex("password1", hex_a);
    HashToHex("password2", hex_b);
    TestAssertStrNeq(hex_a, hex_b,
                     "different passwords produce different hashes");
}

/* Test consistency: same input always gives same output */
static void TestSha1Consistency(void)
{
    char hex_a[41];
    char hex_b[41];

    HashToHex("consistent", hex_a);
    HashToHex("consistent", hex_b);
    TestAssertStrEq(hex_a, hex_b,
                    "same input should produce same hash");
}

/* Test Sha1ToHex produces lowercase hex */
static void TestSha1HexFormat(void)
{
    char hex[41];
    int i;
    int all_valid;

    HashToHex("test", hex);

    /* Should be exactly 40 characters */
    TestAssertEq(40, (long)strlen(hex), "hex string length");

    /* All characters should be lowercase hex digits */
    all_valid = 1;
    for (i = 0; i < 40; i++) {
        if (!((hex[i] >= '0' && hex[i] <= '9') ||
              (hex[i] >= 'a' && hex[i] <= 'f'))) {
            all_valid = 0;
        }
    }
    TestAssertTrue(all_valid, "all chars are lowercase hex");
}

/* Test digest size constant */
static void TestSha1DigestSize(void)
{
    TestAssertEq(20, SHA1_DIGEST_SIZE, "SHA-1 digest is 20 bytes");
    TestAssertEq(64, SHA1_BLOCK_SIZE, "SHA-1 block is 64 bytes");
}

/* Test hashing data that spans multiple blocks (>64 bytes) */
static void TestSha1MultiBlock(void)
{
    char hex[41];
    /* 80 bytes of 'a' — spans two SHA-1 blocks */
    HashToHex(
        "aaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaa",
        hex);
    /* Known SHA-1 of 80 'a's: 86f33652fcffd7fa1443e246dd34fe5d00e25ffd */
    TestAssertStrEq("86f33652fcffd7fa1443e246dd34fe5d00e25ffd",
                    hex, "SHA-1 of 80 'a' chars");
}

int main(void)
{
    TestInit("SHA-1 Tests");

    TestAdd("FIPS vector: abc", TestSha1Abc);
    TestAdd("FIPS vector: long message", TestSha1LongMsg);
    TestAdd("empty string", TestSha1Empty);
    TestAdd("incremental hashing", TestSha1Incremental);
    TestAdd("uniqueness", TestSha1Uniqueness);
    TestAdd("consistency", TestSha1Consistency);
    TestAdd("hex format", TestSha1HexFormat);
    TestAdd("digest size constants", TestSha1DigestSize);
    TestAdd("multi-block data", TestSha1MultiBlock);

    return TestRun();
}
```

- [ ] **Step 3: Add SHA-1 test target to Makefile**

Add to `Makefile`:

```makefile
SHA1_SRC = src/crc/sha1.c

test_sha1: $(BINDIR)/test_sha1

$(BINDIR)/test_sha1: tests/crc/sha1.c $(TEST_SRC) $(UTIL_SRC) $(CRC_SRC) $(SHA1_SRC) include/crc.h
	@mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) -o $(BINDIR)/test_sha1 tests/crc/sha1.c $(TEST_SRC) $(UTIL_SRC) $(CRC_SRC) $(SHA1_SRC)
```

Update `all`, `test`, and `.PHONY` to include `test_sha1`.

- [ ] **Step 4: Create a stub `src/crc/sha1.c` so it compiles but tests fail**

Create `src/crc/sha1.c`:

```c
/*
 * sha1.c - SHA-1 implementation stub
 */

#include <stddef.h>
#include <string.h>
#include "crc.h"

void Sha1Init(Sha1Context *ctx)
{
    (void)ctx;
}

void Sha1Update(Sha1Context *ctx, const byte *data, size_t len)
{
    (void)ctx;
    (void)data;
    (void)len;
}

void Sha1Final(Sha1Context *ctx, byte *digest)
{
    (void)ctx;
    memset(digest, 0, SHA1_DIGEST_SIZE);
}

void Sha1Hash(const byte *data, size_t len, byte *digest)
{
    (void)data;
    (void)len;
    memset(digest, 0, SHA1_DIGEST_SIZE);
}

void Sha1HashString(const char *str, byte *digest)
{
    (void)str;
    memset(digest, 0, SHA1_DIGEST_SIZE);
}

void Sha1ToHex(const byte *digest, char *hex_out)
{
    (void)digest;
    memset(hex_out, '0', 40);
    hex_out[40] = '\0';
}
```

- [ ] **Step 5: Build and run to verify tests fail**

Run: `make test_sha1 && ./bin/test_sha1`
Expected: Compiles successfully, most tests FAIL (stubs return zeroes).

---

### Task 4: Implement SHA-1

**Files:**
- Modify: `src/crc/sha1.c`

- [ ] **Step 1: Implement SHA-1 per FIPS 180-4**

Replace the stub in `src/crc/sha1.c` with the full implementation:

```c
/*
 * sha1.c - SHA-1 implementation (FIPS 180-4)
 *
 * Processes data in 64-byte (512-bit) blocks.
 * All state is kept in caller-provided Sha1Context.
 */

#include <stddef.h>
#include <string.h>
#include "crc.h"

/* Left-rotate a 32-bit value */
#define ROL(v, n) (((v) << (n)) | ((v) >> (32 - (n))))

/* Store a 32-bit value in big-endian byte order */
static void StoreBE32(byte *dst, uint32 val)
{
    dst[0] = (byte)((val >> 24) & 0xFF);
    dst[1] = (byte)((val >> 16) & 0xFF);
    dst[2] = (byte)((val >>  8) & 0xFF);
    dst[3] = (byte)( val        & 0xFF);
}

/* Load a 32-bit value from big-endian byte order */
static uint32 LoadBE32(const byte *src)
{
    return ((uint32)src[0] << 24) |
           ((uint32)src[1] << 16) |
           ((uint32)src[2] <<  8) |
            (uint32)src[3];
}

/* Process a single 64-byte block */
static void ProcessBlock(Sha1Context *ctx, const byte *block)
{
    uint32 w[80];
    uint32 a, b, c, d, e;
    uint32 f, k, temp;
    int t;

    /* Prepare message schedule */
    for (t = 0; t < 16; t++) {
        w[t] = LoadBE32(block + t * 4);
    }
    for (t = 16; t < 80; t++) {
        w[t] = ROL(w[t-3] ^ w[t-8] ^ w[t-14] ^ w[t-16], 1);
    }

    /* Initialize working variables */
    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];
    e = ctx->state[4];

    /* 80 rounds */
    for (t = 0; t < 80; t++) {
        if (t < 20) {
            f = (b & c) | ((~b) & d);
            k = 0x5A827999UL;
        } else if (t < 40) {
            f = b ^ c ^ d;
            k = 0x6ED9EBA1UL;
        } else if (t < 60) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8F1BBCDCUL;
        } else {
            f = b ^ c ^ d;
            k = 0xCA62C1D6UL;
        }
        temp = ROL(a, 5) + f + e + k + w[t];
        e = d;
        d = c;
        c = ROL(b, 30);
        b = a;
        a = temp;
    }

    /* Update state */
    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
}

void Sha1Init(Sha1Context *ctx)
{
    ctx->state[0] = 0x67452301UL;
    ctx->state[1] = 0xEFCDAB89UL;
    ctx->state[2] = 0x98BADCFEUL;
    ctx->state[3] = 0x10325476UL;
    ctx->state[4] = 0xC3D2E1F0UL;
    ctx->count[0] = 0;
    ctx->count[1] = 0;
}

void Sha1Update(Sha1Context *ctx, const byte *data, size_t len)
{
    size_t buf_used;
    size_t buf_free;
    uint32 bit_count;

    if (len == 0) {
        return;
    }

    buf_used = (size_t)((ctx->count[0] >> 3) & 0x3F);
    buf_free = SHA1_BLOCK_SIZE - buf_used;

    /* Update bit count */
    bit_count = (uint32)(len << 3);
    ctx->count[0] += bit_count;
    if (ctx->count[0] < bit_count) {
        ctx->count[1]++;
    }
    ctx->count[1] += (uint32)(len >> 29);

    /* If we have buffered data and new data fills a block */
    if (buf_used > 0 && len >= buf_free) {
        memcpy(ctx->buffer + buf_used, data, buf_free);
        ProcessBlock(ctx, ctx->buffer);
        data += buf_free;
        len -= buf_free;
        buf_used = 0;
    }

    /* Process full blocks */
    while (len >= SHA1_BLOCK_SIZE) {
        ProcessBlock(ctx, data);
        data += SHA1_BLOCK_SIZE;
        len -= SHA1_BLOCK_SIZE;
    }

    /* Buffer remaining data */
    if (len > 0) {
        memcpy(ctx->buffer + buf_used, data, len);
    }
}

void Sha1Final(Sha1Context *ctx, byte *digest)
{
    byte pad[SHA1_BLOCK_SIZE];
    size_t buf_used;
    size_t pad_len;
    byte len_bytes[8];
    int i;

    /* Save bit count before padding */
    StoreBE32(len_bytes, ctx->count[1]);
    StoreBE32(len_bytes + 4, ctx->count[0]);

    /* Pad: append 0x80, then zeroes, then 8-byte length */
    buf_used = (size_t)((ctx->count[0] >> 3) & 0x3F);
    if (buf_used < 56) {
        pad_len = 56 - buf_used;
    } else {
        pad_len = 120 - buf_used;
    }

    memset(pad, 0, pad_len);
    pad[0] = 0x80;
    Sha1Update(ctx, pad, pad_len);
    Sha1Update(ctx, len_bytes, 8);

    /* Output digest in big-endian */
    for (i = 0; i < 5; i++) {
        StoreBE32(digest + i * 4, ctx->state[i]);
    }
}

void Sha1Hash(const byte *data, size_t len, byte *digest)
{
    Sha1Context ctx;

    Sha1Init(&ctx);
    Sha1Update(&ctx, data, len);
    Sha1Final(&ctx, digest);
}

void Sha1HashString(const char *str, byte *digest)
{
    if (str == NULL) {
        memset(digest, 0, SHA1_DIGEST_SIZE);
        return;
    }
    Sha1Hash((const byte *)str, strlen(str), digest);
}

void Sha1ToHex(const byte *digest, char *hex_out)
{
    static const char hex_chars[] = "0123456789abcdef";
    int i;

    for (i = 0; i < SHA1_DIGEST_SIZE; i++) {
        hex_out[i * 2]     = hex_chars[(digest[i] >> 4) & 0x0F];
        hex_out[i * 2 + 1] = hex_chars[ digest[i]       & 0x0F];
    }
    hex_out[40] = '\0';
}
```

- [ ] **Step 2: Build and run SHA-1 tests**

Run: `make test_sha1 && ./bin/test_sha1`
Expected: All 9 tests PASS.

- [ ] **Step 3: Run all tests to ensure nothing is broken**

Run: `make test`
Expected: All test suites pass (test framework, types, string, crc16, sha1).

- [ ] **Step 4: Commit**

```bash
git add src/crc/sha1.c tests/crc/sha1.c include/crc.h Makefile
git commit -m "feat: add SHA-1 implementation with tests"
```

---

### Task 5: Final Verification and Cleanup

- [ ] **Step 1: Run full test suite**

Run: `make clean && make test`
Expected: All test suites compile and pass with zero warnings.

- [ ] **Step 2: Verify C89 compliance explicitly**

Run: `gcc -ansi -pedantic -Wall -Werror -Iinclude -fsyntax-only src/crc/crc.c src/crc/sha1.c`
Expected: No warnings or errors.

- [ ] **Step 3: Submit to QA for review**

Notify the QA agent that the CRC/hashing library is ready for review. Provide:
- Files created: `include/crc.h`, `src/crc/crc.c`, `src/crc/sha1.c`, `tests/crc/crc16.c`, `tests/crc/sha1.c`
- Test results: 17 tests total (8 CRC-16 + 9 SHA-1), all passing
- Standards: C89 compliant, follows all rules from `docs/rules.md`

/* Copyright 2026, Andrew C. Young <andrew@vaelen.org> */
/* SPDX-License-Identifier: MIT */

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
    /* 80 bytes of 'a' -- spans two SHA-1 blocks */
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

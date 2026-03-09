/*
 * VDB Hash Function Tests
 *
 * Test vectors for CRC-16/KERMIT, CRC-16/XMODEM, CRC-32/CKSUM, and SHA-1.
 */

#include "../include/vdb.h"
#include <stdio.h>
#include <string.h>

static int tests_run = 0;
static int tests_passed = 0;

#define RUN_TEST(test_func) do { \
    printf("  " #test_func "... "); \
    tests_run++; \
    if (test_func()) { \
        printf("PASSED\n"); \
        tests_passed++; \
    } else { \
        printf("FAILED\n"); \
    } \
} while(0)

#define ASSERT_EQ(expected, actual) do { \
    if ((expected) != (actual)) { \
        printf("Expected: 0x%lX, Got: 0x%lX\n", \
               (unsigned long)(expected), (unsigned long)(actual)); \
        return 0; \
    } \
} while(0)

#define ASSERT_MEM_EQ(expected, actual, len) do { \
    if (memcmp((expected), (actual), (len)) != 0) { \
        printf("Memory mismatch\n"); \
        return 0; \
    } \
} while(0)

/* Verify fixed-width type sizes are correct */
int test_type_sizes(void) {
    ASSERT_EQ(1, (int)sizeof(byte));
    ASSERT_EQ(2, (int)sizeof(int16));
    ASSERT_EQ(2, (int)sizeof(uint16));
    ASSERT_EQ(4, (int)sizeof(int32));
    ASSERT_EQ(4, (int)sizeof(uint32));
    return 1;
}

/* CRC-16/KERMIT: "123456789" -> 0x2189 */
int test_crc16_known_vector(void) {
    uint16 result = CRC16("123456789", 9);
    ASSERT_EQ(0x2189, result);
    return 1;
}

int test_crc16_empty(void) {
    uint16 result = CRC16("", 0);
    ASSERT_EQ(0x0000, result);
    return 1;
}

int test_crc16_streaming(void) {
    uint16 crc;
    CRC16Start(&crc, "1234", 4);
    CRC16Add(&crc, "56789", 5);
    ASSERT_EQ(0x2189, CRC16End(&crc));
    return 1;
}

/* CRC-16/XMODEM: "123456789" -> 0x31C3 */
int test_crc16x_known_vector(void) {
    uint16 result = CRC16X("123456789", 9);
    ASSERT_EQ(0x31C3, result);
    return 1;
}

int test_crc16x_streaming(void) {
    uint16 crc;
    CRC16XStart(&crc, "12345", 5);
    CRC16XAdd(&crc, "6789", 4);
    ASSERT_EQ(0x31C3, CRC16XEnd(&crc));
    return 1;
}

/* CRC-32/CKSUM (POSIX): "123456789" -> 0x765E7680 */
int test_crc32_known_vector(void) {
    uint32 result = CRC32("123456789", 9);
    ASSERT_EQ(0x8AEAB5FBUL, result);
    return 1;
}

int test_crc32_streaming(void) {
    uint32 crc, total;
    CRC32Start(&crc, &total, "123", 3);
    CRC32Add(&crc, &total, "456789", 6);
    ASSERT_EQ(0x8AEAB5FBUL, CRC32End(&crc, total));
    return 1;
}

/* SHA-1: "" -> da39a3ee5e6b4b0d3255bfef95601890afd80709 */
int test_sha1_empty(void) {
    byte digest[SHA1_DIGEST_SIZE];
    byte expected[] = {
        0xda, 0x39, 0xa3, 0xee, 0x5e, 0x6b, 0x4b, 0x0d, 0x32, 0x55,
        0xbf, 0xef, 0x95, 0x60, 0x18, 0x90, 0xaf, 0xd8, 0x07, 0x09
    };
    SHA1("", 0, digest);
    ASSERT_MEM_EQ(expected, digest, SHA1_DIGEST_SIZE);
    return 1;
}

/* SHA-1: "abc" -> a9993e364706816aba3e25717850c26c9cd0d89d */
int test_sha1_abc(void) {
    byte digest[SHA1_DIGEST_SIZE];
    byte expected[] = {
        0xa9, 0x99, 0x3e, 0x36, 0x47, 0x06, 0x81, 0x6a, 0xba, 0x3e,
        0x25, 0x71, 0x78, 0x50, 0xc2, 0x6c, 0x9c, 0xd0, 0xd8, 0x9d
    };
    SHA1("abc", 3, digest);
    ASSERT_MEM_EQ(expected, digest, SHA1_DIGEST_SIZE);
    return 1;
}

/* SHA-1 streaming */
int test_sha1_streaming(void) {
    SHA1Context ctx;
    byte digest[SHA1_DIGEST_SIZE];
    byte expected[] = {
        0xa9, 0x99, 0x3e, 0x36, 0x47, 0x06, 0x81, 0x6a, 0xba, 0x3e,
        0x25, 0x71, 0x78, 0x50, 0xc2, 0x6c, 0x9c, 0xd0, 0xd8, 0x9d
    };
    SHA1Start(&ctx);
    SHA1Add(&ctx, "a", 1);
    SHA1Add(&ctx, "bc", 2);
    SHA1End(&ctx, digest);
    ASSERT_MEM_EQ(expected, digest, SHA1_DIGEST_SIZE);
    return 1;
}

/* SHA-1: "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"
   -> 84983e441c3bd26ebaae4aa1f95129e5e54670f1 */
int test_sha1_long(void) {
    byte digest[SHA1_DIGEST_SIZE];
    byte expected[] = {
        0x84, 0x98, 0x3e, 0x44, 0x1c, 0x3b, 0xd2, 0x6e, 0xba, 0xae,
        0x4a, 0xa1, 0xf9, 0x51, 0x29, 0xe5, 0xe5, 0x46, 0x70, 0xf1
    };
    SHA1("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 56, digest);
    ASSERT_MEM_EQ(expected, digest, SHA1_DIGEST_SIZE);
    return 1;
}

int main(void) {
    printf("=== Hash Tests ===\n");

    RUN_TEST(test_type_sizes);
    RUN_TEST(test_crc16_known_vector);
    RUN_TEST(test_crc16_empty);
    RUN_TEST(test_crc16_streaming);
    RUN_TEST(test_crc16x_known_vector);
    RUN_TEST(test_crc16x_streaming);
    RUN_TEST(test_crc32_known_vector);
    RUN_TEST(test_crc32_streaming);
    RUN_TEST(test_sha1_empty);
    RUN_TEST(test_sha1_abc);
    RUN_TEST(test_sha1_streaming);
    RUN_TEST(test_sha1_long);

    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}

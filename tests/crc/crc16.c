/* Copyright 2026, Andrew C. Young <andrew@vaelen.org> */
/* SPDX-License-Identifier: MIT */

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
    byte data[1] = {0};
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

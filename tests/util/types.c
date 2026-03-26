/* Copyright 2026, Andrew C. Young <andrew@vaelen.org> */
/* SPDX-License-Identifier: MIT */

/*
 * tests/util/types.c - Type definition tests for vDB utility library
 *
 * Verifies that all portable types have the correct sizes and
 * that signed types can hold the expected ranges of values.
 */

#include "test.h"
#include "util.h"

/* ---- size tests ------------------------------------------------------- */

static void test_byte_size(void)
{
    TestAssertEq(1, (long)sizeof(byte), "sizeof(byte) == 1");
}

static void test_int16_size(void)
{
    TestAssertEq(2, (long)sizeof(int16), "sizeof(int16) == 2");
}

static void test_uint16_size(void)
{
    TestAssertEq(2, (long)sizeof(uint16), "sizeof(uint16) == 2");
}

static void test_int32_size(void)
{
    TestAssertEq(4, (long)sizeof(int32), "sizeof(int32) == 4");
}

static void test_uint32_size(void)
{
    TestAssertEq(4, (long)sizeof(uint32), "sizeof(uint32) == 4");
}

static void test_bool_size(void)
{
    TestAssertEq(1, (long)sizeof(bool), "sizeof(bool) == 1");
}

/* ---- value tests ------------------------------------------------------ */

static void test_true_false(void)
{
    TestAssertEq(1, (long)TRUE, "TRUE == 1");
    TestAssertEq(0, (long)FALSE, "FALSE == 0");
    TestAssertTrue(TRUE, "TRUE is truthy");
    TestAssertTrue(!FALSE, "FALSE is falsy");
}

static void test_int16_signed_range(void)
{
    int16 neg;
    int16 pos;

    neg = -32768;
    pos = 32767;
    TestAssertTrue(neg < 0, "int16 can hold -32768");
    TestAssertTrue(pos > 0, "int16 can hold 32767");
    TestAssertEq(-32768, (long)neg, "int16 -32768 value correct");
    TestAssertEq(32767, (long)pos, "int16 32767 value correct");
}

static void test_int32_signed_range(void)
{
    int32 neg;
    int32 pos;

    neg = -2147483647L - 1L;
    pos = 2147483647L;
    TestAssertTrue(neg < 0, "int32 can hold negative values");
    TestAssertTrue(pos > 0, "int32 can hold 2147483647");
    TestAssertEq(-2147483647L - 1L, (long)neg, "int32 min value correct");
    TestAssertEq(2147483647L, (long)pos, "int32 max value correct");
}

static void test_uint16_unsigned(void)
{
    uint16 val;

    val = 65535U;
    TestAssertTrue(val > 0, "uint16 can hold 65535");
    TestAssertEq(65535L, (long)val, "uint16 65535 value correct");
}

static void test_uint32_unsigned(void)
{
    uint32 val;

    val = 4294967295UL;
    TestAssertTrue(val > 0, "uint32 can hold 4294967295");
}

/* ---- main ------------------------------------------------------------- */

int main(void)
{
    TestInit("Utility Types");

    TestAdd("byte size",           test_byte_size);
    TestAdd("int16 size",          test_int16_size);
    TestAdd("uint16 size",         test_uint16_size);
    TestAdd("int32 size",          test_int32_size);
    TestAdd("uint32 size",         test_uint32_size);
    TestAdd("bool size",           test_bool_size);
    TestAdd("TRUE and FALSE",      test_true_false);
    TestAdd("int16 signed range",  test_int16_signed_range);
    TestAdd("int32 signed range",  test_int32_signed_range);
    TestAdd("uint16 unsigned",     test_uint16_unsigned);
    TestAdd("uint32 unsigned",     test_uint32_unsigned);

    return TestRun();
}

/* Copyright 2026, Andrew C. Young <andrew@vaelen.org> */
/* SPDX-License-Identifier: MIT */

/*
 * tests/util/string.c - String helper tests for vDB utility library
 *
 * Comprehensive tests for StrToLower, StrNCopy, and StrCompareI,
 * including edge cases and NULL input handling.
 */

#include <string.h> /* memset */
#include "test.h"
#include "util.h"

/* ---- StrToLower tests ------------------------------------------------- */

static void test_tolower_normal(void)
{
    char buf[32];

    StrToLower(buf, "Hello World", sizeof(buf));
    TestAssertStrEq("hello world", buf, "StrToLower normal string");
}

static void test_tolower_already_lower(void)
{
    char buf[32];

    StrToLower(buf, "already lower", sizeof(buf));
    TestAssertStrEq("already lower", buf, "StrToLower already lowercase");
}

static void test_tolower_empty(void)
{
    char buf[32];

    memset(buf, 'X', sizeof(buf));
    StrToLower(buf, "", sizeof(buf));
    TestAssertStrEq("", buf, "StrToLower empty string");
}

static void test_tolower_truncation(void)
{
    char buf[6];

    StrToLower(buf, "ABCDEFGHIJ", sizeof(buf));
    TestAssertStrEq("abcde", buf, "StrToLower truncation at max_len");
}

static void test_tolower_null_src(void)
{
    char buf[32];

    memset(buf, 'X', sizeof(buf));
    StrToLower(buf, NULL, sizeof(buf));
    TestAssertStrEq("", buf, "StrToLower NULL src gives empty string");
}

static void test_tolower_null_dst(void)
{
    /* Should not crash */
    StrToLower(NULL, "hello", 10);
    TestAssertTrue(1, "StrToLower NULL dst does not crash");
}

static void test_tolower_zero_len(void)
{
    char buf[4];

    memset(buf, 'X', sizeof(buf));
    StrToLower(buf, "hello", 0);
    /* buf should be unchanged since max_len is 0 */
    TestAssertTrue(buf[0] == 'X', "StrToLower zero max_len leaves buf unchanged");
}

/* ---- StrNCopy tests --------------------------------------------------- */

static void test_ncopy_normal(void)
{
    char buf[32];

    StrNCopy(buf, "hello", sizeof(buf));
    TestAssertStrEq("hello", buf, "StrNCopy normal copy");
}

static void test_ncopy_truncation(void)
{
    char buf[4];

    StrNCopy(buf, "abcdefgh", sizeof(buf));
    TestAssertStrEq("abc", buf, "StrNCopy truncation");
}

static void test_ncopy_empty(void)
{
    char buf[32];

    memset(buf, 'X', sizeof(buf));
    StrNCopy(buf, "", sizeof(buf));
    TestAssertStrEq("", buf, "StrNCopy empty string");
}

static void test_ncopy_exact_fit(void)
{
    char buf[6];

    StrNCopy(buf, "hello", sizeof(buf));
    TestAssertStrEq("hello", buf, "StrNCopy exact fit (5 chars + null)");
}

static void test_ncopy_null_src(void)
{
    char buf[32];

    memset(buf, 'X', sizeof(buf));
    StrNCopy(buf, NULL, sizeof(buf));
    TestAssertStrEq("", buf, "StrNCopy NULL src gives empty string");
}

static void test_ncopy_null_dst(void)
{
    /* Should not crash */
    StrNCopy(NULL, "hello", 10);
    TestAssertTrue(1, "StrNCopy NULL dst does not crash");
}

static void test_ncopy_zero_len(void)
{
    char buf[4];

    memset(buf, 'X', sizeof(buf));
    StrNCopy(buf, "hello", 0);
    TestAssertTrue(buf[0] == 'X', "StrNCopy zero max_len leaves buf unchanged");
}

/* ---- StrCompareI tests ------------------------------------------------ */

static void test_comparei_equal(void)
{
    TestAssertEq(0, (long)StrCompareI("Hello", "hello"),
                 "StrCompareI equal ignoring case");
}

static void test_comparei_equal_same(void)
{
    TestAssertEq(0, (long)StrCompareI("abc", "abc"),
                 "StrCompareI equal same case");
}

static void test_comparei_less(void)
{
    TestAssertTrue(StrCompareI("abc", "def") < 0,
                   "StrCompareI a < b");
}

static void test_comparei_greater(void)
{
    TestAssertTrue(StrCompareI("def", "abc") > 0,
                   "StrCompareI a > b");
}

static void test_comparei_empty(void)
{
    TestAssertEq(0, (long)StrCompareI("", ""),
                 "StrCompareI two empty strings");
}

static void test_comparei_empty_vs_nonempty(void)
{
    TestAssertTrue(StrCompareI("", "a") < 0,
                   "StrCompareI empty < non-empty");
    TestAssertTrue(StrCompareI("a", "") > 0,
                   "StrCompareI non-empty > empty");
}

static void test_comparei_null_both(void)
{
    TestAssertEq(0, (long)StrCompareI(NULL, NULL),
                 "StrCompareI both NULL");
}

static void test_comparei_null_a(void)
{
    TestAssertTrue(StrCompareI(NULL, "hello") < 0,
                   "StrCompareI NULL < non-NULL");
}

static void test_comparei_null_b(void)
{
    TestAssertTrue(StrCompareI("hello", NULL) > 0,
                   "StrCompareI non-NULL > NULL");
}

static void test_comparei_prefix(void)
{
    TestAssertTrue(StrCompareI("abc", "abcdef") < 0,
                   "StrCompareI shorter prefix < longer");
    TestAssertTrue(StrCompareI("abcdef", "abc") > 0,
                   "StrCompareI longer > shorter prefix");
}

/* ---- main ------------------------------------------------------------- */

int main(void)
{
    TestInit("Utility Strings");

    /* StrToLower */
    TestAdd("StrToLower normal",       test_tolower_normal);
    TestAdd("StrToLower already lower", test_tolower_already_lower);
    TestAdd("StrToLower empty",        test_tolower_empty);
    TestAdd("StrToLower truncation",   test_tolower_truncation);
    TestAdd("StrToLower NULL src",     test_tolower_null_src);
    TestAdd("StrToLower NULL dst",     test_tolower_null_dst);
    TestAdd("StrToLower zero len",     test_tolower_zero_len);

    /* StrNCopy */
    TestAdd("StrNCopy normal",         test_ncopy_normal);
    TestAdd("StrNCopy truncation",     test_ncopy_truncation);
    TestAdd("StrNCopy empty",          test_ncopy_empty);
    TestAdd("StrNCopy exact fit",      test_ncopy_exact_fit);
    TestAdd("StrNCopy NULL src",       test_ncopy_null_src);
    TestAdd("StrNCopy NULL dst",       test_ncopy_null_dst);
    TestAdd("StrNCopy zero len",       test_ncopy_zero_len);

    /* StrCompareI */
    TestAdd("StrCompareI equal",       test_comparei_equal);
    TestAdd("StrCompareI same case",   test_comparei_equal_same);
    TestAdd("StrCompareI less",        test_comparei_less);
    TestAdd("StrCompareI greater",     test_comparei_greater);
    TestAdd("StrCompareI empty",       test_comparei_empty);
    TestAdd("StrCompareI empty vs non", test_comparei_empty_vs_nonempty);
    TestAdd("StrCompareI both NULL",   test_comparei_null_both);
    TestAdd("StrCompareI NULL a",      test_comparei_null_a);
    TestAdd("StrCompareI NULL b",      test_comparei_null_b);
    TestAdd("StrCompareI prefix",      test_comparei_prefix);

    return TestRun();
}

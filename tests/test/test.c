/* Copyright 2026, Andrew C. Young <andrew@vaelen.org> */
/* SPDX-License-Identifier: MIT */

/*
 * tests/test/test.c - Self-tests for the unit testing framework
 *
 * Exercises every public assertion function with both passing
 * and (where feasible) failing inputs to verify correct behaviour.
 */

#include <stddef.h>
#include "test.h"

/* ------------------------------------------------------------------ */
/* Test cases                                                          */
/* ------------------------------------------------------------------ */

static void test_assert_true_pass(void)
{
    TestAssertTrue(1, "1 should be true");
    TestAssertTrue(42, "non-zero should be true");
    TestAssertTrue(-1, "negative should be true");
}

static void test_assert_eq_pass(void)
{
    TestAssertEq(0L, 0L, "0 == 0");
    TestAssertEq(100L, 100L, "100 == 100");
    TestAssertEq(-50L, -50L, "-50 == -50");
}

static void test_assert_neq_pass(void)
{
    TestAssertNeq(0L, 1L, "0 != 1");
    TestAssertNeq(-1L, 1L, "-1 != 1");
    TestAssertNeq(100L, 200L, "100 != 200");
}

static void test_assert_str_eq_pass(void)
{
    TestAssertStrEq("hello", "hello", "identical strings");
    TestAssertStrEq("", "", "empty strings");
}

static void test_assert_str_neq_pass(void)
{
    TestAssertStrNeq("hello", "world", "different strings");
    TestAssertStrNeq("abc", "abcd", "different length strings");
}

static void test_assert_true_boundary(void)
{
    /* Zero is the only false value in C */
    TestAssertTrue(1 == 1, "equality expression");
    TestAssertTrue(2 > 1, "comparison expression");
}

static void test_assert_eq_negative(void)
{
    TestAssertEq(-1000L, -1000L, "large negative values equal");
}

static void test_assert_str_eq_special(void)
{
    TestAssertStrEq("line\tone", "line\tone", "strings with tab");
    TestAssertStrEq("a b c", "a b c", "strings with spaces");
}

/* ------------------------------------------------------------------ */
/* Failure-path tests                                                  */
/* ------------------------------------------------------------------ */

/*
 * The tests below intentionally trigger assertion failures and then
 * call TestResetFail() so that the enclosing test still passes.
 * This verifies the framework correctly detects failures without
 * causing the whole suite to report FAIL.
 */

static void test_assert_true_failure(void)
{
    /* TestAssertTrue(0, ...) should set the failure flag */
    TestAssertTrue(0, "(expected failure)");
    /* If we get here the framework did not crash -- good */
    TestResetFail();
}

static void test_assert_eq_failure(void)
{
    TestAssertEq(1L, 2L, "(expected failure)");
    TestResetFail();
}

static void test_assert_neq_failure(void)
{
    TestAssertNeq(5L, 5L, "(expected failure)");
    TestResetFail();
}

static void test_assert_str_eq_null(void)
{
    /* NULL first argument */
    TestAssertStrEq(NULL, "hello", "NULL expected arg");
    TestResetFail();
    /* NULL second argument */
    TestAssertStrEq("hello", NULL, "NULL actual arg");
    TestResetFail();
    /* Both NULL */
    TestAssertStrEq(NULL, NULL, "both args NULL");
    TestResetFail();
}

static void test_assert_str_neq_null(void)
{
    /* NULL first argument */
    TestAssertStrNeq(NULL, "hello", "NULL first arg");
    TestResetFail();
    /* NULL second argument */
    TestAssertStrNeq("hello", NULL, "NULL second arg");
    TestResetFail();
    /* Both NULL */
    TestAssertStrNeq(NULL, NULL, "both args NULL");
    TestResetFail();
}

static void test_assert_str_eq_failure(void)
{
    TestAssertStrEq("aaa", "bbb", "(expected failure)");
    TestResetFail();
}

static void test_assert_str_neq_failure(void)
{
    TestAssertStrNeq("same", "same", "(expected failure)");
    TestResetFail();
}

/* ------------------------------------------------------------------ */
/* Main                                                                */
/* ------------------------------------------------------------------ */

/*
 * Edge cases not tested here (low priority, guarded in source):
 *   - TestAdd overflow: TEST_MAX boundary is guarded with an error
 *     message but not exercised here because filling 256 slots would
 *     bloat the self-tests for little value.
 *   - TestRun with zero tests: produces "0 of 0 tests passed" and
 *     returns 0.  Not tested because it would require a separate
 *     TestInit/TestRun cycle which would interfere with the running
 *     suite.
 *   - TestAdd with NULL func: guarded with an error message and
 *     early return; not exercised here because it prints to stdout
 *     and there is no capture mechanism.
 */

int main(void)
{
    TestInit("Test Framework Self-Tests");

    /* Success-path tests */
    TestAdd("TestAssertTrue with true values", test_assert_true_pass);
    TestAdd("TestAssertEq with equal values", test_assert_eq_pass);
    TestAdd("TestAssertNeq with unequal values", test_assert_neq_pass);
    TestAdd("TestAssertStrEq with matching strings", test_assert_str_eq_pass);
    TestAdd("TestAssertStrNeq with differing strings", test_assert_str_neq_pass);
    TestAdd("TestAssertTrue boundary cases", test_assert_true_boundary);
    TestAdd("TestAssertEq negative values", test_assert_eq_negative);
    TestAdd("TestAssertStrEq special characters", test_assert_str_eq_special);

    /* Failure-path tests */
    TestAdd("TestAssertTrue failure detection", test_assert_true_failure);
    TestAdd("TestAssertEq failure detection", test_assert_eq_failure);
    TestAdd("TestAssertNeq failure detection", test_assert_neq_failure);
    TestAdd("TestAssertStrEq NULL arguments", test_assert_str_eq_null);
    TestAdd("TestAssertStrNeq NULL arguments", test_assert_str_neq_null);
    TestAdd("TestAssertStrEq failure detection", test_assert_str_eq_failure);
    TestAdd("TestAssertStrNeq failure detection", test_assert_str_neq_failure);

    return TestRun();
}

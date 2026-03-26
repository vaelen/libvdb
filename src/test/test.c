/* Copyright 2026, Andrew C. Young <andrew@vaelen.org> */
/* SPDX-License-Identifier: MIT */

/*
 * test.c - Minimal unit testing framework implementation
 *
 * All state is kept in static variables. No dynamic memory is used.
 */

#include <stdio.h>
#include <string.h>
#include "test.h"

/* ---- internal state -------------------------------------------------- */

/* Registered test names */
static const char *test_names[TEST_MAX];

/* Registered test functions */
static void (*test_funcs[TEST_MAX])(void);

/* Number of registered tests */
static int test_count = 0;

/* Suite name set by TestInit */
static const char *suite_name = "";

/*
 * Per-test failure flag.  Set to 1 by any assertion that fails
 * during the currently executing test.  Reset before each test.
 */
static int current_failed = 0;

/* ---- public API ------------------------------------------------------ */

void TestInit(const char *name)
{
    test_count = 0;
    current_failed = 0;
    suite_name = name ? name : "";
    printf("== Test Suite: %s ==\n", suite_name);
}

void TestAdd(const char *name, void (*func)(void))
{
    if (func == NULL) {
        printf("ERROR: NULL function pointer for test \"%s\"\n",
               name ? name : "(null)");
        return;
    }
    if (test_count >= TEST_MAX) {
        printf("ERROR: maximum test count (%d) exceeded\n", TEST_MAX);
        return;
    }
    test_names[test_count] = name;
    test_funcs[test_count] = func;
    test_count++;
}

int TestRun(void)
{
    int i;
    int passed = 0;

    for (i = 0; i < test_count; i++) {
        current_failed = 0;
        test_funcs[i]();
        if (current_failed) {
            printf("  FAIL: %s\n", test_names[i]);
        } else {
            printf("  PASS: %s\n", test_names[i]);
            passed++;
        }
    }

    printf("%d of %d tests passed\n", passed, test_count);
    return (passed == test_count) ? 0 : 1;
}

void TestResetFail(void)
{
    current_failed = 0;
}

void TestAssertTrue(int condition, const char *message)
{
    if (!condition) {
        printf("    ASSERT FAILED: %s\n", message);
        current_failed = 1;
    }
}

void TestAssertEq(long expected, long actual, const char *message)
{
    if (expected != actual) {
        printf("    ASSERT FAILED: %s (expected %ld, got %ld)\n",
               message, expected, actual);
        current_failed = 1;
    }
}

void TestAssertNeq(long a, long b, const char *message)
{
    if (a == b) {
        printf("    ASSERT FAILED: %s (values are both %ld)\n",
               message, a);
        current_failed = 1;
    }
}

void TestAssertStrEq(const char *expected, const char *actual,
                     const char *message)
{
    if (expected == NULL || actual == NULL) {
        printf("    ASSERT FAILED: %s (NULL pointer)\n", message);
        current_failed = 1;
        return;
    }
    if (strcmp(expected, actual) != 0) {
        printf("    ASSERT FAILED: %s (expected \"%s\", got \"%s\")\n",
               message, expected, actual);
        current_failed = 1;
    }
}

void TestAssertStrNeq(const char *a, const char *b, const char *message)
{
    if (a == NULL || b == NULL) {
        printf("    ASSERT FAILED: %s (NULL pointer)\n", message);
        current_failed = 1;
        return;
    }
    if (strcmp(a, b) == 0) {
        printf("    ASSERT FAILED: %s (strings are both \"%s\")\n",
               message, a);
        current_failed = 1;
    }
}

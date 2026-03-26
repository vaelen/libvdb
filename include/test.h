/* Copyright 2026, Andrew C. Young <andrew@vaelen.org> */
/* SPDX-License-Identifier: MIT */

/*
 * test.h - Minimal unit testing framework for vDB
 *
 * Provides test suite registration, execution, and assertion macros.
 * Uses static arrays internally -- no dynamic memory allocation.
 * Maximum of 256 tests per suite.
 *
 * Usage:
 *   TestInit("My Suite");
 *   TestAdd("test name", my_test_func);
 *   return TestRun();
 */

#ifndef TEST_H
#define TEST_H

/* Maximum number of tests that can be registered in a single suite */
#define TEST_MAX 256

/*
 * TestInit - Initialize a test suite.
 *
 * Resets all internal state and stores the suite name for reporting.
 * Must be called before TestAdd or TestRun.
 *
 * Parameters:
 *   suite_name - Name of the test suite (printed in output header).
 */
void TestInit(const char *suite_name);

/*
 * TestAdd - Register a test case.
 *
 * Adds a named test function to the suite. Tests are run in the
 * order they are added. No more than TEST_MAX tests may be added.
 *
 * Parameters:
 *   name - Short description of the test case.
 *   func - Function pointer (void -> void) that contains assertions.
 */
void TestAdd(const char *name, void (*func)(void));

/*
 * TestRun - Execute all registered tests and print results.
 *
 * Runs each test, printing PASS or FAIL after each one.
 * Prints a summary line at the end: "X of Y tests passed".
 *
 * Returns:
 *   0 if all tests passed, 1 if any test failed.
 */
int TestRun(void);

/*
 * TestResetFail - Reset the current test failure state.
 *
 * This is intended for self-testing the framework.  After
 * intentionally triggering a failure assertion, call this to
 * clear the failure flag so the enclosing test can still pass.
 */
void TestResetFail(void);

/*
 * TestAssertTrue - Assert that a condition is true.
 *
 * If the condition is false (zero), the assertion fails and the
 * message is printed, but the current test continues running.
 *
 * Parameters:
 *   condition - Expression to evaluate (nonzero = pass).
 *   message   - Description printed on failure.
 */
void TestAssertTrue(int condition, const char *message);

/*
 * TestAssertEq - Assert that two long values are equal.
 *
 * Parameters:
 *   expected - The expected value.
 *   actual   - The actual value.
 *   message  - Description printed on failure.
 */
void TestAssertEq(long expected, long actual, const char *message);

/*
 * TestAssertNeq - Assert that two long values are not equal.
 *
 * Parameters:
 *   a       - First value.
 *   b       - Second value.
 *   message - Description printed on failure.
 */
void TestAssertNeq(long a, long b, const char *message);

/*
 * TestAssertStrEq - Assert that two strings are equal.
 *
 * Compares using strcmp. Both pointers must be non-NULL.
 *
 * Parameters:
 *   expected - The expected string.
 *   actual   - The actual string.
 *   message  - Description printed on failure.
 */
void TestAssertStrEq(const char *expected, const char *actual,
                     const char *message);

/*
 * TestAssertStrNeq - Assert that two strings are not equal.
 *
 * Compares using strcmp. Both pointers must be non-NULL.
 *
 * Parameters:
 *   a       - First string.
 *   b       - Second string.
 *   message - Description printed on failure.
 */
void TestAssertStrNeq(const char *a, const char *b, const char *message);

#endif /* TEST_H */

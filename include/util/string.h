/* Copyright 2026, Andrew C. Young <andrew@vaelen.org> */
/* SPDX-License-Identifier: MIT */

/*
 * string.h - Safe string helper functions for vDB
 *
 * Provides portable, safe string operations that always
 * null-terminate output buffers. All functions handle NULL
 * inputs gracefully without crashing.
 */

#ifndef UTIL_STRING_H
#define UTIL_STRING_H

#include <stddef.h> /* size_t */

/*
 * StrToLower - Convert string to lowercase.
 *
 * Copies src to dst, converting each character to lowercase.
 * Always null-terminates dst.  max_len includes the null terminator,
 * so at most max_len-1 characters are written before the '\0'.
 *
 * If dst is NULL or max_len is 0, the function does nothing.
 * If src is NULL, dst is set to an empty string.
 *
 * Parameters:
 *   dst     - Destination buffer.
 *   src     - Source string to convert.
 *   max_len - Size of the destination buffer (includes '\0').
 */
void StrToLower(char *dst, const char *src, size_t max_len);

/*
 * StrNCopy - Safe string copy that always null-terminates.
 *
 * Copies at most max_len-1 characters from src to dst and always
 * writes a null terminator.  max_len includes the null terminator.
 *
 * If dst is NULL or max_len is 0, the function does nothing.
 * If src is NULL, dst is set to an empty string.
 *
 * Parameters:
 *   dst     - Destination buffer.
 *   src     - Source string to copy.
 *   max_len - Size of the destination buffer (includes '\0').
 */
void StrNCopy(char *dst, const char *src, size_t max_len);

/*
 * StrCompareI - Case-insensitive string comparison.
 *
 * Compares two strings ignoring case differences (ASCII only).
 *
 * Returns 0 if the strings are equal (ignoring case),
 * a negative value if a < b, a positive value if a > b.
 *
 * NULL is treated as less than any non-NULL string.
 * Two NULL pointers are considered equal.
 *
 * Parameters:
 *   a - First string.
 *   b - Second string.
 *
 * Returns:
 *   Integer less than, equal to, or greater than zero.
 */
int StrCompareI(const char *a, const char *b);

#endif /* UTIL_STRING_H */

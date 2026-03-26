/* Copyright 2026, Andrew C. Young <andrew@vaelen.org> */
/* SPDX-License-Identifier: MIT */

/*
 * string.c - Safe string helper implementations for vDB
 *
 * All functions handle NULL inputs gracefully and always
 * null-terminate output buffers when possible.
 */

#include "util.h"

/*
 * ToLowerChar - Convert a single character to lowercase.
 *
 * Only converts ASCII uppercase letters (A-Z).  All other
 * characters are returned unchanged.
 */
static char ToLowerChar(char c)
{
    if (c >= 'A' && c <= 'Z') {
        return (char)(c + ('a' - 'A'));
    }
    return c;
}

void StrToLower(char *dst, const char *src, size_t max_len)
{
    size_t i;

    if (dst == NULL || max_len == 0) {
        return;
    }

    if (src == NULL) {
        dst[0] = '\0';
        return;
    }

    for (i = 0; i < max_len - 1 && src[i] != '\0'; i++) {
        dst[i] = ToLowerChar(src[i]);
    }
    dst[i] = '\0';
}

void StrNCopy(char *dst, const char *src, size_t max_len)
{
    size_t i;

    if (dst == NULL || max_len == 0) {
        return;
    }

    if (src == NULL) {
        dst[0] = '\0';
        return;
    }

    for (i = 0; i < max_len - 1 && src[i] != '\0'; i++) {
        dst[i] = src[i];
    }
    dst[i] = '\0';
}

int StrCompareI(const char *a, const char *b)
{
    if (a == NULL && b == NULL) {
        return 0;
    }
    if (a == NULL) {
        return -1;
    }
    if (b == NULL) {
        return 1;
    }

    while (*a != '\0' && *b != '\0') {
        char la = ToLowerChar(*a);
        char lb = ToLowerChar(*b);
        if (la != lb) {
            return (int)(unsigned char)la - (int)(unsigned char)lb;
        }
        a++;
        b++;
    }
    return (int)(unsigned char)ToLowerChar(*a)
         - (int)(unsigned char)ToLowerChar(*b);
}

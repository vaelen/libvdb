/* Copyright 2026, Andrew C. Young <andrew@vaelen.org> */
/* SPDX-License-Identifier: MIT */

/*
 * types.h - Portable type definitions for vDB
 *
 * Provides fixed-width integer types and a boolean type that work
 * correctly on both 16-bit (DOS) and 32-bit+ platforms.
 *
 * We use "short" for 16-bit and "long" for 32-bit because "int"
 * varies between platforms (16-bit on DOS, 32-bit on modern systems).
 */

#ifndef UTIL_TYPES_H
#define UTIL_TYPES_H

/* Unsigned 8-bit byte */
typedef unsigned char  byte;

/* Signed 16-bit integer (-32768 to 32767) */
typedef signed short   int16;

/* Unsigned 16-bit integer (0 to 65535) */
typedef unsigned short uint16;

/*
 * 32-bit integer types.
 *
 * On 16-bit platforms (DOS), int is 16 bits and long is 32 bits.
 * On 32-bit platforms, both int and long are 32 bits.
 * On 64-bit platforms (LP64), int is 32 bits but long is 64 bits.
 *
 * We need a type that is exactly 32 bits on all platforms.
 * Using int works on 32-bit and 64-bit systems; using long works
 * on 16-bit and 32-bit systems.  We detect LP64 via sizeof checks
 * at compile time using a conditional define.
 */
#if defined(__LP64__) || defined(_LP64) || defined(__x86_64__) || \
    defined(__amd64__) || defined(__aarch64__) || defined(_M_X64)
/* 64-bit platform: int is 32 bits, long is 64 bits */
typedef signed int     int32;
typedef unsigned int   uint32;
#else
/* 16-bit or 32-bit platform: long is 32 bits */
typedef signed long    int32;
typedef unsigned long  uint32;
#endif

/* Boolean type (TRUE or FALSE) */
typedef unsigned char  bool;
#define TRUE  1
#define FALSE 0

#endif /* UTIL_TYPES_H */

/*
 * MIT License
 *
 * Copyright 2025, Andrew C. Young <andrew@vaelen.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef VDB_H
#define VDB_H

/*
 * VDB - File-based database library
 *
 * Convenience header that includes all VDB components.
 * Each component header can also be included independently.
 */

/* Fixed-width integer types for C89 */

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
  #include <stdint.h>
  typedef uint8_t  byte;
  typedef int16_t  int16;
  typedef uint16_t uint16;
  typedef int32_t  int32;
  typedef uint32_t uint32;
#elif defined(_MSC_VER)
  typedef unsigned char  byte;
  typedef short          int16;
  typedef unsigned short uint16;
  typedef long           int32;
  typedef unsigned long  uint32;
#elif defined(__TURBOC__) || defined(__BORLANDC__)
  typedef unsigned char  byte;
  typedef int            int16;
  typedef unsigned int   uint16;
  typedef long           int32;
  typedef unsigned long  uint32;
#elif defined(__WATCOMC__)
  typedef unsigned char  byte;
  typedef short          int16;
  typedef unsigned short uint16;
  typedef long           int32;
  typedef unsigned long  uint32;
#elif defined(THINK_C) || defined(__MWERKS__) || defined(__MC68K__) || defined(__POWERPC__)
  typedef unsigned char  byte;
  typedef short          int16;
  typedef unsigned short uint16;
  typedef long           int32;
  typedef unsigned long  uint32;
#else
  /* Generic - detect sizes */
  typedef unsigned char  byte;
  typedef short          int16;
  typedef unsigned short uint16;
  #if defined(__LP64__) || defined(_LP64) || (defined(__SIZEOF_LONG__) && __SIZEOF_LONG__ == 8)
    /* 64-bit Unix: long is 8 bytes, int is 4 bytes */
    typedef int            int32;
    typedef unsigned int   uint32;
  #else
    /* 32-bit or Windows LLP64: long is 4 bytes */
    typedef long           int32;
    typedef unsigned long  uint32;
  #endif
#endif

/* Epoch conversion constant: seconds between Mac (1/1/1904) and Unix (1/1/1970) */
#define EPOCH_DELTA 2082844800UL

#include "hash.h"
#include "btree.h"
#include "db.h"

#endif /* VDB_H */

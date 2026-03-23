/*
 * MIT License
 *
 * Copyright 2026, Andrew C. Young <andrew@vaelen.org>
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

#ifndef HASH_H
#define HASH_H

#include "vdbtypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* SHA-1 digest size */
#define SHA1_DIGEST_SIZE 20

/* SHA-1 context */
typedef struct {
    uint32 h0, h1, h2, h3, h4;
    uint32 msg_len_lo, msg_len_hi;
    int16  buffer_len;
    byte   buffer[64];
} SHA1Context;

/* CRC-16/KERMIT (CCITT) */
uint16 CRC16(const void *data, int16 len);
uint16 CRC16Start(uint16 *crc, const void *data, int16 len);
uint16 CRC16Add(uint16 *crc, const void *data, int16 len);
uint16 CRC16End(uint16 *crc);

/* CRC-16/XMODEM (ZMODEM) */
uint16 CRC16X(const void *data, int16 len);
uint16 CRC16XStart(uint16 *crc, const void *data, int16 len);
uint16 CRC16XAdd(uint16 *crc, const void *data, int16 len);
uint16 CRC16XEnd(uint16 *crc);

/* CRC-32/CKSUM (POSIX) */
uint32 CRC32(const void *data, int16 len);
uint32 CRC32Start(uint32 *crc, uint32 *total_len, const void *data, int16 len);
uint32 CRC32Add(uint32 *crc, uint32 *total_len, const void *data, int16 len);
uint32 CRC32End(uint32 *crc, uint32 total_len);

/* SHA-1 */
void SHA1Start(SHA1Context *ctx);
void SHA1Add(SHA1Context *ctx, const void *data, int16 len);
void SHA1End(SHA1Context *ctx, byte digest[SHA1_DIGEST_SIZE]);
void SHA1(const void *data, int16 len, byte digest[SHA1_DIGEST_SIZE]);

#ifdef __cplusplus
}
#endif

#endif /* HASH_H */

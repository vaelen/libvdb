/* Copyright 2026, Andrew C. Young <andrew@vaelen.org> */
/* SPDX-License-Identifier: MIT */

/*
 * sha1.c - SHA-1 implementation (FIPS 180-4)
 *
 * Processes data in 64-byte (512-bit) blocks.
 * All state is kept in caller-provided Sha1Context.
 */

#include <stddef.h>
#include <string.h>
#include "crc.h"

/* Left-rotate a 32-bit value */
#define ROL(v, n) (((v) << (n)) | ((v) >> (32 - (n))))

/* Store a 32-bit value in big-endian byte order */
static void StoreBE32(byte *dst, uint32 val)
{
    dst[0] = (byte)((val >> 24) & 0xFF);
    dst[1] = (byte)((val >> 16) & 0xFF);
    dst[2] = (byte)((val >>  8) & 0xFF);
    dst[3] = (byte)( val        & 0xFF);
}

/* Load a 32-bit value from big-endian byte order */
static uint32 LoadBE32(const byte *src)
{
    return ((uint32)src[0] << 24) |
           ((uint32)src[1] << 16) |
           ((uint32)src[2] <<  8) |
            (uint32)src[3];
}

/* Process a single 64-byte block */
static void ProcessBlock(Sha1Context *ctx, const byte *block)
{
    uint32 w[80];
    uint32 a, b, c, d, e;
    uint32 f, k, temp;
    int t;

    /* Prepare message schedule */
    for (t = 0; t < 16; t++) {
        w[t] = LoadBE32(block + t * 4);
    }
    for (t = 16; t < 80; t++) {
        w[t] = ROL(w[t-3] ^ w[t-8] ^ w[t-14] ^ w[t-16], 1);
    }

    /* Initialize working variables */
    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];
    e = ctx->state[4];

    /* 80 rounds */
    for (t = 0; t < 80; t++) {
        if (t < 20) {
            f = (b & c) | ((~b) & d);
            k = 0x5A827999UL;
        } else if (t < 40) {
            f = b ^ c ^ d;
            k = 0x6ED9EBA1UL;
        } else if (t < 60) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8F1BBCDCUL;
        } else {
            f = b ^ c ^ d;
            k = 0xCA62C1D6UL;
        }
        temp = ROL(a, 5) + f + e + k + w[t];
        e = d;
        d = c;
        c = ROL(b, 30);
        b = a;
        a = temp;
    }

    /* Update state */
    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
}

void Sha1Init(Sha1Context *ctx)
{
    ctx->state[0] = 0x67452301UL;
    ctx->state[1] = 0xEFCDAB89UL;
    ctx->state[2] = 0x98BADCFEUL;
    ctx->state[3] = 0x10325476UL;
    ctx->state[4] = 0xC3D2E1F0UL;
    ctx->count[0] = 0;
    ctx->count[1] = 0;
}

void Sha1Update(Sha1Context *ctx, const byte *data, size_t len)
{
    size_t buf_used;
    size_t buf_free;
    uint32 bit_count;

    if (len == 0) {
        return;
    }

    buf_used = (size_t)((ctx->count[0] >> 3) & 0x3F);
    buf_free = SHA1_BLOCK_SIZE - buf_used;

    /* Update bit count */
    bit_count = (uint32)(len << 3);
    ctx->count[0] += bit_count;
    if (ctx->count[0] < bit_count) {
        ctx->count[1]++;
    }
    ctx->count[1] += (uint32)(len >> 29);

    /* If we have buffered data and new data fills a block */
    if (buf_used > 0 && len >= buf_free) {
        memcpy(ctx->buffer + buf_used, data, buf_free);
        ProcessBlock(ctx, ctx->buffer);
        data += buf_free;
        len -= buf_free;
        buf_used = 0;
    }

    /* Process full blocks */
    while (len >= SHA1_BLOCK_SIZE) {
        ProcessBlock(ctx, data);
        data += SHA1_BLOCK_SIZE;
        len -= SHA1_BLOCK_SIZE;
    }

    /* Buffer remaining data */
    if (len > 0) {
        memcpy(ctx->buffer + buf_used, data, len);
    }
}

void Sha1Final(Sha1Context *ctx, byte *digest)
{
    byte pad[SHA1_BLOCK_SIZE];
    size_t buf_used;
    size_t pad_len;
    byte len_bytes[8];
    int i;

    /* Save bit count before padding */
    StoreBE32(len_bytes, ctx->count[1]);
    StoreBE32(len_bytes + 4, ctx->count[0]);

    /* Pad: append 0x80, then zeroes, then 8-byte length */
    buf_used = (size_t)((ctx->count[0] >> 3) & 0x3F);
    if (buf_used < 56) {
        pad_len = 56 - buf_used;
    } else {
        pad_len = 120 - buf_used;
    }

    memset(pad, 0, pad_len);
    pad[0] = 0x80;
    Sha1Update(ctx, pad, pad_len);
    Sha1Update(ctx, len_bytes, 8);

    /* Output digest in big-endian */
    for (i = 0; i < 5; i++) {
        StoreBE32(digest + i * 4, ctx->state[i]);
    }
}

void Sha1Hash(const byte *data, size_t len, byte *digest)
{
    Sha1Context ctx;

    Sha1Init(&ctx);
    Sha1Update(&ctx, data, len);
    Sha1Final(&ctx, digest);
}

void Sha1HashString(const char *str, byte *digest)
{
    if (str == NULL) {
        memset(digest, 0, SHA1_DIGEST_SIZE);
        return;
    }
    Sha1Hash((const byte *)str, strlen(str), digest);
}

void Sha1ToHex(const byte *digest, char *hex_out)
{
    static const char hex_chars[] = "0123456789abcdef";
    int i;

    for (i = 0; i < SHA1_DIGEST_SIZE; i++) {
        hex_out[i * 2]     = hex_chars[(digest[i] >> 4) & 0x0F];
        hex_out[i * 2 + 1] = hex_chars[ digest[i]       & 0x0F];
    }
    hex_out[40] = '\0';
}

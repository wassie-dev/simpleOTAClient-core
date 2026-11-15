/**
 * sota_hash_portable.c - sota_hash.h with no dependencies, for the host
 * tests and for a port to a platform without a crypto library: a minimal
 * FIPS 180-4 SHA-256 (the construction of the standard's reference,
 * checked against the NIST vectors in test/host/test_signing.c), and
 * Monocypher's SHA-512 from monocypher-ed25519.c beside this file. The
 * ESP32 port uses mbedTLS instead (port/esp32/sota_hash_mbedtls.c), so
 * neither file is copied into the clients.
 */

#include "sota_hash.h"

#include <string.h>

#include "sota_signing.h"

/* A running SHA-256, kept in a sota_hash_ctx. */
typedef struct {
    uint32_t state[8];  /* The hash state. */
    uint64_t bitlen;    /* Bits hashed so far. */
    uint8_t buffer[64]; /* A partial block. */
    size_t buffer_len;  /* Bytes in buffer. */
} sha256_t;

typedef char sha256_fits_ctx[sizeof(sha256_t) <= SOTA_HASH_CTX_SIZE ? 1 : -1];

static const uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
    0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
    0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
    0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
    0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
    0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

#define ROTR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

static void sha256_block(sha256_t *ctx, const uint8_t block[64]) {
    uint32_t w[64];
    for (int i = 0; i < 16; ++i) {
        w[i] = ((uint32_t)block[4 * i] << 24) | ((uint32_t)block[4 * i + 1] << 16) |
               ((uint32_t)block[4 * i + 2] << 8) | (uint32_t)block[4 * i + 3];
    }
    for (int i = 16; i < 64; ++i) {
        uint32_t s0 = ROTR(w[i - 15], 7) ^ ROTR(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = ROTR(w[i - 2], 17) ^ ROTR(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    uint32_t a = ctx->state[0], b = ctx->state[1], c = ctx->state[2],
             d = ctx->state[3], e = ctx->state[4], f = ctx->state[5],
             g = ctx->state[6], h = ctx->state[7];

    for (int i = 0; i < 64; ++i) {
        uint32_t s1 = ROTR(e, 6) ^ ROTR(e, 11) ^ ROTR(e, 25);
        uint32_t ch = (e & f) ^ (~e & g);
        uint32_t t1 = h + s1 + ch + K[i] + w[i];
        uint32_t s0 = ROTR(a, 2) ^ ROTR(a, 13) ^ ROTR(a, 22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t t2 = s0 + maj;
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }

    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
}

static void sha256_init(sha256_t *ctx) {
    ctx->state[0] = 0x6a09e667;
    ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372;
    ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f;
    ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab;
    ctx->state[7] = 0x5be0cd19;
    ctx->bitlen = 0;
    ctx->buffer_len = 0;
}

static void sha256_update(sha256_t *ctx, const uint8_t *data, size_t len) {
    ctx->bitlen += (uint64_t)len * 8;

    if (ctx->buffer_len) {
        size_t fill = 64 - ctx->buffer_len;
        if (fill > len) fill = len;
        memcpy(ctx->buffer + ctx->buffer_len, data, fill);
        ctx->buffer_len += fill;
        data += fill;
        len -= fill;
        if (ctx->buffer_len == 64) {
            sha256_block(ctx, ctx->buffer);
            ctx->buffer_len = 0;
        }
    }
    while (len >= 64) {
        sha256_block(ctx, data);
        data += 64;
        len -= 64;
    }
    if (len) {
        memcpy(ctx->buffer, data, len);
        ctx->buffer_len = len;
    }
}

static void sha256_final(sha256_t *ctx, uint8_t out[32]) {
    uint64_t bitlen = ctx->bitlen;

    /* Padding: 0x80, zeros, then the 64-bit big-endian bit length. */
    static const uint8_t pad_byte = 0x80;
    uint8_t zeros[64] = {0};
    size_t rem = ctx->buffer_len;
    size_t pad_zeros = (rem < 56) ? (55 - rem) : (119 - rem);

    /* Feed the padding through update(); the length appended below uses
     * the `bitlen` snapshot taken above, so the ctx->bitlen churn from
     * these calls is irrelevant. */
    sha256_update(ctx, &pad_byte, 1);
    while (pad_zeros) {
        size_t n = pad_zeros > 64 ? 64 : pad_zeros;
        sha256_update(ctx, zeros, n);
        pad_zeros -= n;
    }
    uint8_t len_be[8];
    for (int i = 0; i < 8; ++i) {
        len_be[i] = (uint8_t)(bitlen >> (56 - 8 * i));
    }
    sha256_update(ctx, len_be, 8);

    for (int i = 0; i < 8; ++i) {
        out[4 * i] = (uint8_t)(ctx->state[i] >> 24);
        out[4 * i + 1] = (uint8_t)(ctx->state[i] >> 16);
        out[4 * i + 2] = (uint8_t)(ctx->state[i] >> 8);
        out[4 * i + 3] = (uint8_t)(ctx->state[i]);
    }
}

void sota_sha256_init(sota_hash_ctx *ctx) { sha256_init((sha256_t *)ctx); }

void sota_sha256_update(sota_hash_ctx *ctx, const uint8_t *data, size_t len) {
    sha256_update((sha256_t *)ctx, data, len);
}

void sota_sha256_final(sota_hash_ctx *ctx, uint8_t out[32]) {
    sha256_final((sha256_t *)ctx, out);
}

#if SOTA_SIGNED_UPDATES

#include "monocypher-ed25519.h"

typedef char sha512_fits_ctx[sizeof(crypto_sha512_ctx) <= SOTA_HASH_CTX_SIZE ? 1 : -1];

void sota_sha512_init(sota_hash_ctx *ctx) { crypto_sha512_init((crypto_sha512_ctx *)ctx); }

void sota_sha512_update(sota_hash_ctx *ctx, const uint8_t *data, size_t len) {
    crypto_sha512_update((crypto_sha512_ctx *)ctx, data, len);
}

void sota_sha512_final(sota_hash_ctx *ctx, uint8_t out[64]) {
    crypto_sha512_final((crypto_sha512_ctx *)ctx, out);
}

#endif /* SOTA_SIGNED_UPDATES */

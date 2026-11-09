/**
 * sota_hash_mbedtls.c - sota_hash.h on the mbedTLS that ESP-IDF and
 * Arduino-ESP32 ship, which TLS has linked already and which uses the
 * chip's SHA hardware where there is some.
 *
 * Two shapes, because mbedTLS 4 (ESP-IDF 6) has no mbedtls/sha256.h: its
 * hashes are PSA Crypto's. Before that, PSA is optional and would add its
 * own code to the image, so mbedTLS 2 (Arduino-ESP32 2.x) and 3 use the
 * hash modules directly.
 *
 * A hash that fails gives zeros, which match no checksum and no signature.
 */

#include "sota_hash.h"

#include <string.h>

#include "sota_signing.h"

#if __has_include("mbedtls/build_info.h")
#include "mbedtls/build_info.h" /* mbedTLS 3 and later */
#else
#include "mbedtls/version.h"
#endif

#if MBEDTLS_VERSION_MAJOR >= 4

#include "psa/crypto.h"

_Static_assert(sizeof(psa_hash_operation_t) <= SOTA_HASH_CTX_SIZE,
               "psa_hash_operation_t fits a sota_hash_ctx");

static void hash_init(sota_hash_ctx *ctx, psa_algorithm_t alg)
{
    psa_hash_operation_t *op = (psa_hash_operation_t *)ctx;
    *op = psa_hash_operation_init();
    /* Does nothing once TLS, or an earlier call, has done it. A failure
     * here, or in the set-up, leaves the operation inactive, and final()
     * then writes zeros. */
    if (psa_crypto_init() == PSA_SUCCESS) (void)psa_hash_setup(op, alg);
}

static void hash_update(sota_hash_ctx *ctx, const uint8_t *data, size_t len)
{
    /* An operation that failed stays in its error state until final(). */
    (void)psa_hash_update((psa_hash_operation_t *)ctx, data, len);
}

static void hash_final(sota_hash_ctx *ctx, uint8_t *out, size_t size)
{
    psa_hash_operation_t *op = (psa_hash_operation_t *)ctx;
    size_t written = 0;
    if (psa_hash_finish(op, out, size, &written) != PSA_SUCCESS || written != size) {
        (void)psa_hash_abort(op);
        memset(out, 0, size);
    }
}

void sota_sha256_init(sota_hash_ctx *ctx) { hash_init(ctx, PSA_ALG_SHA_256); }

void sota_sha256_update(sota_hash_ctx *ctx, const uint8_t *data, size_t len)
{
    hash_update(ctx, data, len);
}

void sota_sha256_final(sota_hash_ctx *ctx, uint8_t out[32]) { hash_final(ctx, out, 32); }

#if SOTA_SIGNED_UPDATES

void sota_sha512_init(sota_hash_ctx *ctx) { hash_init(ctx, PSA_ALG_SHA_512); }

void sota_sha512_update(sota_hash_ctx *ctx, const uint8_t *data, size_t len)
{
    hash_update(ctx, data, len);
}

void sota_sha512_final(sota_hash_ctx *ctx, uint8_t out[64]) { hash_final(ctx, out, 64); }

#endif /* SOTA_SIGNED_UPDATES */

#else /* mbedTLS 2 and 3 */

#include "mbedtls/sha256.h"

/* mbedTLS 2 names the functions that report an error with a _ret suffix;
 * mbedTLS 3 gave their names to the plain ones. */
#if MBEDTLS_VERSION_MAJOR < 3
#define SOTA_MBEDTLS(function) function##_ret
#else
#define SOTA_MBEDTLS(function) function
#endif

_Static_assert(sizeof(mbedtls_sha256_context) <= SOTA_HASH_CTX_SIZE,
               "mbedtls_sha256_context fits a sota_hash_ctx");

/* The context is freed in final(), whatever happened before: on the ESP32
 * that is what hands the SHA hardware back. */

void sota_sha256_init(sota_hash_ctx *ctx)
{
    mbedtls_sha256_context *sha = (mbedtls_sha256_context *)ctx;
    mbedtls_sha256_init(sha);
    (void)SOTA_MBEDTLS(mbedtls_sha256_starts)(sha, 0);
}

void sota_sha256_update(sota_hash_ctx *ctx, const uint8_t *data, size_t len)
{
    (void)SOTA_MBEDTLS(mbedtls_sha256_update)((mbedtls_sha256_context *)ctx, data, len);
}

void sota_sha256_final(sota_hash_ctx *ctx, uint8_t out[32])
{
    mbedtls_sha256_context *sha = (mbedtls_sha256_context *)ctx;
    if (SOTA_MBEDTLS(mbedtls_sha256_finish)(sha, out) != 0) memset(out, 0, 32);
    mbedtls_sha256_free(sha);
}

#if SOTA_SIGNED_UPDATES

#include "mbedtls/sha512.h"

#if !defined(MBEDTLS_SHA512_C)
#error "simpleota: signed updates need SHA-512: enable CONFIG_MBEDTLS_SHA512_C, or build with SIMPLEOTA_DISABLE_SIGNED_UPDATES"
#endif

_Static_assert(sizeof(mbedtls_sha512_context) <= SOTA_HASH_CTX_SIZE,
               "mbedtls_sha512_context fits a sota_hash_ctx");

void sota_sha512_init(sota_hash_ctx *ctx)
{
    mbedtls_sha512_context *sha = (mbedtls_sha512_context *)ctx;
    mbedtls_sha512_init(sha);
    (void)SOTA_MBEDTLS(mbedtls_sha512_starts)(sha, 0);
}

void sota_sha512_update(sota_hash_ctx *ctx, const uint8_t *data, size_t len)
{
    (void)SOTA_MBEDTLS(mbedtls_sha512_update)((mbedtls_sha512_context *)ctx, data, len);
}

void sota_sha512_final(sota_hash_ctx *ctx, uint8_t out[64])
{
    mbedtls_sha512_context *sha = (mbedtls_sha512_context *)ctx;
    if (SOTA_MBEDTLS(mbedtls_sha512_finish)(sha, out) != 0) memset(out, 0, 64);
    mbedtls_sha512_free(sha);
}

#endif /* SOTA_SIGNED_UPDATES */

#endif /* MBEDTLS_VERSION_MAJOR */

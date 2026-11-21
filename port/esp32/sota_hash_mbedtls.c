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
 * A step that fails marks the hash failed, later steps do nothing, and
 * final() returns false, so the caller refuses what the hash was for.
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

#if !defined(PSA_WANT_ALG_SHA_256)
#error "simpleota: the checksum needs SHA-256: enable it in PSA Crypto (PSA_WANT_ALG_SHA_256)"
#endif
#if SOTA_SIGNED_UPDATES && !defined(PSA_WANT_ALG_SHA_512)
#error "simpleota: signed updates need SHA-512: enable it in PSA Crypto (PSA_WANT_ALG_SHA_512), or build with SIMPLEOTA_DISABLE_SIGNED_UPDATES"
#endif

typedef struct {
    psa_hash_operation_t op;
    bool failed;
} hash_t;

_Static_assert(sizeof(hash_t) <= SOTA_HASH_CTX_SIZE, "a PSA hash fits a sota_hash_ctx");

static void hash_init(sota_hash_ctx *ctx, psa_algorithm_t alg)
{
    hash_t *h = (hash_t *)ctx;
    h->op = psa_hash_operation_init();
    /* psa_crypto_init() does nothing once TLS, or an earlier call, has
     * done it. */
    h->failed = psa_crypto_init() != PSA_SUCCESS || psa_hash_setup(&h->op, alg) != PSA_SUCCESS;
}

static void hash_update(sota_hash_ctx *ctx, const uint8_t *data, size_t len)
{
    hash_t *h = (hash_t *)ctx;
    if (!h->failed && psa_hash_update(&h->op, data, len) != PSA_SUCCESS) h->failed = true;
}

static bool hash_final(sota_hash_ctx *ctx, uint8_t *out, size_t size)
{
    hash_t *h = (hash_t *)ctx;
    size_t written = 0;
    bool ok = !h->failed && psa_hash_finish(&h->op, out, size, &written) == PSA_SUCCESS &&
              written == size;
    if (!ok) {
        /* Releases an operation left active; does nothing to an inactive one. */
        (void)psa_hash_abort(&h->op);
        memset(out, 0, size);
    }
    return ok;
}

void sota_sha256_init(sota_hash_ctx *ctx) { hash_init(ctx, PSA_ALG_SHA_256); }

void sota_sha256_update(sota_hash_ctx *ctx, const uint8_t *data, size_t len)
{
    hash_update(ctx, data, len);
}

bool sota_sha256_final(sota_hash_ctx *ctx, uint8_t out[32]) { return hash_final(ctx, out, 32); }

#if SOTA_SIGNED_UPDATES

void sota_sha512_init(sota_hash_ctx *ctx) { hash_init(ctx, PSA_ALG_SHA_512); }

void sota_sha512_update(sota_hash_ctx *ctx, const uint8_t *data, size_t len)
{
    hash_update(ctx, data, len);
}

bool sota_sha512_final(sota_hash_ctx *ctx, uint8_t out[64]) { return hash_final(ctx, out, 64); }

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

/* The context is freed in final(), whatever happened before: on the ESP32
 * that is what hands the SHA hardware back. */

typedef struct {
    mbedtls_sha256_context sha;
    bool failed;
} sha256_t;

_Static_assert(sizeof(sha256_t) <= SOTA_HASH_CTX_SIZE, "an mbedTLS SHA-256 fits a sota_hash_ctx");

void sota_sha256_init(sota_hash_ctx *ctx)
{
    sha256_t *h = (sha256_t *)ctx;
    mbedtls_sha256_init(&h->sha);
    h->failed = SOTA_MBEDTLS(mbedtls_sha256_starts)(&h->sha, 0) != 0;
}

void sota_sha256_update(sota_hash_ctx *ctx, const uint8_t *data, size_t len)
{
    sha256_t *h = (sha256_t *)ctx;
    if (!h->failed && SOTA_MBEDTLS(mbedtls_sha256_update)(&h->sha, data, len) != 0) {
        h->failed = true;
    }
}

bool sota_sha256_final(sota_hash_ctx *ctx, uint8_t out[32])
{
    sha256_t *h = (sha256_t *)ctx;
    bool ok = !h->failed && SOTA_MBEDTLS(mbedtls_sha256_finish)(&h->sha, out) == 0;
    mbedtls_sha256_free(&h->sha);
    if (!ok) memset(out, 0, 32);
    return ok;
}

#if SOTA_SIGNED_UPDATES

#include "mbedtls/sha512.h"

#if !defined(MBEDTLS_SHA512_C)
#error "simpleota: signed updates need SHA-512: enable CONFIG_MBEDTLS_SHA512_C, or build with SIMPLEOTA_DISABLE_SIGNED_UPDATES"
#endif

typedef struct {
    mbedtls_sha512_context sha;
    bool failed;
} sha512_t;

_Static_assert(sizeof(sha512_t) <= SOTA_HASH_CTX_SIZE, "an mbedTLS SHA-512 fits a sota_hash_ctx");

void sota_sha512_init(sota_hash_ctx *ctx)
{
    sha512_t *h = (sha512_t *)ctx;
    mbedtls_sha512_init(&h->sha);
    h->failed = SOTA_MBEDTLS(mbedtls_sha512_starts)(&h->sha, 0) != 0;
}

void sota_sha512_update(sota_hash_ctx *ctx, const uint8_t *data, size_t len)
{
    sha512_t *h = (sha512_t *)ctx;
    if (!h->failed && SOTA_MBEDTLS(mbedtls_sha512_update)(&h->sha, data, len) != 0) {
        h->failed = true;
    }
}

bool sota_sha512_final(sota_hash_ctx *ctx, uint8_t out[64])
{
    sha512_t *h = (sha512_t *)ctx;
    bool ok = !h->failed && SOTA_MBEDTLS(mbedtls_sha512_finish)(&h->sha, out) == 0;
    mbedtls_sha512_free(&h->sha);
    if (!ok) memset(out, 0, 64);
    return ok;
}

#endif /* SOTA_SIGNED_UPDATES */

#endif /* MBEDTLS_VERSION_MAJOR */

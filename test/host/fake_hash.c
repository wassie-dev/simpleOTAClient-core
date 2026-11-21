/**
 * fake_hash.c - around the sota_hash.h functions, by the linker's --wrap
 * (see the Makefile). It counts the hashes started and not finished: a port
 * may hold hardware for a hash until its final(), so the engine must leave
 * none open, whatever path an update takes. And it plays a port whose
 * hashes fail, which must not let anything through.
 */

#include "fake_hash.h"

#include <string.h>

#include "sota_hash.h"

int fake_open_hashes;
bool fake_sha256_fails;
bool fake_sha512_fails;

void __real_sota_sha256_init(sota_hash_ctx *ctx);
bool __real_sota_sha256_final(sota_hash_ctx *ctx, uint8_t out[32]);
void __real_sota_sha512_init(sota_hash_ctx *ctx);
bool __real_sota_sha512_final(sota_hash_ctx *ctx, uint8_t out[64]);
void __wrap_sota_sha256_init(sota_hash_ctx *ctx);
bool __wrap_sota_sha256_final(sota_hash_ctx *ctx, uint8_t out[32]);
void __wrap_sota_sha512_init(sota_hash_ctx *ctx);
bool __wrap_sota_sha512_final(sota_hash_ctx *ctx, uint8_t out[64]);

void __wrap_sota_sha256_init(sota_hash_ctx *ctx)
{
    ++fake_open_hashes;
    __real_sota_sha256_init(ctx);
}

/* A failed hash says so, and leaves a digest anyone can predict: zeros. */
bool __wrap_sota_sha256_final(sota_hash_ctx *ctx, uint8_t out[32])
{
    --fake_open_hashes;
    bool ok = __real_sota_sha256_final(ctx, out);
    if (!fake_sha256_fails) return ok;
    memset(out, 0, 32);
    return false;
}

void __wrap_sota_sha512_init(sota_hash_ctx *ctx)
{
    ++fake_open_hashes;
    __real_sota_sha512_init(ctx);
}

bool __wrap_sota_sha512_final(sota_hash_ctx *ctx, uint8_t out[64])
{
    --fake_open_hashes;
    bool ok = __real_sota_sha512_final(ctx, out);
    if (!fake_sha512_fails) return ok;
    memset(out, 0, 64);
    return false;
}

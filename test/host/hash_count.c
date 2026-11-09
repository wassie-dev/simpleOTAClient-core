/**
 * hash_count.c - counts the hashes the engine has started and not finished,
 * for test_engine. The linker's --wrap puts these around the sota_hash.h
 * functions (see the Makefile). A port may hold hardware for a hash until
 * its final(), so the engine must leave none open, whatever path an update
 * takes.
 */

#include "fake_hal.h"
#include "sota_hash.h"

int fake_open_hashes;

void __real_sota_sha256_init(sota_hash_ctx *ctx);
void __real_sota_sha256_final(sota_hash_ctx *ctx, uint8_t out[32]);
void __real_sota_sha512_init(sota_hash_ctx *ctx);
void __real_sota_sha512_final(sota_hash_ctx *ctx, uint8_t out[64]);
void __wrap_sota_sha256_init(sota_hash_ctx *ctx);
void __wrap_sota_sha256_final(sota_hash_ctx *ctx, uint8_t out[32]);
void __wrap_sota_sha512_init(sota_hash_ctx *ctx);
void __wrap_sota_sha512_final(sota_hash_ctx *ctx, uint8_t out[64]);

void __wrap_sota_sha256_init(sota_hash_ctx *ctx)
{
    ++fake_open_hashes;
    __real_sota_sha256_init(ctx);
}

void __wrap_sota_sha256_final(sota_hash_ctx *ctx, uint8_t out[32])
{
    --fake_open_hashes;
    __real_sota_sha256_final(ctx, out);
}

void __wrap_sota_sha512_init(sota_hash_ctx *ctx)
{
    ++fake_open_hashes;
    __real_sota_sha512_init(ctx);
}

void __wrap_sota_sha512_final(sota_hash_ctx *ctx, uint8_t out[64])
{
    --fake_open_hashes;
    __real_sota_sha512_final(ctx, out);
}

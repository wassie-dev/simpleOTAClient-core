/**
 * @file
 * @brief SHA-256 and SHA-512, implemented by the platform port: on ESP32 by
 * mbedTLS, which every build already links for TLS and which uses the
 * chip's SHA hardware where there is some. port/portable/ has an
 * implementation with no dependencies, which the host tests use.
 *
 * The core hashes the download with SHA-256 for its checksum, and Ed25519
 * verification hashes it with SHA-512.
 *
 * Every init must be followed by exactly one final on the same context:
 * final also releases what the port holds for it (on some chips, the SHA
 * hardware). A hash that is no longer wanted is finished into a scratch
 * buffer. A port whose hash fails writes zeros, which match no checksum and
 * no signature.
 */

#ifndef SOTA_HASH_H
#define SOTA_HASH_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Bytes a port has for its own hash state; each port checks at compile
 *  time that its state fits. */
#define SOTA_HASH_CTX_SIZE 320u

/** A running hash. Opaque: the port's state lives in it. */
typedef union {
    uint64_t align;                     /**< Aligns the storage for any state. */
    uint8_t opaque[SOTA_HASH_CTX_SIZE]; /**< The port's state. */
} sota_hash_ctx;

/** Start a SHA-256. */
void sota_sha256_init(sota_hash_ctx *ctx);
/** Add len bytes from data. */
void sota_sha256_update(sota_hash_ctx *ctx, const uint8_t *data, size_t len);
/** Finish the hash, write the 32-byte digest to out, and release ctx. */
void sota_sha256_final(sota_hash_ctx *ctx, uint8_t out[32]);

/** Start a SHA-512. Not needed in a build without signed updates
 *  (SOTA_SIGNED_UPDATES in sota_signing.h). */
void sota_sha512_init(sota_hash_ctx *ctx);
/** Add len bytes from data. */
void sota_sha512_update(sota_hash_ctx *ctx, const uint8_t *data, size_t len);
/** Finish the hash, write the 64-byte digest to out, and release ctx. */
void sota_sha512_final(sota_hash_ctx *ctx, uint8_t out[64]);

#ifdef __cplusplus
}
#endif

#endif /* SOTA_HASH_H */

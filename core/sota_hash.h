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
 * buffer.
 *
 * A failure at any step (init, any update, or final itself) makes final
 * return false. The caller must then refuse whatever the digest was for:
 * a digest that is wrong in a known way, all zeros for one, would match a
 * checksum the server chose and let a forged Ed25519 signature verify.
 *
 * The core keeps more than one hash open at a time and feeds them in turn.
 * While it downloads a signed update it has one SHA-256 open, for the
 * checksum, and one SHA-512 per candidate key, up to
 * SOTA_VERIFIER_MAX_KEYS (2, in sota_signing.h); no more than that is ever
 * open. So a port must allow one SHA-256 and two SHA-512 open at once, and
 * an init must not block waiting for another open hash to finish, since
 * that one finishes only after this one is fed. A port with a single hash
 * engine runs the hashes that do not get it in software: waiting would
 * hang the update, and failing the hash would refuse every signed one.
 */

#ifndef SOTA_HASH_H
#define SOTA_HASH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Bytes a port has for its own hash state, a failure flag included. The
 *  largest library state measured is 224 bytes, 232 with the flag:
 *  mbedtls_sha512_context on the ESP32-S2 and S3 (ESP-IDF 5.5, mbedTLS
 *  3.6; 216 on the ESP32, C3 and C6). The portable SHA-512 is 216 bytes on
 *  a 64-bit host. The rest is margin for targets and versions not
 *  measured, mbedTLS 4's psa_hash_operation_t among them. Each port checks
 *  at compile time that its state fits, so a port whose state is larger
 *  fails to build: raise this size until it fits. */
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
/** Finish the hash, write the 32-byte digest to out, and release ctx.
 *  @return false when any step of the hash failed: out is not the digest. */
bool sota_sha256_final(sota_hash_ctx *ctx, uint8_t out[32]);

/** Start a SHA-512. Not needed in a build without signed updates
 *  (SOTA_SIGNED_UPDATES in sota_signing.h). */
void sota_sha512_init(sota_hash_ctx *ctx);
/** Add len bytes from data. */
void sota_sha512_update(sota_hash_ctx *ctx, const uint8_t *data, size_t len);
/** Finish the hash, write the 64-byte digest to out, and release ctx.
 *  @return false when any step of the hash failed: out is not the digest. */
bool sota_sha512_final(sota_hash_ctx *ctx, uint8_t out[64]);

#ifdef __cplusplus
}
#endif

#endif /* SOTA_HASH_H */

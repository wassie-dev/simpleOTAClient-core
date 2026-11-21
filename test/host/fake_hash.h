/**
 * @file
 * @brief The linker's --wrap around the sota_hash.h functions (fake_hash.c,
 * see the Makefile): counts the hashes started and not finished, and can
 * play a port whose hashes fail.
 */

#ifndef FAKE_HASH_H
#define FAKE_HASH_H

#include <stdbool.h>

extern int fake_open_hashes; /**< Hashes started and not yet finished. */
extern bool fake_sha256_fails; /**< Every SHA-256 finished from now on fails. */
extern bool fake_sha512_fails; /**< Every SHA-512 finished from now on fails. */

#endif /* FAKE_HASH_H */

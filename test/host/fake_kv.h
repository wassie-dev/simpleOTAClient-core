/**
 * fake_kv.h - an in-memory sota_kv.h for the host tests.
 *
 * fake_kv_cut_after(n) lets n more writes (sets and erases) through and
 * fails every one after it without applying it: the store then holds
 * exactly what a power cut at that point would have left.
 */

#ifndef FAKE_KV_H
#define FAKE_KV_H

#include <stddef.h>

/** Empty the store and allow every write. */
void fake_kv_reset(void);

/** Let n more writes through, then fail the rest. */
void fake_kv_cut_after(int n);

/** Fail only the write n writes from now (0 = the next one); later writes
 *  go through. A write error that is not a power cut. */
void fake_kv_fail_nth(int n);

/** Allow every write again, keeping the contents. */
void fake_kv_power_on(void);

/** Writes applied since the last reset. */
int fake_kv_writes(void);

/** Keys in the store. */
size_t fake_kv_count(void);

#endif /* FAKE_KV_H */

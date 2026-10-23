/**
 * fake_kv.h - an in-memory sota_kv.h for the host tests.
 *
 * fake_kv_cut_after(n) lets n more writes (sets and erases) through and
 * fails every one after it without applying it: the store then holds
 * exactly what a power cut at that point would have left.
 */

#ifndef FAKE_KV_H
#define FAKE_KV_H

#include <stdbool.h>
#include <stddef.h>

/** Empty the store and allow every write. */
void fake_kv_reset(void);

/** Let n more writes through, then fail the rest. */
void fake_kv_cut_after(int n);

/** Fail only the write n writes from now (0 = the next one); later writes
 *  go through. A write error that is not a power cut. */
void fake_kv_fail_nth(int n);

/** Called, instead of failing, by the first write the cut stops; it does
 *  not return (the host tests longjmp out, as a power cut stops the code).
 *  NULL to fail the write instead. */
void fake_kv_set_cut_hook(void (*hook)(void));

/** A step that is not a write, such as a boot slot switch: counted against
 *  the cut like a write. False when the cut stops it. */
bool fake_kv_step(void);

/** Allow every write again, keeping the contents. */
void fake_kv_power_on(void);

/** Writes applied since the last reset. */
int fake_kv_writes(void);

/** Keys in the store. */
size_t fake_kv_count(void);

#endif /* FAKE_KV_H */

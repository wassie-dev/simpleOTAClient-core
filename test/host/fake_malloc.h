/**
 * fake_malloc.h - forced into the engine test's sources (-include), so the
 * test can see what the engine allocates. fake_hal.c counts the calls.
 */

#ifndef FAKE_MALLOC_H
#define FAKE_MALLOC_H

#include <stddef.h>
#include <stdlib.h>

/** malloc(n), counted in fake_malloc_watched when n is fake_malloc_watch. */
void *fake_malloc(size_t n);
extern size_t fake_malloc_watch;
extern int fake_malloc_watched;

#define malloc(n) fake_malloc(n)

#endif /* FAKE_MALLOC_H */

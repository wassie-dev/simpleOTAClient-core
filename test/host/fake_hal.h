/**
 * fake_hal.h - the rest of the HAL for the engine tests: a two-slot device
 * with an ESP-IDF-like bootloader, a scripted server, and the application
 * hooks. The key-value store is fake_kv.h.
 *
 * sota_sys_restart(), sota_boot_rollback() and a power cut (fake_kv's cut
 * hook) longjmp to fake_reset_target, as a real reset leaves the code.
 */

#ifndef FAKE_HAL_H
#define FAKE_HAL_H

#include <setjmp.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "sota_app.h"

#define FAKE_SLOT_A 0x10000u
#define FAKE_SLOT_B 0x110000u

enum { FAKE_JUMP_RESTART = 1, FAKE_JUMP_POWER_CUT = 2 };

enum { IMG_UNDEFINED, IMG_NEW, IMG_PENDING, IMG_VALID, IMG_INVALID, IMG_ABORTED };

typedef struct {
    bool bl_rollback;      /**< The bootloader supports app rollback. */
    uint32_t build[2];     /**< The build each slot holds, 0 = none. */
    int state[2];          /**< IMG_* per slot. */
    int boot;              /**< Slot the next boot selects. */
    int running;
    int reset_reason;
    bool fail_finish, fail_boot_set, fail_mark_valid, fail_timer;
    bool timer_armed;
    uint32_t timer_s;
    bool wdt_armed;
    uint32_t sleep_ms;     /**< Total slept. */
    int writing;           /**< Slot being written, -1 when none. */
    uint8_t written[8192];
    size_t written_len;
} fake_world_t;

typedef struct {
    bool up;
    uint32_t offer_build;  /**< 0: nothing offered. */
    char deployment[32];
    uint8_t image[5000];
    size_t image_len;
    char checksum[65];
    bool chunked;          /**< No Content-Length on the download. */
    int dl_status;
    int dl_fail_at;        /**< Byte offset where the download breaks, -1 = never. */
    int dl_fail_with;      /**< What a read returns from there on. */
    bool corrupt;          /**< Flip a byte of the download. */
    const char *extra;     /**< More JSON members for the offer, or NULL. */
    size_t dl_pos;
    const char *redirect[5]; /**< Location of each redirect before the image, NULL ends. */
    int redirects_done;
    char opened[128];        /**< The URL the last download opened. */
    char log[64][48];      /**< "event reason" of each status received. */
    int log_count;
    int checks;
} fake_server_t;

extern fake_world_t fw;
extern fake_server_t fs;
extern jmp_buf fake_reset_target;

extern sota_event_id_t fake_events[64];
extern int fake_event_count;
extern int fake_wakes;

/** Slot A runs build 1, confirmed; slot B is empty. Clears the server. */
void fake_world_reset(bool bl_rollback);

/** Offer build to every device below it, as deployment dep. */
void fake_server_offer(uint32_t build, const char *dep);

/** A reset with reason: pick the slot the bootloader starts. A deep-sleep
 *  wake resumes the image that slept. */
void fake_bootloader(int reset_reason);

/** Statuses received with this event (and reason, unless NULL). */
int fake_server_count(const char *event, const char *reason);

/** Events emitted with this id. */
int fake_event_seen(sota_event_id_t id);

#endif /* FAKE_HAL_H */

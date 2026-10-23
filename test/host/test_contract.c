/**
 * Host checks for the NVS contract (docs/NVS_CONTRACT.md): the key names,
 * types, values and blob layout that every release of both clients reads
 * and writes. A failure here means a change would break devices that
 * update, or roll back, across releases.
 *
 *   make -C test/host test
 */

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "fake_kv.h"
#include "sota_kv.h"
#include "sota_report.h"
#include "sota_state.h"
#include "sota_trial.h"

static int g_failures = 0;

static void check(int cond, const char *msg)
{
    printf("%s  %s\n", cond ? "  ok" : "FAIL", msg);
    if (!cond) g_failures++;
}

#define A 0x10000u  /* the image the update replaced */
#define B 0x150000u /* the update's slot */

static const char HASH_OLD[] = "1111111111111111111111111111111111111111111111111111111111111111";
static const char HASH_NEW[] = "2222222222222222222222222222222222222222222222222222222222222222";

static bool u8_is(const char *key, uint8_t want)
{
    uint8_t v;
    return sota_kv_get_u8(key, &v) && v == want;
}

static bool u32_is(const char *key, uint32_t want)
{
    uint32_t v;
    return sota_kv_get_u32(key, &v) && v == want;
}

static bool str_is(const char *key, const char *want)
{
    char v[80];
    return sota_kv_get_str(key, v, sizeof(v)) && strcmp(v, want) == 0;
}

static sota_boot_outcome_t classify(const sota_state_t *st, uint32_t running)
{
    sota_boot_facts_t f = {st->trial, st->staged, st->new_part, st->prev_part,
                           running, running, false};
    return sota_classify_boot(&f);
}

static void test_values(void)
{
    check(SOTA_TRIAL_STAGED == 1 && SOTA_TRIAL_ROLLED_BACK == 2 && SOTA_TRIAL_BOOTED == 3,
          "sota_trial values: 1 trial, 2 rolled back, 3 booted");
    check(SOTA_STAGED_STARTED == 1 && SOTA_STAGED_SNAPSHOT == 2,
          "sota_staged values: 1 started, 2 snapshot");
    check(SOTA_OUTBOX_VERSION == 1 && SOTA_OUTBOX_SLOTS == 2, "outbox version 1, two slots");
    check(sizeof(sota_report_t) == 104 && offsetof(sota_report_t, reason) == 12 &&
              offsetof(sota_report_t, deployment) == 36 && offsetof(sota_report_t, build) == 100,
          "report layout: event[12] reason[24] deployment[64] build u32");
    check(sizeof(sota_outbox_t) == 212 && offsetof(sota_outbox_t, count) == 1 &&
              offsetof(sota_outbox_t, reports) == 4,
          "outbox blob: version u8, count u8, pad, two reports: 212 bytes");
}

/* A trial record exactly as Arduino 0.6.0 writes it: no sota_new_part. */
static void write_arduino_060_trial(void)
{
    sota_kv_set_u8("sota_staged", SOTA_STAGED_STARTED);
    sota_kv_set_u32("sota_prev_part", A);
    sota_kv_set_u32("sota_prev_build", 41);
    sota_kv_set_str("sota_prev_hash", HASH_OLD);
    sota_kv_set_str("sota_prev_ver", "1.0");
    sota_kv_set_u8("sota_staged", SOTA_STAGED_SNAPSHOT);
    sota_kv_set_u32("sota_build", 42);
    sota_kv_set_str("sota_hash", HASH_NEW);
    sota_kv_set_str("sota_ver", "1.1");
    sota_kv_set_str("sota_fail_dep", "dep-42");
    sota_kv_set_u32("sota_fail_build", 42);
    sota_kv_set_u8("sota_conf_pend", 1);
    sota_kv_set_u8("sota_trial", 1);
    sota_kv_erase("sota_staged");
}

static void test_reads_old_records(void)
{
    sota_state_t st;

    fake_kv_reset();
    write_arduino_060_trial();
    sota_state_load(&st);
    check(st.live.build == 42 && strcmp(st.live.hash, HASH_NEW) == 0 &&
              strcmp(st.live.version, "1.1") == 0 && st.prev.build == 41 &&
              strcmp(st.prev.hash, HASH_OLD) == 0 && st.prev_part == A && st.new_part == 0 &&
              strcmp(st.deployment, "dep-42") == 0 && st.report_build == 42 && st.conf_pend == 1,
          "Arduino 0.6.0 trial record: every field loads");
    check(classify(&st, B) == SOTA_BOOT_TRIAL, "  and on its new image it is a trial");
    check(classify(&st, A) == SOTA_BOOT_ROLLED_BACK, "  and back on the old image a rollback");

    fake_kv_reset();
    write_arduino_060_trial();
    sota_kv_set_u32("sota_new_part", B); /* ESP-IDF 0.2.x writes both */
    sota_state_load(&st);
    check(classify(&st, B) == SOTA_BOOT_TRIAL && classify(&st, A) == SOTA_BOOT_ROLLED_BACK,
          "ESP-IDF 0.2.x trial record: classifies the same");

    fake_kv_reset();
    sota_kv_set_u32("sota_build", 7);
    sota_state_load(&st);
    check(st.live.build == 7 && st.image_id[0] == '\0' && classify(&st, A) == SOTA_BOOT_NONE,
          "a store from before sota_image: no record, no image noted");
}

/* What an older release reads back after this one wrote it, with the
 * types it reads them as. */
static void test_old_releases_read_new_records(void)
{
    fake_kv_reset();
    sota_identity_t prev = {41, "", "1.0"}, next = {42, "", "1.1"};
    strcpy(prev.hash, HASH_OLD);
    strcpy(next.hash, HASH_NEW);
    sota_state_stage_trial(&prev, &next, B, A, "dep-42");
    sota_state_unstage();
    check(u8_is("sota_trial", 1) && u32_is("sota_prev_part", A) && u32_is("sota_new_part", B) &&
              u32_is("sota_build", 42) && str_is("sota_hash", HASH_NEW) &&
              str_is("sota_ver", "1.1") && u32_is("sota_prev_build", 41) &&
              str_is("sota_prev_hash", HASH_OLD) && str_is("sota_prev_ver", "1.0") &&
              str_is("sota_fail_dep", "dep-42") && u32_is("sota_fail_build", 42) &&
              u8_is("sota_conf_pend", 1),
          "a staged trial: every key with the type older releases read");

    sota_state_mark_booted();
    check(u8_is("sota_trial", SOTA_TRIAL_BOOTED),
          "a booted trial is 3, which older releases read as a trial");

    sota_state_t st;
    sota_state_load(&st);
    sota_state_mark_rolled_back(&st, "panic");
    check(u8_is("sota_trial", 2) && str_is("sota_rb_reason", "panic") &&
              u32_is("sota_build", 41) && str_is("sota_hash", HASH_OLD),
          "a rollback: sota_trial 2, the reason, and the previous identity live");

    sota_outbox_t box;
    sota_outbox_init(&box);
    sota_outbox_push(&box, "confirmed", "", "dep-42", 42);
    sota_state_save_outbox(&box);
    sota_outbox_t raw;
    check(sota_kv_get_blob("sota_outbox", &raw, sizeof(raw)) && raw.version == 1 &&
              raw.count == 1 && strcmp(raw.reports[0].event, "confirmed") == 0,
          "the outbox is one 212-byte blob");
}

int main(void)
{
    test_values();
    test_reads_old_records();
    test_old_releases_read_new_records();
    if (g_failures) {
        printf("\n%d contract check(s) failed\n", g_failures);
        return 1;
    }
    printf("NVS contract host checks passed\n");
    return 0;
}

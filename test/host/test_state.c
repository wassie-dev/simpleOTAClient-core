/**
 * Host checks for the persisted state: what each function writes, and that
 * a power cut or a failed write at any point leaves a state the next boot
 * resolves to a known identity.
 *
 *   make -C test/host test
 */

#include <stdio.h>
#include <string.h>

#include "fake_kv.h"
#include "sota_kv.h"
#include "sota_state.h"
#include "sota_trial.h"

static int g_failures = 0;

static void check(int cond, const char *msg)
{
    printf("%s  %s\n", cond ? "  ok" : "FAIL", msg);
    if (!cond) g_failures++;
}

#define A 0x10000u  /* running image */
#define B 0x150000u /* update slot */

static const char HASH_OLD[] = "1111111111111111111111111111111111111111111111111111111111111111";
static const char HASH_NEW[] = "2222222222222222222222222222222222222222222222222222222222222222";

static sota_identity_t identity(uint32_t build, const char *hash, const char *version)
{
    sota_identity_t id;
    memset(&id, 0, sizeof(id));
    id.build = build;
    strcpy(id.hash, hash);
    strcpy(id.version, version);
    return id;
}

static bool is(const sota_identity_t *id, uint32_t build, const char *hash, const char *version)
{
    return id->build == build && strcmp(id->hash, hash) == 0 && strcmp(id->version, version) == 0;
}

static bool has_trial_keys(void)
{
    uint8_t u8;
    uint32_t u32;
    char s[80];
    return sota_kv_get_u8("sota_trial", &u8) || sota_kv_get_u8("sota_staged", &u8) ||
           sota_kv_get_u32("sota_new_part", &u32) || sota_kv_get_u32("sota_prev_part", &u32) ||
           sota_kv_get_str("sota_fail_dep", s, sizeof(s)) ||
           sota_kv_get_u8("sota_conf_pend", &u8) || sota_kv_get_str("sota_rb_reason", s, sizeof(s));
}

/* A device on build 41 with no record. */
static void device_on_41(void)
{
    fake_kv_reset();
    sota_identity_t old = identity(41, HASH_OLD, "1.0");
    sota_state_set_identity(&old);
}

/* What boot validation does with the record, as far as identity goes. */
static sota_boot_outcome_t boot(uint32_t running, uint32_t boot_part, int reset_reason)
{
    sota_state_t st;
    sota_state_load(&st);
    sota_boot_facts_t f;
    memset(&f, 0, sizeof(f));
    f.trial = st.trial;
    f.staged = st.staged;
    f.new_part = st.new_part;
    f.prev_part = st.prev_part;
    f.running_part = running;
    f.boot_part = boot_part;
    f.deep_sleep_wake = reset_reason == SOTA_RST_DEEPSLEEP;
    sota_boot_outcome_t o = sota_classify_boot(&f);
    switch (o) {
    case SOTA_BOOT_ROLLED_BACK:
        sota_state_set_identity(&st.prev);
        sota_state_drop_trial();
        break;
    case SOTA_BOOT_ABANDONED:
        if (sota_abandon_restores_identity(st.staged)) sota_state_set_identity(&st.prev);
        sota_state_drop_trial();
        break;
    case SOTA_BOOT_REPORT_PENDING:
    case SOTA_BOOT_INCOMPLETE:
        sota_state_drop_trial();
        break;
    case SOTA_BOOT_TRIAL:
        if (st.staged) sota_state_unstage();
        break;
    default:
        break;
    }
    return o;
}

static sota_identity_t live(void)
{
    sota_state_t st;
    sota_state_load(&st);
    return st.live;
}

static bool stage_update(void)
{
    sota_identity_t old = identity(41, HASH_OLD, "1.0");
    sota_identity_t next = identity(42, HASH_NEW, "1.1");
    return sota_state_stage_trial(&old, &next, B, A, "dep-42");
}

static void test_load_and_stage(void)
{
    fake_kv_reset();
    sota_state_t st;
    sota_state_load(&st);
    check(st.live.build == 0 && st.live.hash[0] == '\0' && st.trial == 0 && st.new_part == 0,
          "a fresh device loads as zeros");

    device_on_41();
    check(stage_update(), "stage writes the record");
    sota_state_load(&st);
    check(is(&st.live, 42, HASH_NEW, "1.1") && is(&st.prev, 41, HASH_OLD, "1.0"),
          "staged: the new identity is live, the old one saved");
    check(st.trial == 1 && st.staged == SOTA_STAGED_SNAPSHOT && st.new_part == B &&
              st.prev_part == A && strcmp(st.deployment, "dep-42") == 0 &&
              st.report_build == 42 && st.conf_pend == 1 && st.rb_reason[0] == '\0',
          "staged: both partitions and the report context");
    check(sota_state_unstage(), "unstage");
    sota_state_load(&st);
    check(st.staged == 0 && st.trial == 1, "unstage removes only sota_staged");

    check(sota_state_drop_trial() && !has_trial_keys(), "drop removes every trial key");
    check(is(&st.live, 42, HASH_NEW, "1.1"), "drop keeps the live identity");

    device_on_41();
    sota_kv_set_str("sota_rb_reason", "panic");
    stage_update();
    sota_state_load(&st);
    check(st.rb_reason[0] == '\0', "stage clears a reason left by an older record");
}

static void test_versions(void)
{
    device_on_41();
    char long_version[SOTA_VERSION_MAX + 2];
    memset(long_version, 'v', sizeof(long_version) - 1);
    long_version[sizeof(long_version) - 1] = '\0';
    sota_identity_t next = identity(42, HASH_NEW, "");
    memcpy(next.version, long_version, SOTA_VERSION_MAX);
    next.version[SOTA_VERSION_MAX] = '\0';
    check(sota_state_set_identity(&next), "a 64-character version is stored");
    sota_identity_t now = live();
    check(strlen(now.version) == SOTA_VERSION_MAX, "and loads back whole");

    sota_kv_set_str("sota_ver", long_version);
    now = live();
    check(now.version[0] == '\0', "a stored version too long for the field loads as empty");
}

static void test_accept(void)
{
    device_on_41();
    sota_identity_t next = identity(42, HASH_NEW, "1.1");
    check(sota_state_accept_update(&next, "dep-42"), "accept an update without a trial");
    sota_state_t st;
    sota_state_load(&st);
    check(is(&st.live, 42, HASH_NEW, "1.1") && st.trial == 0 && st.conf_pend == 1 &&
              strcmp(st.deployment, "dep-42") == 0 && st.report_build == 42,
          "accepted: new identity live, confirmed report context, no trial");
    check(boot(B, B, SOTA_RST_SW) == SOTA_BOOT_NONE, "the next boot sees no trial");
}

static void test_power_cut_while_staging(void)
{
    device_on_41();
    stage_update();
    const int writes = fake_kv_writes() - 3; /* device_on_41() made 3 */

    bool before_switch_ok = true, after_switch_ok = true, fails_ok = true;
    for (int cut = 0; cut <= writes; ++cut) {
        /* Cut before the boot slot switched: the old image boots again. */
        device_on_41();
        fake_kv_cut_after(cut);
        bool staged = stage_update();
        fake_kv_power_on();
        sota_boot_outcome_t o = boot(A, A, SOTA_RST_POWERON);
        sota_identity_t now = live();
        before_switch_ok = before_switch_ok && staged == (cut == writes) &&
                           (o == SOTA_BOOT_NONE || o == SOTA_BOOT_ABANDONED) &&
                           (cut != 0 || o == SOTA_BOOT_NONE) &&
                           is(&now, 41, HASH_OLD, "1.0") && !has_trial_keys();

        /* The same cut, but a write that fails without a power cut: stage
         * cleans up after itself. */
        device_on_41();
        fake_kv_fail_nth(cut);
        staged = stage_update();
        now = live();
        fails_ok = fails_ok && staged == (cut == writes) &&
                   (cut == writes || (is(&now, 41, HASH_OLD, "1.0") && !has_trial_keys()));
    }

    /* Staged in full and the slot switched, then cut before unstaging: the
     * new image boots as a trial. */
    device_on_41();
    stage_update();
    sota_boot_outcome_t o = boot(B, B, SOTA_RST_POWERON);
    sota_identity_t now = live();
    sota_state_t st;
    sota_state_load(&st);
    after_switch_ok = o == SOTA_BOOT_TRIAL && is(&now, 42, HASH_NEW, "1.1") && st.staged == 0 &&
                      st.trial == 1;

    check(writes > 10, "staging takes more than ten writes");
    check(before_switch_ok,
          "a power cut at any write before the slot switch boots the old identity, no record");
    check(fails_ok, "a failed write while staging leaves the old identity, no record");
    check(after_switch_ok, "a power cut after the slot switch boots the new image as a trial");
}

static void test_power_cut_while_rolling_back(void)
{
    device_on_41();
    stage_update();
    sota_state_unstage();
    const int base = fake_kv_writes();
    sota_state_t st;
    sota_state_load(&st);
    sota_state_mark_rolled_back(&st, "confirm_timeout");
    const int writes = fake_kv_writes() - base;

    bool ok = true;
    for (int cut = 0; cut <= writes; ++cut) {
        device_on_41();
        stage_update();
        sota_state_unstage();
        sota_state_load(&st);
        fake_kv_cut_after(cut);
        bool marked = sota_state_mark_rolled_back(&st, "confirm_timeout");
        fake_kv_power_on();
        /* The boot slot was switched back first, so the old image runs. */
        sota_state_t after;
        sota_state_load(&after);
        sota_boot_outcome_t o = boot(A, A, SOTA_RST_SW);
        sota_identity_t now = live();
        ok = ok && marked == (cut == writes) &&
             (cut == writes ? o == SOTA_BOOT_REPORT_PENDING &&
                                  strcmp(after.rb_reason, "confirm_timeout") == 0
                            : o == SOTA_BOOT_ROLLED_BACK) &&
             is(&now, 41, HASH_OLD, "1.0") && !has_trial_keys();
    }
    check(ok, "a power cut at any write of a rollback boots the old identity");
}

static void test_outbox(void)
{
    fake_kv_reset();
    sota_outbox_t box;
    sota_state_load_outbox(&box);
    check(box.count == 0 && sota_outbox_valid(&box), "no stored outbox loads empty");

    sota_outbox_push(&box, "confirmed", NULL, "dep-42", 42);
    check(sota_state_save_outbox(&box), "save an outbox");
    sota_outbox_t loaded;
    sota_state_load_outbox(&loaded);
    check(loaded.count == 1 && strcmp(loaded.reports[0].deployment, "dep-42") == 0,
          "and load it back");

    sota_outbox_pop(&box);
    check(sota_state_save_outbox(&box) && fake_kv_count() == 0,
          "an empty outbox removes the key");

    unsigned char junk[sizeof(sota_outbox_t)];
    memset(junk, 0xA5, sizeof(junk));
    sota_kv_set_blob("sota_outbox", junk, sizeof(junk));
    sota_state_load_outbox(&loaded);
    check(loaded.count == 0 && sota_outbox_valid(&loaded), "a corrupt outbox loads empty");
    sota_kv_set_blob("sota_outbox", junk, 10);
    sota_state_load_outbox(&loaded);
    check(loaded.count == 0, "an outbox of another size loads empty");
}

int main(void)
{
    test_load_and_stage();
    test_versions();
    test_accept();
    test_power_cut_while_staging();
    test_power_cut_while_rolling_back();
    test_outbox();

    if (g_failures) {
        printf("%d FAILURE(S)\n", g_failures);
        return 1;
    }
    printf("state host checks passed\n");
    return 0;
}

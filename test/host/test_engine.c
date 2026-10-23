/**
 * Host checks for the engine on a fake device (fake_hal.h, fake_kv.h):
 * updates that confirm, roll back or fail, signed updates, and power cuts
 * at every write and slot switch of an update's whole life, each of which
 * must settle on a known image, never a trial that cannot end.
 *
 *   make -C test/host test
 */

#include <stdio.h>
#include <string.h>

#include "fake_hal.h"
#include "fake_kv.h"
#include "monocypher-ed25519.h"
#include "sota_engine.h"
#include "sota_engine_priv.h"
#include "sota_trial.h"

/* A simulated reset leaves the heap behind, as a real one does. */
const char *__asan_default_options(void);
const char *__asan_default_options(void) { return "detect_leaks=0"; }

static int g_failures = 0;

static void check(int cond, const char *msg)
{
    printf("%s  %s\n", cond ? "  ok" : "FAIL", msg);
    if (!cond) g_failures++;
}

static sota_engine_t g_e;
static sota_config_t g_cfg;
static const char *g_pems[2];
static const char *g_key_ids[2];
static int g_reset;
static bool g_checked;
static sota_apply_result_t g_applied;
static sota_confirm_result_t g_confirmed;
static uint32_t g_wait;

static void configure(void)
{
    memset(&g_cfg, 0, sizeof(g_cfg));
    g_cfg.token = "tok";
    g_cfg.device.device_id = "aa:bb:cc:dd:ee:ff";
    g_cfg.device.framework = "esp_idf";
    g_cfg.device.chip_family = "esp32";
    g_cfg.device.nvs_schema_version = 1;
    g_cfg.auto_confirm = true;
    g_cfg.auto_reboot = true;
    g_cfg.rollback = true;
    g_pems[0] = g_pems[1] = NULL;
}

/* Slot A runs build 1, which the device also reports. */
static void device_on_build_1(bool bl_rollback)
{
    fake_kv_reset();
    fake_world_reset(bl_rollback);
    sota_identity_t id;
    memset(&id, 0, sizeof(id));
    id.build = 1;
    sota_state_set_identity(&id);
    memset(&g_e, 0, sizeof(g_e));
    configure();
}

/* ---- actions, each run under setjmp: a reset longjmps out of them ------- */

static void act_boot(void)
{
    sota_engine_deinit(&g_e);
    sota_engine_waiting = false;
    fake_bootloader(g_reset);
    sota_engine_init(&g_e, &g_cfg);
    for (int i = 0; i < 2; ++i) {
        if (g_pems[i]) sota_engine_add_key(&g_e, g_key_ids[i], g_pems[i]);
    }
    sota_engine_validate_boot(&g_e);
}
static void act_check(void) { g_checked = sota_engine_check(&g_e); }
static void act_apply(void) { g_applied = sota_engine_apply(&g_e); }
static void act_confirm(void) { g_confirmed = sota_engine_confirm(&g_e); }
static void act_step(void) { g_wait = sota_engine_step(&g_e, true, NULL); }
static void act_reboot_for_update(void) { sota_engine_reboot_for_update(&g_e); }
static void act_timeout(void)
{
    sota_engine_timeout_fired(&g_e);
    sota_engine_service(&g_e);
}

/* 0 when the action returned, else the FAKE_JUMP_* that ended it. */
static int run(void (*action)(void))
{
    switch (setjmp(fake_reset_target)) {
    case 0:
        action();
        return 0;
    case FAKE_JUMP_RESTART:
        return FAKE_JUMP_RESTART;
    default:
        return FAKE_JUMP_POWER_CUT;
    }
}

/* A reset, then the adapter starting the engine. Follows any restart the
 * boot itself makes. Returns the number of restarts. */
static int boot(int reset_reason)
{
    int restarts = 0;
    g_reset = reset_reason;
    while (run(act_boot) == FAKE_JUMP_RESTART && restarts < 4) {
        g_reset = SOTA_RST_SW;
        ++restarts;
    }
    return restarts;
}

static bool on_trial(void) { return g_e.in_trial && fw.timer_armed; }

/* An update to build 2, applied and rebooted into: the trial is running. */
static void reach_trial(void)
{
    fake_server_offer(2, "dep-2");
    boot(SOTA_RST_POWERON);
    run(act_check);
    int jump = run(act_apply);
    check(jump == FAKE_JUMP_RESTART, "apply restarts into the update");
    boot(SOTA_RST_SW);
}

static void test_up_to_date(void)
{
    device_on_build_1(true);
    boot(SOTA_RST_POWERON);
    run(act_check);
    check(!g_checked && fs.checks == 1, "no offer: check reports none");
    check(fake_event_seen(SOTA_EVENT_CHECK_DONE) == 1, "CHECK_DONE emitted");
    run(act_apply);
    check(g_applied == SOTA_APPLY_NO_OFFER, "apply without an offer does nothing");
}

static void test_update_confirmed(void)
{
    device_on_build_1(true);
    reach_trial();
    check(fake_server_count("download_started", NULL) == 1 &&
              fake_server_count("downloaded", NULL) == 1 &&
              fake_server_count("flashed", NULL) == 1 &&
              fake_server_count("validated", NULL) == 1 &&
              fake_server_count("reboot", NULL) == 1,
          "apply posts its statuses in order");
    check(fw.running == 1 && fw.build[1] == 2, "the new image runs");
    check(on_trial() && fw.wdt_armed && g_e.st.live.build == 2, "it runs as a supervised trial");
    check(fw.timer_s == SOTA_DEFAULT_CONFIRM_TIMEOUT_S, "with the default confirm timeout");
    check(fake_event_seen(SOTA_EVENT_TRIAL_BOOT) == 1, "TRIAL_BOOT emitted");

    run(act_check);
    check(!g_e.in_trial && !fw.timer_armed && !fw.wdt_armed, "the first check-in confirms it");
    check(fw.state[1] == IMG_VALID, "the bootloader marks it valid");
    check(fake_server_count("confirmed", NULL) == 1, "confirmed is reported straight away");
    check(fake_event_seen(SOTA_EVENT_CONFIRMED) == 1, "CONFIRMED emitted");
    check(g_e.st.trial == 0 && g_e.st.conf_pend == 0 && g_e.outbox.count == 0,
          "nothing is left in storage");

    boot(SOTA_RST_POWERON);
    run(act_step);
    check(g_e.st.live.build == 2 && !g_e.in_trial && g_wait == SOTA_DEFAULT_CHECK_INTERVAL_S,
          "a later boot is a normal one");
}

static void test_confirm_timeout(bool bl_rollback)
{
    device_on_build_1(bl_rollback);
    reach_trial();
    fs.up = false;
    run(act_step);
    check(g_wait == SOTA_TRIAL_RETRY_INTERVAL_S, "a trial without a check-in polls quickly");
    check(run(act_timeout) == FAKE_JUMP_RESTART, "the confirm timeout restarts the device");
    check(fake_event_seen(SOTA_EVENT_ROLLING_BACK) == 1, "ROLLING_BACK emitted");
    check(!bl_rollback || fw.state[1] == IMG_INVALID, "the bootloader gives up the new image");
    boot(SOTA_RST_SW);
    check(fw.running == 0 && g_e.st.live.build == 1 && !g_e.in_trial,
          "the previous image runs and is reported");
    fs.up = true;
    run(act_step);
    check(fake_server_count("rolled_back", "confirm_timeout") == 1,
          "rolled_back confirm_timeout is reported");
    check(g_e.outbox.count == 0 && g_e.st.trial == 0, "nothing is left in storage");
}

static void test_crash_in_trial(bool bl_rollback)
{
    device_on_build_1(bl_rollback);
    reach_trial();
    int restarts = boot(SOTA_RST_PANIC);
    check(fw.running == 0 && g_e.st.live.build == 1 && !g_e.in_trial,
          "a trial image that panics is left");
    check(bl_rollback ? restarts == 0 : restarts == 1,
          bl_rollback ? "by the bootloader" : "by a software rollback on its next boot");
    run(act_step);
    check(fake_server_count("rolled_back", "panic") == 1, "rolled_back panic is reported");
}

static void test_hang_in_trial(void)
{
    device_on_build_1(false);
    reach_trial();
    check(fw.wdt_armed, "without bootloader rollback the chip watchdog still guards the trial");
    boot(SOTA_RST_WDT);
    check(fw.running == 0 && g_e.st.live.build == 1, "and its reset rolls back");
    run(act_step);
    check(fake_server_count("rolled_back", "watchdog") == 1, "rolled_back watchdog is reported");
}

static void test_reset_in_trial(bool bl_rollback, int reset, const char *reason)
{
    device_on_build_1(bl_rollback);
    reach_trial();
    boot(reset);
    char msg[96];
    snprintf(msg, sizeof(msg), "%s rollback: a %s reset in the trial rolls back",
             bl_rollback ? "bootloader" : "software", reason);
    check(fw.running == 0 && g_e.st.live.build == 1 && !g_e.in_trial, msg);
    run(act_step);
    check(fake_server_count("rolled_back", reason) == 1, "and is reported with its cause");
}

/* A power loss after the confirm is saved keeps the image. */
static void test_power_loss_after_confirm(bool bl_rollback)
{
    device_on_build_1(bl_rollback);
    reach_trial();
    fs.up = false;
    check(sota_engine_confirm(&g_e) == SOTA_CONFIRM_DONE, "a manual confirm while offline");
    boot(SOTA_RST_POWERON);
    fs.up = true;
    run(act_step);
    check(fw.running == 1 && g_e.st.live.build == 2 && !g_e.in_trial &&
              fake_server_count("confirmed", NULL) == 1 &&
              fake_server_count("rolled_back", NULL) == 0,
          "a power loss after a confirm keeps the image and reports it confirmed");
}

static void test_deep_sleep_in_trial(void)
{
    device_on_build_1(true);
    reach_trial();
    boot(SOTA_RST_DEEPSLEEP);
    check(fw.running == 0 && g_e.st.live.build == 1, "a trial that slept is rolled back");
    run(act_step);
    check(fake_server_count("rolled_back", "deep_sleep") == 1, "rolled_back deep_sleep is reported");
}

static void test_no_timer(void)
{
    device_on_build_1(true);
    fw.fail_timer = true;
    reach_trial();
    check(fw.running == 0 && g_e.st.live.build == 1,
          "a trial that cannot be supervised is rolled back");
}

static void test_manual_confirm(void)
{
    device_on_build_1(true);
    g_cfg.auto_confirm = false;
    reach_trial();
    run(act_check);
    check(on_trial(), "a check-in does not confirm with auto_confirm off");

    fake_server_offer(3, "dep-3");
    run(act_check);
    run(act_apply);
    check(g_checked && g_applied == SOTA_APPLY_UNCONFIRMED && g_e.has_offer,
          "an offer during the trial waits, kept");

    fw.fail_mark_valid = true;
    run(act_confirm);
    check(g_confirmed == SOTA_CONFIRM_FAILED && on_trial(),
          "a confirm the bootloader refuses leaves the trial running");
    fw.fail_mark_valid = false;
    run(act_confirm);
    check(g_confirmed == SOTA_CONFIRM_DONE && !g_e.in_trial, "a later confirm ends it");
    run(act_confirm);
    check(g_confirmed == SOTA_CONFIRM_NO_TRIAL, "confirming again does nothing");

    check(run(act_apply) == FAKE_JUMP_RESTART && fw.build[0] == 3,
          "the kept offer applies after the confirm");
}

static void test_timeout_during_failed_confirm(void)
{
    device_on_build_1(true);
    g_cfg.auto_confirm = false;
    reach_trial();
    sota_engine_timeout_fired(&g_e);
    check(fake_wakes == 1, "the timeout wakes the application");
    fw.fail_mark_valid = true;
    check(run(act_confirm) == FAKE_JUMP_RESTART,
          "a confirm that fails after the timeout fired rolls back");
    boot(SOTA_RST_SW);
    check(fw.running == 0 && g_e.st.live.build == 1, "to the previous image");
}

static void expect_failure(const char *what, const char *reason, sota_apply_result_t result)
{
    int before = fake_server_count("failed", reason);
    run(act_check);
    run(act_apply);
    char msg[96];
    snprintf(msg, sizeof(msg), "%s: %s", what, reason);
    check(g_applied == result && fake_server_count("failed", reason) == before + 1 &&
              fake_event_seen(SOTA_EVENT_UPDATE_FAILED) >= 1,
          msg);
    check(fw.boot == 0 && fw.running == 0 && g_e.st.trial == 0 && g_e.st.live.build == 1 &&
              !g_e.has_offer,
          "  and the device stays as it was");
}

static void test_apply_failures(void)
{
    device_on_build_1(true);
    fake_server_offer(2, "dep-2");
    boot(SOTA_RST_POWERON);

    fs.corrupt = true;
    expect_failure("corrupted download", "checksum_mismatch", SOTA_APPLY_CHECKSUM_FAIL);
    fs.corrupt = false;
    fs.dl_status = 404;
    expect_failure("missing file", "http_status_404", SOTA_APPLY_FLASH_FAIL);
    fs.dl_status = 0;
    expect_failure("no connection", "https_begin_failed", SOTA_APPLY_FLASH_FAIL);
    fs.dl_status = 200;
    fs.dl_fail_at = 2048;
    expect_failure("stalled download", "download_stalled", SOTA_APPLY_FLASH_FAIL);
    fs.dl_fail_at = -1;
    fw.fail_finish = true;
    expect_failure("image the bootloader rejects", "update_end_failed", SOTA_APPLY_FLASH_FAIL);
    fw.fail_finish = false;
    fw.fail_boot_set = true;
    expect_failure("slot switch refused", "update_end_failed", SOTA_APPLY_FLASH_FAIL);
    fw.fail_boot_set = false;
    fake_kv_fail_nth(0);
    expect_failure("trial record not saved", "state_save_failed", SOTA_APPLY_FLASH_FAIL);

    fs.chunked = true;
    check(run(act_check) == 0 && run(act_apply) == FAKE_JUMP_RESTART,
          "a download without a length uses the offered size");
}

static void test_rollback_disabled(void)
{
    device_on_build_1(true);
    g_cfg.rollback = false;
    reach_trial();
    check(!g_e.in_trial && fw.state[1] == IMG_VALID && g_e.st.live.build == 2,
          "without rollback the new image is accepted on its first boot");
    run(act_step);
    check(fake_server_count("confirmed", NULL) == 1 && g_e.outbox.count == 0,
          "and reported confirmed");
}

static void test_manual_reboot(void)
{
    device_on_build_1(true);
    g_cfg.auto_reboot = false;
    fake_server_offer(2, "dep-2");
    boot(SOTA_RST_POWERON);
    run(act_check);
    run(act_apply);
    check(g_applied == SOTA_APPLY_OK && sota_engine_update_waiting() && fw.running == 0,
          "with auto_reboot off, apply returns and the update waits");
    check(fake_server_count("reboot", NULL) == 0 && fake_event_seen(SOTA_EVENT_BEFORE_REBOOT) == 1,
          "BEFORE_REBOOT emitted, no reboot status yet");
    check(sota_engine_report(&g_e, "custom", NULL) == 200, "report() still names the deployment");
    run(act_check);
    check(!g_checked, "check-ins report the flashed build, so it is not offered again");
    fake_server_offer(3, "dep-3");
    run(act_check);
    run(act_apply);
    check(g_applied == SOTA_APPLY_REBOOT_PENDING, "nothing else is flashed over it");

    boot(SOTA_RST_DEEPSLEEP);
    run(act_apply);
    check(sota_engine_update_waiting() && g_applied == SOTA_APPLY_REBOOT_PENDING,
          "a deep-sleep wake still knows the update waits");

    check(run(act_reboot_for_update) == FAKE_JUMP_RESTART &&
              fake_server_count("reboot", NULL) == 1,
          "reboot_for_update posts reboot and restarts");
    boot(SOTA_RST_SW);
    check(on_trial() && g_e.st.live.build == 2, "into the trial");
}

static void test_report_retries(void)
{
    device_on_build_1(true);
    reach_trial();
    run(act_timeout);
    fs.up = false;
    boot(SOTA_RST_SW);
    uint32_t waits[3];
    for (int i = 0; i < 3; ++i) {
        run(act_step);
        waits[i] = g_wait;
    }
    check(waits[0] == SOTA_TRIAL_RETRY_INTERVAL_S && waits[1] == SOTA_TRIAL_RETRY_INTERVAL_S &&
              waits[2] == SOTA_DEFAULT_CHECK_INTERVAL_S,
          "a report without a response retries quickly, then at the interval");
    boot(SOTA_RST_POWERON);
    check(g_e.outbox.count == 1, "it survives a reboot");
    fs.up = true;
    run(act_step);
    check(g_e.outbox.count == 0 && fake_server_count("rolled_back", NULL) == 1,
          "and is sent once the server answers");
}

/* ---- signed updates ------------------------------------------------------ */

static void base64(const uint8_t *in, size_t n, char *out)
{
    static const char t[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t o = 0;
    for (size_t i = 0; i < n; i += 3) {
        uint32_t v = (uint32_t)in[i] << 16;
        if (i + 1 < n) v |= (uint32_t)in[i + 1] << 8;
        if (i + 2 < n) v |= in[i + 2];
        out[o++] = t[v >> 18 & 63];
        out[o++] = t[v >> 12 & 63];
        out[o++] = i + 1 < n ? t[v >> 6 & 63] : '=';
        out[o++] = i + 2 < n ? t[v & 63] : '=';
    }
    out[o] = '\0';
}

static void make_pem(const uint8_t pk[32], char *pem)
{
    uint8_t der[44] = {0x30, 0x2a, 0x30, 0x05, 0x06, 0x03, 0x2b, 0x65, 0x70, 0x03, 0x21, 0x00};
    memcpy(der + 12, pk, 32);
    char b64[64];
    base64(der, sizeof(der), b64);
    sprintf(pem, "-----BEGIN PUBLIC KEY-----\n%s\n-----END PUBLIC KEY-----\n", b64);
}

static char g_extra[256];

static void signed_offer(const uint8_t sk[32], const uint8_t pk[32], const char *key_id,
                         bool tamper)
{
    uint8_t sig[64];
    crypto_ed25519_sign(sig, sk, pk, fs.image, fs.image_len - (tamper ? 1 : 0));
    char b64[100];
    base64(sig, sizeof(sig), b64);
    snprintf(g_extra, sizeof(g_extra),
             "\"security_mode\":\"signed\",\"signature\":\"%s\",\"signing_key_id\":\"%s\"", b64,
             key_id);
    fs.extra = g_extra;
}

static void test_signed(void)
{
    uint8_t sk1[32], sk2[32], pk1[32], pk2[32];
    for (int i = 0; i < 32; ++i) {
        sk1[i] = (uint8_t)(i + 1);
        sk2[i] = (uint8_t)(0x80 + i);
    }
    crypto_ed25519_public_key(pk1, sk1);
    crypto_ed25519_public_key(pk2, sk2);
    static char pem1[200], pem2[200];
    make_pem(pk1, pem1);
    make_pem(pk2, pem2);

    device_on_build_1(true);
    g_cfg.device.security_mode = "signed";
    g_pems[0] = pem1;
    g_key_ids[0] = "k1";
    g_pems[1] = pem2;
    g_key_ids[1] = "k2";
    fake_server_offer(2, "dep-2");
    boot(SOTA_RST_POWERON);
    check(g_e.num_keys == 2, "two keys pinned");

    run(act_check);
    run(act_apply);
    check(g_applied == SOTA_APPLY_SIGNATURE_FAIL &&
              fake_server_count("failed", "signature_invalid") == 1 &&
              fake_server_count("download_started", NULL) == 0,
          "an unsigned offer is refused before the download");

    signed_offer(sk2, pk2, "k2", true);
    run(act_check);
    run(act_apply);
    check(g_applied == SOTA_APPLY_SIGNATURE_FAIL && fake_server_count("downloaded", NULL) == 1,
          "a signature over other bytes fails after the checksum");

    signed_offer(sk2, pk2, "k1", false);
    run(act_check);
    run(act_apply);
    check(g_applied == SOTA_APPLY_SIGNATURE_FAIL, "the key the offer names is the one used");

    signed_offer(sk2, pk2, "unknown", false);
    check(run(act_check) == 0 && run(act_apply) == FAKE_JUMP_RESTART,
          "an unknown key id tries every pinned key, and a good signature applies");
    check(sota_engine_add_key(&g_e, NULL, pem1) == false, "a third key is refused");
}

/* ---- power cuts ----------------------------------------------------------- */

/* Settled: no trial, no record, nothing to report, nothing more offered. */
static bool settled(void)
{
    return !g_e.in_trial && g_e.st.trial == 0 && g_e.st.staged == 0 && g_e.outbox.count == 0 &&
           !sota_engine_waiting && (!fs.offer_build || g_e.st.live.build >= fs.offer_build);
}

/*
 * One update's whole life with the power cut after `cut` writes and slot
 * switches. trial_fails: the trial times out instead of confirming. Returns
 * false once `cut` is past the last write, so the sweep stops.
 */
static bool cut_run(bool bl_rollback, bool trial_fails, int cut, char *why, size_t why_cap)
{
    device_on_build_1(bl_rollback);
    fake_server_offer(2, "dep-2");
    fake_kv_cut_after(cut);
    bool cut_hit = false;
    int reset = SOTA_RST_POWERON;
    for (int boots = 0; boots < 12; ++boots) {
        int jump = 0;
        g_reset = reset;
        jump = run(act_boot);
        for (int pass = 0; jump == 0 && pass < 12; ++pass) {
            if (settled()) {
                uint32_t running = fw.build[fw.running];
                if (g_e.st.live.build != running) {
                    snprintf(why, why_cap, "reports build %u but runs %u",
                             (unsigned)g_e.st.live.build, (unsigned)running);
                    return true;
                }
                if (running == 2 && fake_server_count("confirmed", NULL) == 0) {
                    snprintf(why, why_cap, "build 2 never reported confirmed");
                    return true;
                }
                why[0] = '\0';
                return cut_hit;
            }
            jump = run(trial_fails && g_e.in_trial ? act_timeout : act_step);
        }
        if (jump == FAKE_JUMP_POWER_CUT) {
            cut_hit = true;
            fake_kv_power_on();
            reset = SOTA_RST_POWERON;
        } else {
            reset = SOTA_RST_SW;
        }
    }
    snprintf(why, why_cap, "never settled (trial %u, live %u, running %u)",
             (unsigned)g_e.st.trial, (unsigned)g_e.st.live.build, (unsigned)fw.build[fw.running]);
    return true;
}

static void test_power_cuts(bool bl_rollback, bool trial_fails)
{
    char why[96] = "";
    int cuts = 0, bad = 0;
    for (int n = 0; n < 200; ++n) {
        if (!cut_run(bl_rollback, trial_fails, n, why, sizeof(why))) break;
        ++cuts;
        if (why[0]) {
            if (!bad++) printf("      cut after %d: %s\n", n, why);
        }
    }
    char msg[120];
    snprintf(msg, sizeof(msg), "%d power cuts (%s rollback, trial %s) all settle", cuts,
             bl_rollback ? "bootloader" : "software", trial_fails ? "times out" : "confirms");
    check(cuts > 20 && bad == 0, msg);
}

static void test_one_engine(void)
{
    device_on_build_1(true);
    run(act_boot);
    sota_engine_t other;
    check(!sota_engine_init(&other, &g_cfg), "a second engine is refused while one runs");
    fake_server_offer(2, "dep-2");
    int posts = fs.checks + fs.log_count;
    check(!sota_engine_check(&other) && sota_engine_apply(&other) == SOTA_APPLY_NO_OFFER &&
              sota_engine_confirm(&other) == SOTA_CONFIRM_NO_TRIAL &&
              sota_engine_report(&other, "x", NULL) < 0 && fs.checks + fs.log_count == posts,
          "and does nothing: no check-in, flash, confirm or report");
    reach_trial();
    sota_engine_deinit(&other);
    check(fw.wdt_armed && g_e.in_trial, "shutting it down leaves the running trial's watchdog");
    sota_engine_confirm(&g_e);
    check(sota_engine_init(&g_e, &g_cfg), "the running engine can set itself up again");
    sota_engine_deinit(&g_e);
    check(sota_engine_init(&other, &g_cfg), "once it is shut down, another can run");
    sota_engine_deinit(&other);
}

int main(void)
{
    test_up_to_date();
    test_update_confirmed();
    test_confirm_timeout(true);
    test_confirm_timeout(false);
    test_crash_in_trial(true);
    test_crash_in_trial(false);
    test_hang_in_trial();
    test_reset_in_trial(false, SOTA_RST_POWERON, "power_on");
    test_reset_in_trial(false, SOTA_RST_BROWNOUT, "brownout");
    test_reset_in_trial(false, SOTA_RST_SW, "reset");
    test_reset_in_trial(true, SOTA_RST_POWERON, "power_on");
    test_power_loss_after_confirm(false);
    test_power_loss_after_confirm(true);
    test_deep_sleep_in_trial();
    test_no_timer();
    test_manual_confirm();
    test_timeout_during_failed_confirm();
    test_apply_failures();
    test_rollback_disabled();
    test_manual_reboot();
    test_report_retries();
    test_signed();
    test_one_engine();
    test_power_cuts(true, false);
    test_power_cuts(true, true);
    test_power_cuts(false, false);
    test_power_cuts(false, true);

    printf("\n%s (%d failure%s)\n", g_failures ? "FAILED" : "PASSED", g_failures,
           g_failures == 1 ? "" : "s");
    return g_failures ? 1 : 0;
}

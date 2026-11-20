/**
 * Host checks for a build without signed updates
 * (-DSIMPLEOTA_DISABLE_SIGNED_UPDATES). It links without Monocypher, which
 * is the point of the option: this test fails to build if the engine still
 * reaches for it.
 *
 *   make -C test/host test
 */

#include <stdio.h>
#include <string.h>

#include "fake_hal.h"
#include "fake_kv.h"
#include "sota_boot.h"
#include "sota_engine.h"
#include "sota_engine_priv.h"

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
static sota_apply_result_t g_applied;

static void act_apply(void) { g_applied = sota_engine_apply(&g_e); }

/* 0 when the action returned, else the FAKE_JUMP_* that ended it. */
static int run(void (*action)(void))
{
    int jump = setjmp(fake_reset_target);
    if (jump == 0) action();
    return jump;
}

static void start(const char *security_mode)
{
    fake_kv_reset();
    fake_world_reset(true);
    sota_identity_t id;
    memset(&id, 0, sizeof(id));
    id.build = 1;
    sota_state_set_identity(&id);
    sota_boot_info_t info;
    sota_boot_info(&info);
    sota_state_set_image_id(info.image_id);

    sota_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.token = "tok";
    cfg.device.device_id = "aa:bb:cc:dd:ee:ff";
    cfg.device.framework = "esp_idf";
    cfg.device.chip_family = "esp32";
    cfg.device.security_mode = security_mode;
    cfg.auto_reboot = true;
    cfg.rollback = true;
    sota_engine_deinit(&g_e);
    sota_engine_waiting = false;
    sota_engine_init(&g_e, &cfg);
    sota_engine_validate_boot(&g_e);
}

int main(void)
{
    static const char pem[] = "-----BEGIN PUBLIC KEY-----\n"
                              "MCowBQYDK2VwAyEAGb9ECWmEzf6FQbrBZ9w7lshQhqowtrbLDFw4rXAxZuE=\n"
                              "-----END PUBLIC KEY-----\n";
    check(SOTA_SIGNED_UPDATES == 0, "the build leaves signed updates out");

    start(NULL);
    check(!sota_engine_add_key(&g_e, "k1", pem) && g_e.num_keys == 0, "no key can be pinned");

    fake_server_offer(2, "dep-2");
    check(sota_engine_check(&g_e) && run(act_apply) == FAKE_JUMP_RESTART,
          "an unsigned update installs");

    /* As on a device that has no key: the checksum is all there is. */
    start(NULL);
    fake_server_offer(2, "dep-2");
    fs.extra = "\"security_mode\":\"signed\",\"signature\":\"AAAA\"";
    check(sota_engine_check(&g_e) && run(act_apply) == FAKE_JUMP_RESTART,
          "a signed offer is taken on its checksum, as without a key");

    printf(g_failures ? "FAILED (%d failure%s)\n" : "PASSED (0 failures)\n", g_failures,
           g_failures == 1 ? "" : "s");
    return g_failures ? 1 : 0;
}

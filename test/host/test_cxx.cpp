/**
 * The core's headers as C++, the way the Arduino adapter includes them:
 * every header compiles under -std=c++11, and the C functions link.
 *
 *   make -C test/host test
 */

#include <stdio.h>
#include <string.h>

#include "monocypher-ed25519.h"
#include "sota_app.h"
#include "sota_boot.h"
#include "sota_engine.h"
#include "sota_image.h"
#include "sota_json.h"
#include "sota_report.h"
#include "sota_runner.h"
#include "sota_sys.h"
#include "sota_sha256.h"
#include "sota_kv.h"
#include "sota_net.h"
#include "sota_protocol.h"
#include "sota_signing.h"
#include "sota_state.h"
#include "sota_sys.h"
#include "sota_time.h"
#include "sota_trial.h"
#include "sota_url.h"
#include "sota_wdt_calc.h"

int main()
{
    int failures = 0;
#define CHECK(cond, msg)                                         \
    do {                                                         \
        printf("%s  %s\n", (cond) ? "  ok" : "FAIL", msg);      \
        if (!(cond)) ++failures;                                 \
    } while (0)

    sota_boot_facts_t f;
    memset(&f, 0, sizeof(f));
    f.trial = 1;
    f.new_part = 0x150000u;
    f.running_part = 0x150000u;
    CHECK(sota_classify_boot(&f) == SOTA_BOOT_TRIAL, "trial classifier");

    sota_outbox_t box;
    sota_outbox_init(&box);
    CHECK(sota_outbox_push(&box, "confirmed", nullptr, "d", 1) && sota_outbox_valid(&box),
          "outbox");

    bool clamped = false;
    CHECK(sota_wdt_stage_ticks(300, 60, 0, 150000, &clamped) == 54000000u, "watchdog maths");
    CHECK(sota_s_to_ticks(60, 1000) == 60000u, "ticks");
    CHECK(sota_validate_server_url("https://simpleota.com") == SOTA_URL_OK, "url");

    bool b = false;
    CHECK(sota_json_get_bool("{\"a\":true}", "a", &b) && b, "json links");

    sota_sha256_ctx sha;
    uint8_t digest[32];
    sota_sha256_init(&sha);
    sota_sha256_update(&sha, reinterpret_cast<const uint8_t *>("abc"), 3);
    sota_sha256_final(&sha, digest);
    CHECK(digest[0] == 0xba && digest[31] == 0xad, "sha256 links");

    CHECK(sota_signed_gate(true, 1, true, false) == SOTA_GATE_FAIL_CLOSED, "signing links");

    if (failures) {
        printf("%d FAILURE(S)\n", failures);
        return 1;
    }
    printf("C++ header checks passed\n");
    return 0;
}

/* Builds the core, the ESP32 port and the public API into an app, so all
 * of it is compiled and linked. It is not flashed. */

#include "nvs_flash.h"
#include "simpleota.h"

static bool connected(void *ctx)
{
    (void)ctx;
    return true;
}

#if SIMPLEOTA_API_VERSION < SIMPLEOTA_API_VERSION_NUM(1, 0)
#error "simpleota.h is older than this app expects"
#endif

void app_main(void)
{
    if (nvs_flash_init() != ESP_OK) return;
    simpleota_config_t cfg = {
        .token = "token",
        .is_connected = connected,
    };
    if (simpleota_init(&cfg) != ESP_OK) return;
    simpleota_start();
    simpleota_check_now();
    simpleota_stop();
    bool update = false;
    if (simpleota_check(&update) == ESP_OK && update) simpleota_apply();
    simpleota_confirm();
    simpleota_is_trial_boot();
    simpleota_current_build();
    simpleota_last_offered_version();
    simpleota_set_server_url("https://example.com");
    simpleota_reboot_for_update();
    simpleota_deinit();
}

/* Builds the core and the ESP32 port into an app, with stub adapter
 * functions, so both are compiled and linked. It is not flashed. */

#include <stdarg.h>

#include "nvs_flash.h"
#include "sota_app.h"
#include "sota_engine.h"
#include "sota_net.h"
#include "sota_runner.h"
#include "sota_sys.h"

int sota_net_post(void *user, const char *url, const char *token, const char *body,
                  char *resp, size_t cap)
{
    (void)user, (void)url, (void)token, (void)body, (void)resp, (void)cap;
    return -1;
}

int sota_net_dl_open(void *user, const char *url, int64_t *len, char *location,
                     size_t location_cap)
{
    (void)user, (void)url, (void)len, (void)location, (void)location_cap;
    return 0;
}

int sota_net_dl_read(void *user, uint8_t *buf, size_t cap)
{
    (void)user, (void)buf, (void)cap;
    return SOTA_NET_ERROR;
}

void sota_net_dl_close(void *user) { (void)user; }
void sota_app_event(void *user, const sota_event_t *event) { (void)user, (void)event; }
static sota_runner_t s_runner;

void sota_app_wake(void *user) { (void)user, sota_runner_wake(&s_runner); }

void sota_app_log(int level, const char *fmt, ...)
{
    (void)level, (void)fmt;
}

static sota_engine_t s_engine;

void app_main(void)
{
    if (nvs_flash_init() != ESP_OK) return;
    sota_config_t cfg = {
        .token = "token",
        .device = {.device_id = "id", .framework = "esp_idf", .chip_family = "esp32"},
        .auto_confirm = true,
        .auto_reboot = true,
        .rollback = true,
    };
    char id[SOTA_DEVICE_ID_LEN];
    sota_sys_default_device_id(id);
    cfg.device.device_id = id;
    if (!sota_engine_init(&s_engine, &cfg)) return;
    s_runner.engine = &s_engine;
    s_runner.stack_size = 8192;
    s_runner.priority = 5;
    sota_runner_start(&s_runner, "simpleota");
    sota_runner_on_task(&s_runner);
    sota_runner_stop(&s_runner, 1000);
    sota_engine_step(&s_engine, true);
    sota_engine_confirm(&s_engine);
    sota_engine_reboot_for_update(&s_engine);
    sota_engine_hold_pending_image();
}

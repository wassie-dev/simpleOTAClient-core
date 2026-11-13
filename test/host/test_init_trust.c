/**
 * Host checks for how simpleota_init() chooses the TLS trust, over stubs
 * for the engine, the task runner, FreeRTOS and the framework.
 *
 *   make -C test/host test
 */

#include <stdio.h>
#include <string.h>

#include "simpleota.h"
#include "sota_engine.h"
#include "sota_framework.h"
#include "sota_net_esp32.h"
#include "sota_runner.h"
#include "sota_sys.h"

#include "esp_log.h"
#include "esp_system.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

typedef esp_err_t (*attach_fn)(void *conf);

static attach_fn g_default;       /* The framework's default bundle, or NULL. */
static const char *g_default_pem; /* Or its default PEM, or NULL. */
static int g_default_calls;
static const sota_net_esp32_t *g_net; /* The engine's user pointer. */

bool sota_framework_default_trust(const char **cert_pem, attach_fn *crt_bundle_attach)
{
    ++g_default_calls;
    *cert_pem = g_default_pem;
    *crt_bundle_attach = g_default;
    return g_default || g_default_pem;
}
void sota_framework_init_logging(const char *tag) { (void)tag; }

bool sota_engine_init(sota_engine_t *e, const sota_config_t *cfg)
{
    (void)e;
    g_net = (const sota_net_esp32_t *)cfg->user;
    return true;
}
bool sota_engine_add_key(sota_engine_t *e, const char *key_id, const char *pem)
{
    (void)e; (void)key_id; (void)pem;
    return true;
}
void sota_engine_validate_boot(sota_engine_t *e) { (void)e; }
bool sota_engine_check(sota_engine_t *e) { (void)e; return false; }
sota_apply_result_t sota_engine_apply(sota_engine_t *e) { (void)e; return SOTA_APPLY_NO_OFFER; }
sota_confirm_result_t sota_engine_confirm(sota_engine_t *e) { (void)e; return SOTA_CONFIRM_FAILED; }
bool sota_engine_reboot_for_update(sota_engine_t *e) { (void)e; return false; }
void sota_engine_deinit(sota_engine_t *e) { (void)e; }
bool sota_engine_update_waiting(void) { return false; }
bool sota_runner_start(sota_runner_t *r, const char *name) { (void)r; (void)name; return false; }
bool sota_runner_stop(sota_runner_t *r, uint32_t timeout_ms) { (void)r; (void)timeout_ms; return true; }
void sota_runner_wake(sota_runner_t *r) { (void)r; }
bool sota_runner_on_task(const sota_runner_t *r) { (void)r; return false; }
void sota_sys_default_device_id(char out[SOTA_DEVICE_ID_LEN]) { strcpy(out, "host"); }
void esp_restart(void) {}
BaseType_t xTaskCreate(TaskFunction_t fn, const char *name, uint32_t stack, void *arg,
                       UBaseType_t prio, TaskHandle_t *out)
{
    (void)fn; (void)name; (void)stack; (void)arg; (void)prio; (void)out;
    return 0;
}
TaskHandle_t xTaskGetCurrentTaskHandle(void) { return NULL; }
UBaseType_t uxTaskPriorityGet(TaskHandle_t task) { (void)task; return 0; }
void vTaskDelete(TaskHandle_t task) { (void)task; }
void vTaskDelay(TickType_t ticks) { (void)ticks; }
SemaphoreHandle_t xSemaphoreCreateBinaryStatic(StaticSemaphore_t *buf) { return buf; }
BaseType_t xSemaphoreGive(SemaphoreHandle_t sem) { (void)sem; return pdTRUE; }
BaseType_t xSemaphoreTake(SemaphoreHandle_t sem, TickType_t ticks) { (void)sem; (void)ticks; return pdTRUE; }

static esp_err_t default_bundle(void *conf) { (void)conf; return ESP_OK; }
static esp_err_t app_bundle(void *conf) { (void)conf; return ESP_OK; }

static int g_failures = 0;

static void check(int cond, const char *msg)
{
    printf("%s  %s\n", cond ? "  ok" : "FAIL", msg);
    if (!cond) g_failures++;
}

/* Init with the given trust and framework default, recording the result. */
static esp_err_t init_with_pem_default(const char *cert_pem, attach_fn attach, attach_fn dflt,
                                       const char *dflt_pem)
{
    simpleota_deinit();
    g_default = dflt;
    g_default_pem = dflt_pem;
    g_default_calls = 0;
    g_net = NULL;
    simpleota_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.token = "token";
    cfg.cert_pem = cert_pem;
    cfg.crt_bundle_attach = attach;
    return simpleota_init(&cfg);
}

static esp_err_t init_with(const char *cert_pem, attach_fn attach, attach_fn dflt)
{
    return init_with_pem_default(cert_pem, attach, dflt, NULL);
}

int main(void)
{
    static const char pem[] = "-----BEGIN CERTIFICATE-----\n";
    static const char default_pem[] = "-----BEGIN CERTIFICATE-----\nroots\n";

    check(init_with(NULL, NULL, NULL) == ESP_ERR_INVALID_STATE,
          "no cert_pem, no bundle and no default: init fails closed");

    check(init_with(NULL, NULL, default_bundle) == ESP_OK && g_net &&
              !g_net->cert_pem && g_net->crt_bundle_attach == default_bundle,
          "no cert_pem and no bundle: the framework's default bundle");

    check(init_with(pem, NULL, default_bundle) == ESP_OK && g_net && g_net->cert_pem &&
              g_net->cert_pem != pem && strcmp(g_net->cert_pem, pem) == 0 &&
              !g_net->crt_bundle_attach && g_default_calls == 0,
          "cert_pem: a copy of it, and not the default bundle");

    check(init_with(pem, NULL, NULL) == ESP_OK && g_net && g_net->cert_pem,
          "cert_pem needs no default");

    check(init_with(NULL, app_bundle, default_bundle) == ESP_OK && g_net &&
              !g_net->cert_pem && g_net->crt_bundle_attach == app_bundle &&
              g_default_calls == 0,
          "crt_bundle_attach: the application's bundle, not the default");

    check(init_with(NULL, app_bundle, NULL) == ESP_OK && g_net &&
              g_net->crt_bundle_attach == app_bundle,
          "crt_bundle_attach needs no default");

    check(init_with_pem_default(NULL, NULL, NULL, default_pem) == ESP_OK && g_net &&
              g_net->cert_pem == default_pem && !g_net->crt_bundle_attach,
          "no cert_pem and no bundle: the framework's default PEM");

    check(init_with_pem_default(pem, NULL, NULL, default_pem) == ESP_OK && g_net &&
              g_net->cert_pem && g_net->cert_pem != default_pem &&
              strcmp(g_net->cert_pem, pem) == 0 && g_default_calls == 0,
          "cert_pem replaces the framework's default PEM");

    simpleota_deinit();
    return g_failures ? 1 : 0;
}

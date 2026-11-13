/**
 * Host checks for the default TLS trust, built for each framework with the
 * certificate bundle on and off (see the Makefile).
 *
 *   make -C test/host test
 */

#include <stddef.h>
#include <stdio.h>

#include "sota_framework.h"

#include "esp_crt_bundle.h"
#include "esp_log.h"

esp_err_t esp_crt_bundle_attach(void *conf)
{
    (void)conf;
    return ESP_OK;
}

esp_log_level_t esp_log_level_get(const char *tag)
{
    (void)tag;
    return ESP_LOG_ERROR;
}

void esp_log_level_set(const char *tag, esp_log_level_t level)
{
    (void)tag;
    (void)level;
}

static esp_err_t other_attach(void *conf)
{
    (void)conf;
    return ESP_OK;
}

int main(void)
{
    esp_err_t (*attach)(void *conf) = other_attach;
    const bool ok = sota_framework_default_trust(&attach);
#if CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
    const bool pass = ok && attach == esp_crt_bundle_attach;
    printf("%s  %s: the default is ESP-IDF's certificate bundle\n", pass ? "  ok" : "FAIL",
           SOTA_FRAMEWORK_NAME);
#else
    const bool pass = !ok && attach == NULL;
    printf("%s  %s: without the bundle there is no default trust\n", pass ? "  ok" : "FAIL",
           SOTA_FRAMEWORK_NAME);
#endif
    return pass ? 0 : 1;
}

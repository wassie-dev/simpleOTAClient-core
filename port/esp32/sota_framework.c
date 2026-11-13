/**
 * sota_framework.c - see sota_framework.h. On Arduino it also holds the
 * bootloader hook, here because simpleota_init() calls into this file: a
 * PlatformIO library archive links an object only when something uses it,
 * and the hook must replace Arduino-ESP32's weak definition.
 */

#include "sota_framework.h"

#include <stddef.h>

#include "esp_log.h"

#if defined(ARDUINO)
#if __has_include("esp_arduino_version.h")
#include "esp_arduino_version.h"
#endif
/* 2.0.4 is the first core whose prebuilt ESP-IDF links the bundle. */
#if !defined(ESP_ARDUINO_VERSION)
#error "SimpleOTAClient needs Arduino-ESP32 2.0.4 or later"
#elif ESP_ARDUINO_VERSION < ESP_ARDUINO_VERSION_VAL(2, 0, 4)
#error "SimpleOTAClient needs Arduino-ESP32 2.0.4 or later"
#endif
#endif

#if CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
#include "esp_crt_bundle.h"
#endif

/* A Kconfig choice on ESP-IDF; Arduino-ESP32 2.0.4 and later have the full
 * bundle. */
bool sota_framework_default_trust(esp_err_t (**crt_bundle_attach)(void *conf))
{
#if CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
    *crt_bundle_attach = esp_crt_bundle_attach;
    return true;
#else
    *crt_bundle_attach = NULL;
    return false;
#endif
}

#if defined(ARDUINO)

#include "nvs_flash.h"
#include "sota_engine.h"
#include "sota_sys.h"
#include "sota_wdt_calc.h"

/* Arduino-ESP32 logs only errors by default; the Arduino library always
 * showed its warnings, such as a rejected token. */
void sota_framework_init_logging(const char *tag)
{
    if (esp_log_level_get(tag) < ESP_LOG_WARN) esp_log_level_set(tag, ESP_LOG_WARN);
}

#if CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE && !defined(SIMPLEOTA_DISABLE_BOOTLOADER_ROLLBACK)
/* initArduino() accepts a pending image before setup() unless this returns
 * true. Only a SimpleOTA trial is held; any other image, such as one
 * written by ArduinoOTA, is accepted as usual. This runs before
 * Arduino-ESP32's own nvs_flash_init(): initialise here, never erase.
 * A held trial gets the chip watchdog straight away, so a hang before
 * setup() reaches simpleota_init() is reset too; the default confirm
 * timeout applies until then. */
bool verifyRollbackLater(void)
{
    if (nvs_flash_init() != ESP_OK) return false;
    const bool hold = sota_engine_hold_pending_image();
    if (hold) {
        sota_sys_wdt_arm(SOTA_DEFAULT_CONFIRM_TIMEOUT_S,
                         sota_wdt_floor_sec(CONFIG_SIMPLEOTA_HTTP_TIMEOUT_MS));
    } else {
        sota_sys_wdt_quiesce();
    }
    return hold;
}
#endif

#else /* ESP-IDF */

void sota_framework_init_logging(const char *tag)
{
    (void)tag;
}

#endif

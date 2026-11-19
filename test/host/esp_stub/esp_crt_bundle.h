/* Host stand-in for ESP-IDF's esp_crt_bundle.h. */
#ifndef ESP_CRT_BUNDLE_H
#define ESP_CRT_BUNDLE_H
#include "esp_err.h"
esp_err_t esp_crt_bundle_attach(void *conf);
#endif

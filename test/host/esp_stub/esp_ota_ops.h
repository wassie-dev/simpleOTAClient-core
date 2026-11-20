/* Host stand-in for ESP-IDF's esp_ota_ops.h: what sota_image_ota.c uses. */
#ifndef ESP_OTA_OPS_H
#define ESP_OTA_OPS_H
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
typedef uint32_t esp_ota_handle_t;
typedef struct {
    uint32_t address;
    uint32_t size;
} esp_partition_t;
#define OTA_WITH_SEQUENTIAL_WRITES 0xfffffffeu
const esp_partition_t *esp_ota_get_running_partition(void);
const esp_partition_t *esp_ota_get_next_update_partition(const esp_partition_t *start_from);
esp_err_t esp_ota_begin(const esp_partition_t *partition, size_t image_size,
                        esp_ota_handle_t *out_handle);
esp_err_t esp_ota_write(esp_ota_handle_t handle, const void *data, size_t size);
esp_err_t esp_ota_abort(esp_ota_handle_t handle);
esp_err_t esp_ota_end(esp_ota_handle_t handle);
#endif

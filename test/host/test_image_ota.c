/**
 * Host checks for the ESP32 port's sota_image_accepts(), built for an
 * ESP32-C6 (CONFIG_IDF_FIRMWARE_CHIP_ID 0x000D) over a stub esp_ota_ops.h.
 *
 *   make -C test/host test
 */

#include <stdio.h>
#include <string.h>

#include "sota_image.h"

#include "esp_ota_ops.h"

const esp_partition_t *esp_ota_get_running_partition(void) { return NULL; }
const esp_partition_t *esp_ota_get_next_update_partition(const esp_partition_t *start_from)
{
    (void)start_from;
    return NULL;
}
esp_err_t esp_ota_begin(const esp_partition_t *partition, size_t image_size,
                        esp_ota_handle_t *out_handle)
{
    (void)partition; (void)image_size; (void)out_handle;
    return ESP_FAIL;
}
esp_err_t esp_ota_write(esp_ota_handle_t handle, const void *data, size_t size)
{
    (void)handle; (void)data; (void)size;
    return ESP_FAIL;
}
esp_err_t esp_ota_abort(esp_ota_handle_t handle) { (void)handle; return ESP_OK; }
esp_err_t esp_ota_end(esp_ota_handle_t handle) { (void)handle; return ESP_FAIL; }

static int g_failures = 0;

static void check(int cond, const char *msg)
{
    printf("%s  %s\n", cond ? "  ok" : "FAIL", msg);
    if (!cond) g_failures++;
}

int main(void)
{
    /* The first 24 bytes of Arduino-ESP32 3.3 builds of BasicOTA. */
    static const uint8_t c6[24] = {
        0xe9, 0x05, 0x02, 0x20, 0x3c, 0x08, 0x80, 0x40, 0xee, 0x00, 0x00, 0x00,
        0x0d, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x01,
    };
    static const uint8_t esp32[24] = {
        0xe9, 0x06, 0x02, 0x2f, 0x2c, 0x1c, 0x08, 0x40, 0xee, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x01,
    };
    uint8_t h[24];

    check(sota_image_accepts(c6, sizeof(c6)), "an ESP32-C6 image is accepted");
    check(sota_image_accepts(c6, 14), "the header's first 14 bytes are enough");
    check(!sota_image_accepts(esp32, sizeof(esp32)), "an ESP32 image is refused");

    memcpy(h, c6, sizeof(h));
    h[12] = 0x00;
    h[13] = 0x0d;
    check(!sota_image_accepts(h, sizeof(h)), "chip id 0x0D00 is not 0x000D: little-endian");

    memcpy(h, c6, sizeof(h));
    h[0] = 0x7f;
    check(!sota_image_accepts(h, sizeof(h)), "a first byte other than 0xE9 is refused");

    memset(h, 0, sizeof(h));
    check(sota_image_accepts(h, 13), "13 bytes cannot tell, so they are accepted");

    return g_failures ? 1 : 0;
}

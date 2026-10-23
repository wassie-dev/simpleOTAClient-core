/* Calls into the core and the ESP32 port, so both are compiled and linked. */

#include "nvs_flash.h"
#include "sota_state.h"

void app_main(void)
{
    if (nvs_flash_init() != ESP_OK) return;
    sota_state_t st;
    sota_state_load(&st);
    sota_outbox_t box;
    sota_state_load_outbox(&box);
    sota_state_set_identity(&st.live);
}

// Builds the core, the ESP32 port and the public API into a sketch, so all
// of it is compiled and linked for Arduino-ESP32. CI copies core/,
// port/esp32/ and include/ into a library first.

#include <simpleota.h>

static bool connected(void*) { return true; }

void setup() {
  simpleota_config_t cfg = {};
  cfg.token = "token";
  cfg.is_connected = connected;
  if (simpleota_init(&cfg) != ESP_OK) return;
  simpleota_start();
  simpleota_check_now();
  simpleota_stop();
  if (simpleota_check()) simpleota_apply();
  simpleota_confirm();
  simpleota_is_trial_boot();
  simpleota_current_build();
  simpleota_last_offered_version();
  simpleota_set_server_url("https://example.com");
  simpleota_reboot_for_update();
  simpleota_deinit();
}

void loop() {}

// Builds the core and the ESP32 port into a sketch, with stub adapter
// functions, so both are compiled and linked for Arduino-ESP32. CI copies
// core/ and port/esp32/ into a library first. The stubs are in stubs.cpp.

#include <sota_engine.h>

static sota_engine_t engine;

void setup() {
  sota_config_t cfg = {};
  cfg.token = "token";
  cfg.device.device_id = "id";
  cfg.device.framework = "arduino";
  cfg.device.chip_family = "esp32";
  cfg.rollback = true;
  if (!sota_engine_init(&engine, &cfg)) return;
  sota_engine_step(&engine, true, nullptr);
  sota_engine_confirm(&engine);
  sota_engine_hold_pending_image();
}

void loop() {}

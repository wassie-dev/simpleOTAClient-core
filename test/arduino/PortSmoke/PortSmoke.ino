// Builds the core and the ESP32 port into a sketch, with stub adapter
// functions, so both are compiled and linked for Arduino-ESP32. CI copies
// core/ and port/esp32/ into a library first.

#include <sota_app.h>
#include <sota_engine.h>
#include <sota_net.h>

extern "C" {
int sota_net_post(void*, const char*, const char*, const char*, char*, size_t) { return -1; }
int sota_net_dl_open(void*, const char*, int64_t*) { return 0; }
int sota_net_dl_read(void*, uint8_t*, size_t) { return SOTA_NET_ERROR; }
void sota_net_dl_close(void*) {}
void sota_app_event(void*, const sota_event_t*) {}
void sota_app_wake(void*) {}
void sota_app_log(int, const char*, ...) {}
}

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

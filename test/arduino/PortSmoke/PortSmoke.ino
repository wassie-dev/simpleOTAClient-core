// Calls into the core and the ESP32 port, so both are compiled and linked
// for Arduino-ESP32. CI copies core/ and port/esp32/ into a library first.

#include <sota_state.h>

void setup() {
  sota_state_t st;
  sota_state_load(&st);
  sota_outbox_t box;
  sota_state_load_outbox(&box);
  sota_state_set_identity(&st.live);
}

void loop() {}

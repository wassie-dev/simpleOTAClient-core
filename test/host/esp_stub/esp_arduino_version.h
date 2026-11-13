/* Host stand-in for Arduino-ESP32's esp_arduino_version.h; a test may set
 * the version with -DSTUB_ARDUINO_VERSION_PATCH (2.0.x). */
#ifndef ESP_ARDUINO_VERSION_H
#define ESP_ARDUINO_VERSION_H
#define ESP_ARDUINO_VERSION_VAL(major, minor, patch) (((major) << 16) | ((minor) << 8) | (patch))
#ifndef STUB_ARDUINO_VERSION_PATCH
#define STUB_ARDUINO_VERSION_PATCH 17
#endif
#define ESP_ARDUINO_VERSION ESP_ARDUINO_VERSION_VAL(2, 0, STUB_ARDUINO_VERSION_PATCH)
#endif

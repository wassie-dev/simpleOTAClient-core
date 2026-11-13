/* Host stand-in for Arduino-ESP32's esp_arduino_version.h. A test sets the
 * 2.0.x patch with -DSTUB_ARDUINO_VERSION_PATCH, or leaves the version out
 * with -DSTUB_NO_ARDUINO_VERSION. */
#ifndef ESP_ARDUINO_VERSION_H
#define ESP_ARDUINO_VERSION_H
#ifndef STUB_NO_ARDUINO_VERSION
#define ESP_ARDUINO_VERSION_VAL(major, minor, patch) (((major) << 16) | ((minor) << 8) | (patch))
#ifndef STUB_ARDUINO_VERSION_PATCH
#define STUB_ARDUINO_VERSION_PATCH 17
#endif
#define ESP_ARDUINO_VERSION ESP_ARDUINO_VERSION_VAL(2, 0, STUB_ARDUINO_VERSION_PATCH)
#endif
#endif

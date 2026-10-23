# SimpleOTA client API

The SimpleOTA client for ESP32 checks for firmware updates on the
[SimpleOTA](https://simpleota.com) platform, streams and verifies them, and
runs each new image as a trial that rolls back unless it is confirmed.

It ships as two packages with the same API, built from these sources:

- [SimpleOTAClient-Arduino](https://github.com/xanderwasserman/SimpleOTAClient-Arduino)
  for Arduino-ESP32 and PlatformIO
- [SimpleOTAClient-ESP-IDF](https://github.com/xanderwasserman/SimpleOTAClient-ESP-IDF)
  for ESP-IDF

## Using the client

Everything an application calls is in **simpleota.h**: fill a
::simpleota_config_t, call simpleota_init() early, then either
simpleota_start() for the managed background task, or simpleota_check() and
simpleota_apply() from your own loop. The clients' READMEs cover
installation, rollback, anti-rollback, signed firmware and troubleshooting.

## Inside the core

The rest of this reference is for working on the client itself. The engine
(sota_engine.h) is portable C99 with no platform headers, and is tested on
a host. Where it needs the platform it declares one small interface per
concern, bound at link time:

| Interface | What it provides | ESP32 port |
| --- | --- | --- |
| sota_kv.h | The key-value store the state lives in | NVS |
| sota_boot.h | Which image runs and which boots next | esp_ota_* |
| sota_image.h | Writing an update into the next app slot | esp_ota_* |
| sota_sys.h | Time, locking, the confirm timer and the chip watchdog | FreeRTOS, esp_timer, wdt_hal |
| sota_net.h | HTTP | esp_http_client |
| sota_runner.h | The managed task | FreeRTOS |
| sota_app.h | Events and log lines for the application | simpleota.c |

What is stored, and in which order, is a contract with releases in the
field: see the [NVS contract](NVS_CONTRACT.md).

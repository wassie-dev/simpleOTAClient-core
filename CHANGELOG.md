# Changelog

## 0.1.0 (unreleased)

The first release: the logic both clients carried their own copies of, in
one place.

### Added

- `sota_trial.h`, `sota_report.h`, `sota_time.h`, `sota_json`, `sota_signing`,
  `sota_sha256` and Monocypher 3.1.3, from SimpleOTAClient-ESP-IDF 0.2.1.
- `sota_url.h`, from SimpleOTA-Arduino 0.6.0, so both clients validate the
  server URL the same way.
- `sota_wdt_calc.h`, the watchdog deadline maths from both clients, with the
  ESP-IDF client's margin floor that grows with the HTTP timeout.
- `sota_s_to_ticks()`, for durations in seconds.
- `sota_hold_pending_image()`, from the Arduino client's
  `verifyRollbackLater()`.
- `sota_state`, the persisted state shared by both clients: identity, trial
  record and outbox, over the `sota_kv.h` key-value interface. Both
  partition keys go into every trial record. Host tests cut power, or fail
  a write, at every write of staging and of a rollback, and check that the
  next boot ends on a known identity with no record left behind.
- `port/esp32/sota_kv_nvs.c`, `sota_kv.h` on NVS, compiled in CI under
  ESP-IDF 5.1 to latest and Arduino-ESP32 2.0.17 and 3.3.0.
- Host tests merged from both clients, a C++11 build of every header, and
  CI with GCC, Clang and sanitizers.
- `scripts/sync-core.sh`, which copies the core into a client and checks a
  copy.

### Changed, compared with the clients' own copies

- The trial classifier reads `sota_new_part` when the record has it and
  falls back to `sota_prev_part`. Records from Arduino releases up to 0.6.0,
  which only write `sota_prev_part`, now classify on the ESP-IDF client too,
  instead of being dropped as incomplete.
- Tick conversion saturates at `UINT32_MAX - 1` on both clients. The Arduino
  client saturated at `UINT32_MAX`, which FreeRTOS reads as "wait forever".

# SimpleOTAClient-core

The portable C99 core shared by the two SimpleOTA clients:

- [SimpleOTA-Arduino](https://github.com/xanderwasserman/SimpleOTA-Arduino) (Arduino-ESP32, PlatformIO)
- [SimpleOTAClient-ESP-IDF](https://github.com/xanderwasserman/SimpleOTAClient-ESP-IDF) (ESP-IDF component)

It is not installed on its own. Each client carries a copy of `core/` as
plain files, so every way of installing a client (Arduino Library Manager,
a GitHub ZIP, PlatformIO, an ESP-IDF git dependency or the component
registry) gets it. Git submodules are not used, because the Library Manager
and GitHub archives publish them as empty folders.

## What is in it

Nothing here includes an Arduino, ESP-IDF or FreeRTOS header, so all of it
is tested on a host. Where the core needs the platform, it declares a small
interface (such as `sota_kv.h`) that the platform port defines. These are
bound at link time, not through function pointers, so nothing is called
indirectly and the linker drops what a build does not use.

| File | What it does |
| --- | --- |
| `sota_engine.{h,c}` | The engine: boot validation, check-ins, the trial with its confirm and rollback, reports, and a managed task's next wait. |
| `sota_apply.c` | Download, verify and flash an offer, then save the trial record and switch the boot slot, in that order. |
| `sota_trial.h` | What a boot means for a saved trial record: trial, rolled back, abandoned, waiting, and so on. Reads records from every earlier release of both clients. |
| `sota_report.h` | The outbox of `confirmed` / `rolled_back` reports (one NVS blob) and the poll interval policy. |
| `sota_state.{h,c}` | Everything a client persists (identity, trial record, outbox) and the order it is written in, so a power cut between any two writes is recoverable. The keys are a contract with releases in the field: see [docs/NVS_CONTRACT.md](docs/NVS_CONTRACT.md). |
| `sota_protocol.{h,c}` | The check-in and status request bodies, written into a caller's buffer with every string escaped, and the check response read into an offer. No heap. |
| `sota_kv.h` | The key-value store the state lives in. The platform port implements it (NVS on ESP32); the host tests use an in-memory fake that can cut power after any write. |
| `sota_wdt_calc.h` | The chip watchdog's deadline for a trial: confirm timeout plus a margin. |
| `sota_time.h` | Millisecond and second durations to FreeRTOS ticks, without 32-bit wrap. |
| `sota_url.h` | Server base URL validation and joining. |
| `sota_json.{h,c}` | The minimal JSON scanner and string escaper used for server responses and request bodies. |
| `sota_signing.{h,c}` | The signed-firmware gate (anti-downgrade), PEM key parsing, base64 signatures, and the streaming two-key Ed25519 verifier. |
| `sota_sha256.{h,c}` | A dependency-free SHA-256, the same on every IDF version (mbedTLS 4 in IDF 6 has no legacy API). |
| `monocypher*` | [Monocypher](https://monocypher.org/) 3.1.3, unmodified. See `core/LICENSE-vendor.md`. |

The interfaces, one per concern:

| Header | What it covers | Implemented by |
| --- | --- | --- |
| `sota_kv.h` | The stored state | the port |
| `sota_image.h` | Writing an update into the next slot | the port |
| `sota_boot.h` | Which image runs and boots next; mark valid; roll back | the port |
| `sota_sys.h` | Sleep and time, the lock, the confirm timer, the chip watchdog, restart, the default device ID | the port |
| `sota_runner.h` | The managed task: start, stop, wake | the port |
| `sota_net.h` | HTTP: check-in and status posts, the firmware download | the client |
| `sota_app.h` | Events, log lines, and a wake-up when the confirm timeout fires | the client |

`port/esp32/` implements the port's half with the ESP-IDF APIs that both
ESP-IDF (5.1 and later) and Arduino-ESP32 (2.x and 3.x) provide:

| File | Implements |
| --- | --- |
| `sota_kv_nvs.c` | `sota_kv.h` on NVS, namespace `simpleota` |
| `sota_image_ota.c` | `sota_image.h` on `esp_ota_*` |
| `sota_boot_ota.c` | `sota_boot.h` on `esp_ota_*` and `esp_reset_reason()` |
| `sota_sys_esp32.c` | `sota_sys.h` on FreeRTOS and `esp_timer` |
| `sota_runner_esp32.c` | `sota_runner.h` on a FreeRTOS task |
| `sota_wdt_esp32.c` | The trial watchdog on `wdt_hal` |

A client then supplies `sota_net.h` and `sota_app.h`, owns a
`sota_engine_t` and a `sota_runner_t`, and calls `sota_engine_check()` and
`sota_engine_apply()` (polling), or starts the runner (managed). Its
`sota_app_wake()` calls `sota_runner_wake()`.
`SOTA_LOG_LEVEL` sets which log lines are compiled in, and `SOTA_DL_CHUNK`
the download buffer (1024 bytes).

## Tests

```sh
make -C test/host test
```

CI also compiles `core/` and `port/esp32/` in a small ESP-IDF app
(`test/esp-idf`, several IDF versions and targets) and an Arduino sketch
(`test/arduino`, cores 2.0.17 and 3.3.0).

`test_engine` runs the engine on a fake two-slot device, with and without
bootloader rollback, and cuts power after every write and slot switch of
an update's whole life, checking that each run settles on the image it
reports.

The tests build as C99 with `-Werror`, and `test_cxx` builds every header
as C++11, the way the Arduino client includes them. CI also runs them under
AddressSanitizer and UndefinedBehaviorSanitizer with GCC and Clang.

## Updating a client

1. Tag a release of this repository (`vX.Y.Z`) and add it to `CHANGELOG.md`.
2. In the client, with this repository checked out at that tag beside it:

   ```sh
   ../SimpleOTAClient-core/scripts/sync-core.sh src/sota_core          # Arduino
   ../SimpleOTAClient-core/scripts/sync-core.sh simpleota/src/core     # ESP-IDF
   ```

   The script replaces the folder's contents with `core/` and
   `port/esp32/`, side by side, and writes the tag to `CORE_VERSION` in it.
3. The client's CI checks out the tag named in `CORE_VERSION` and runs
   `sync-core.sh --check` on the folder, so a hand edit to the copy fails
   CI. Change the core here instead.

## License

MIT, see `LICENSE`. Monocypher is BSD-2-Clause OR CC0-1.0.

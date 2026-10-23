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
is tested on a host.

| File | What it does |
| --- | --- |
| `sota_trial.h` | What a boot means for a saved trial record: trial, rolled back, abandoned, waiting, and so on. Reads records from every earlier release of both clients. |
| `sota_report.h` | The outbox of `confirmed` / `rolled_back` reports (one NVS blob) and the poll interval policy. |
| `sota_wdt_calc.h` | The chip watchdog's deadline for a trial: confirm timeout plus a margin. |
| `sota_time.h` | Millisecond and second durations to FreeRTOS ticks, without 32-bit wrap. |
| `sota_url.h` | Server base URL validation and joining. |
| `sota_json.{h,c}` | The minimal JSON scanner and string escaper used for server responses and request bodies. |
| `sota_signing.{h,c}` | The signed-firmware gate (anti-downgrade), PEM key parsing, base64 signatures, and the streaming two-key Ed25519 verifier. |
| `sota_sha256.{h,c}` | A dependency-free SHA-256, the same on every IDF version (mbedTLS 4 in IDF 6 has no legacy API). |
| `monocypher*` | [Monocypher](https://monocypher.org/) 3.1.3, unmodified. See `core/LICENSE-vendor.md`. |

The engine (check, apply, boot validation, confirm and rollback) and an
ESP32 platform port move here in later phases of the plan.

## Tests

```sh
make -C test/host test
```

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

   The script replaces the folder's contents and writes the tag to
   `CORE_VERSION` in it.
3. The client's CI checks out the tag named in `CORE_VERSION` and runs
   `sync-core.sh --check` on the folder, so a hand edit to the copy fails
   CI. Change the core here instead.

## License

MIT, see `LICENSE`. Monocypher is BSD-2-Clause OR CC0-1.0.

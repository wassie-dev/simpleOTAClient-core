# Changelog

## 1.2.0 (unreleased)

API 1.2: additions only. A configuration that leaves the new field at zero
and keeps its security mode behaves as in 1.1, and nothing stored changes.

Not for release before the platform and the CLI send what it reads: until
then no offer carries a strict signature, and a `STRICT` device takes no
update.

### Added

- **Strict signatures.** The image signature covers the firmware bytes only,
  and the server assigns the build number, so a server that was taken over
  could offer an old signed image under a higher number, or another
  project's image when two projects share a key. The SimpleOTA CLI now
  makes a second signature with the same key once it knows the build
  number, over a small text (project id, key id, build number, image
  SHA-256), and the device writes the same text from what it holds itself
  and checks that signature before it downloads anything. The code calls
  the text a manifest; `sota_manifest.h` has the format.
- `SIMPLEOTA_SECURITY_STRICT`: `SIGNED`, and an update without a strict
  signature that verifies is refused (reason `signature_invalid`). It needs
  a signing key and `project_id`; init fails without either. The device
  reports `security_mode` `signed` and `"require_manifest":true` on
  check-ins.
- `simpleota_config_t::project_id`. With it and a pinned key, a device in
  any mode checks a strict signature when the offer carries one, and
  refuses a wrong one. That lets a fleet on `SIGNED` prove its strict
  signatures before it moves to `STRICT`. It is a rehearsal, not the
  protection: outside `STRICT` a server can leave the strict signature out,
  and the update is taken on its image signature as before. Without
  `project_id` strict signatures are not looked at, as in 1.1.
- The check response's optional `manifest_signature` (base64, 64 bytes) and
  `manifest_version` (1 when absent) are read. A version this client does
  not know counts as no strict signature.

Costs about 1 KB of flash, and during the check a second verifier on the
heap for a moment (about 700 bytes), freed before the download starts.

## 1.1.0 (unreleased)

API 1.1: additions only. A configuration that leaves the new field at zero
behaves as in 1.0, and nothing stored changes.

### Added

- `simpleota_config_t::can_update`: in managed mode the application can put
  an offered update off (low battery, a job that must not be interrupted).
  Nothing is downloaded, and the next check, or `simpleota_check_now()`,
  asks again. `sota_app_can_update()` is its hook in `sota_app.h`.
- A check-in answered with 429 or 503 and a `Retry-After` in seconds waits
  that long, a day at most, and up to a tenth longer so a fleet told the
  same number does not come back at once. A trial keeps its quick checks:
  its confirm timeout does not wait for a busy server. `sota_net_post()`
  hands the header back through a new argument.
- A download that is not a firmware image for this chip is dropped after
  its first bytes with the new reason `image_mismatch`, before anything is
  written, where it used to download in full and fail `esp_ota_end()`.
  `sota_image_accepts()` is new in `sota_image.h`.
- `-DSIMPLEOTA_DISABLE_SIGNED_UPDATES` builds the client without the
  Ed25519 verifier, for devices that never take a signed update: about
  13 KB less flash. `simpleota_init()` then refuses signing keys and
  `SIMPLEOTA_SECURITY_SIGNED`, and a signed offer is handled as on a device
  with no key pinned. The host tests link this variant without Monocypher.

## 1.0.1 (unreleased)

Fixes from a security and robustness review. No public API change, and
nothing stored changes. Ports add `sota_sys_random()` to `sota_sys.h`.

### Changed

- The managed task retries a check-in that got no answer (or a 5xx, 408 or
  429), and a download that broke, after 30 s, 2 min and 10 min before it
  falls back to the check interval. Until now one dropped connection cost a
  whole interval, an hour by default. Each failed download still posts its
  `failed` status, so a flaky link reports up to three more for each run
  of failures.
- A check-in out of memory, and a download that cannot allocate its
  redirect buffer (reported as `https_begin_failed`), are retried on the
  same schedule as a network failure.
- The managed task spreads every wait of a minute or more by up to a tenth
  either way, so devices that power up together do not check in together
  from then on. `sota_sys_random()` is new in `sota_sys.h` for it.
- `simpleota_init()` warns when signing keys are pinned without
  `SIMPLEOTA_SECURITY_SIGNED`: in any other mode an update the server marks
  unsigned is accepted on its checksum, which the header now says too.
- A check that offers nothing no longer allocates the 2 KB URL buffer.

### Fixed

- `sota_json_get_string()` took a string cut off by the end of the response
  as complete, so a check response longer than the 4 KB buffer could yield
  half a value. It now fails.
- `sota_json_get_string()` decodes `\uXXXX` (to UTF-8) instead of dropping
  the backslash: a server or proxy that escapes `&` as `\u0026` no longer
  breaks the pre-signed download URL. A surrogate pair, as a server that
  escapes all non-ASCII sends an emoji, decodes to one character. A
  malformed escape, U+0000 and a surrogate outside a pair fail the value.
- A download longer than the announced size stops at once, before the
  excess is written to flash, instead of running to the end of the stream.
  The reason is `short_read`, as before. It is not retried early, since it
  fails the same way every time; a download that ends short is.

## 1.0.0 (2026-10-25)

The first release: the logic both clients carried their own copies of, in
one place.

### Added

- An API reference generated by Doxygen from the headers, which CI builds on
  every push (failing on any undocumented declaration) and publishes to
  GitHub Pages, one version per tag plus `main`, keeping earlier versions.
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
- `sota_protocol`: the check-in and status bodies and the check response,
  shared by both clients, with no heap and no printf. An offer whose
  `signature_algorithm` is present but not a short string is now unusable
  (fail closed); the ESP-IDF client treated it as ed25519.
- `sota_json_has_key()`.
- `port/esp32/sota_kv_nvs.c`, `sota_kv.h` on NVS, compiled in CI under
  ESP-IDF 5.1 to latest and Arduino-ESP32 2.0.17 and 3.3.0.
- The engine, `sota_engine` and `sota_apply`: boot validation, check-ins,
  the apply pipeline, the trial with its confirm and rollback, deferred
  reports and the managed task's schedule, behind link-time interfaces:
  `sota_image.h`, `sota_boot.h` and `sota_sys.h` from the port, and
  `sota_net.h` and `sota_app.h` (events, logging, wake-up) from the client.
- `port/esp32/`: `sota_image_ota.c`, `sota_boot_ota.c`, `sota_sys_esp32.c`
  and `sota_wdt_esp32.c`, the trial watchdog from both clients.
- API version 1.0 (`SIMPLEOTA_API_VERSION`). `simpleota_check(&update)`
  returns `ESP_OK` when the server answered and `ESP_FAIL` when it did not,
  so polling apps can tell a failed check from no update. The header states
  the compatibility rules (zero means default, additions only within 1.x)
  and lists the stable `reason` tokens. `security_mode` is an enum,
  `simpleota_security_t` (none, basic, signed); the old "token" mode is
  gone, because a device reporting it was offered neither basic nor signed
  builds. Signed mode without a signing key fails init, because the device
  would be offered signed builds it cannot verify.
- The public API, `include/simpleota.h`, with `port/esp32/simpleota.c` and
  the `esp_http_client` transport, `sota_net_esp32.c`: the ESP-IDF client's
  API, now the API of both clients. `sota_framework.{h,c}` holds what
  differs in the Arduino build: Kconfig defaults, the ISRG Root X1 and X2
  certificates as the default CA, warnings at Arduino's default log level,
  and the `verifyRollbackLater()` bootloader hook. `sync-core.sh` takes the
  header's destination as a second argument.
- simpleota_check() and simpleota_apply() run on a short-lived task with
  CONFIG_SIMPLEOTA_TASK_STACK_SIZE while the caller waits. A signed update
  checked on the ESP-IDF main task, whose default stack is 3584 bytes,
  overflowed it.
- A refused downgrade is reported to the server once per boot, not on
  every check. Every check logs its result at info level.
- `sota_runner.h` and `port/esp32/sota_runner_esp32.c`: the managed task
  both clients ran, with its initial delay, connection check, cooperative
  stop and the confirm timeout's rollback task.
- `sota_sys_default_device_id()`, the Wi-Fi station MAC both clients used
  as the default device ID.
- The confirm time is clamped in one place, `sota_engine_init()`, and
  `sota_sys_now_ms()` is added. The engine times a download stall
  (`download_stalled`) itself: a transport returns `SOTA_NET_AGAIN` while
  no byte has arrived, instead of timing it.
- Anti-rollback: an offer whose build number is not above the running
  build is always refused (`downgrade_refused`, `SOTA_APPLY_DOWNGRADE`),
  because an old image's signature still verifies. To go back, release the
  old code under a new, higher build number.
- `sota_boot_info_t.image_id`, the running image's ELF SHA-256, stored as
  `sota_image`. A device that boots an image SimpleOTA did not install,
  such as a serial flash of another project, reports build 0 instead of
  the stored build, which would otherwise refuse that project's builds.
  With `sota_config_t.image_build` set, it reports that build instead, so a
  release flashed over USB is not replaced by the server.
- `sota_json_is_object()`. Labels that are not one JSON object are left out
  of the check-in with a warning, so a typo can't break every check-in.
- `docs/NVS_CONTRACT.md`: every key both clients store, its type, values,
  write order and the outbox layout, with the rules for changing them.
  `test_contract` checks them, including records written by Arduino 0.6.0
  and ESP-IDF 0.2.x.
- Engine tests on a fake two-slot device, with and without bootloader
  rollback, that cut power at every write and slot switch of an update's
  life, through to its confirm or rollback, and check that each run
  settles on the image it reports.
- Host tests merged from both clients, a C++11 build of every header, and
  CI with GCC, Clang and sanitizers.
- `scripts/sync-core.sh`, which copies the core into a client and checks a
  copy.

### Changed, compared with the clients' own copies

- The trial classifier reads `sota_new_part` when the record has it and
  falls back to `sota_prev_part`. Records from Arduino releases up to 0.6.0,
  which only write `sota_prev_part`, now classify on the ESP-IDF client too,
  instead of being dropped as incomplete.
- Rollback marks the image invalid through the bootloader when it holds the
  image on probation, and otherwise switches back to the previous slot, as
  the Arduino client did. The ESP-IDF client chooses whether to use it
  without bootloader rollback.
- Without the bootloader's probation, a trial ends at any reset of the new
  image, as the bootloader's does: a crash, a watchdog, a brownout, a power
  loss, a restart or deep sleep, reported with that reason. Its first boot
  saves `sota_trial = 3` for this. Before, each reset restarted the trial
  with a fresh confirm timeout. A rollback cut short by a power loss is
  finished on the next boot.
- One engine runs per device; a second `sota_engine_init()` is refused and
  that engine does nothing.
- The trial watchdog runs without bootloader rollback too: its reset now
  ends in that software rollback. Before, it ran only with bootloader
  rollback.
- The confirm timeout's timer only notes the timeout and wakes the client,
  which rolls back from a task, as the ESP-IDF client did.
- During a trial, check-ins are sent once and queued reports wait, as the
  ESP-IDF client did, so the confirm timeout waits behind one request at
  most. Polling follows the ESP-IDF client's schedule.
- Downloads follow up to three redirects on both clients, walked by the
  engine instead of the transport, and each only to another `https://` URL
  (`redirect_not_https`). `sota_net_dl_open()` returns a redirect's
  `Location` instead of following it.
- The update slot is erased as the download is written, and an image larger
  than the slot is refused before the download.
- JSON keys are matched in the top-level object only, and numbers past 32
  bits are refused.
- A rollback's reason is the one it saved, then a slot the bootloader marked
  invalid (`confirm_timeout`), then the reset reason.
- Tick conversion saturates at `UINT32_MAX - 1` on both clients. The Arduino
  client saturated at `UINT32_MAX`, which FreeRTOS reads as "wait forever".

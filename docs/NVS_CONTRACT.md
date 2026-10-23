# NVS contract

What SimpleOTA stores on a device, and the rules that keep it readable by
every release of both clients.

A device updates from one release of the library to another, and can roll
back to the one before, in the middle of a trial. So the image that reads
these keys is often not the one that wrote them. Any change that an older or
newer release would read differently can leave a device on the wrong image,
reporting the wrong build, or stuck in a trial. This file is the contract.
`core/sota_state.c` implements it, and `test/host/test_contract.c` checks it.

## Rules

1. **Never rename, retype or remove a key**, and never give an existing value
   a new meaning. Every key below is read by releases already in the field.
2. **New keys are additive.** A release that does not know a key must be able
   to ignore it, and a release that knows it must cope with it being missing
   (it was written by an older release). Add the key to the table below with
   the first release that writes it.
3. **New values must read safely as old ones.** For example, older releases
   read any `sota_trial` value other than 0 and 2 as a trial, so 3 was safe
   to add. Check how every older reader handles a new value before using it.
4. **Keep the write order.** The order below is what makes every power cut
   recoverable. Writes that are not listed go through `sota_state.c` and are
   added to this file.
5. **A blob layout never changes in place.** To change the outbox, write a
   new key (such as `sota_outbox2`), keep reading the old one, and leave its
   layout as it is.
6. **Changing the contract needs a test.** Extend `test_contract.c` with the
   new key or value, written as the older release writes it and read back as
   the older release reads it.

## Where

- ESP32: NVS namespace `"simpleota"` in the default `nvs` partition
  (`port/esp32/sota_kv_nvs.c`). Arduino's `Preferences("simpleota")` is the
  same store, so both clients read the same keys.
- Key names are at most 15 characters (the NVS limit).
- Strings are NUL-terminated NVS strings; `u8` and `u32` are NVS integers of
  that width. A key read with another type is treated as missing.
- Anyone erasing the NVS partition or the `simpleota` namespace resets the
  device to "no update yet": build 0, no trial, no reports.

## Keys

| Key | Type | Meaning | Written since |
|---|---|---|---|
| `sota_build` | u32 | Build number the device reports as running. 0 or missing: none yet. | all |
| `sota_hash` | str | sha256 hex (64 chars) of that build's image, or empty. | all |
| `sota_ver` | str | Version label of that build, at most 64 chars, else empty. | all |
| `sota_image` | str | ELF SHA-256 prefix (16 hex chars) of the image `sota_build` was last seen running on. Missing: not noted yet. | core 0.1.0 (Arduino 1.0.0, ESP-IDF 1.0.0) |
| `sota_trial` | u8 | 0 or missing: no trial. 1: an update is staged, its image has not booted. 3: its image has booted once. 2: a rollback happened, its report not yet queued. Any value other than 0 and 2 is a trial. | Arduino 0.2, ESP-IDF 0.1; 3 since core 0.1.0 |
| `sota_staged` | u8 | Present only while a trial record is written but the boot slot has not switched: 1 started, 2 previous identity saved in full. | Arduino 0.5, ESP-IDF 0.2 |
| `sota_new_part` | u32 | Flash address the update was written to. | ESP-IDF all, Arduino 1.0.0 |
| `sota_prev_part` | u32 | Flash address of the image the update replaced. | all with rollback |
| `sota_prev_build` | u32 | \ | all with rollback |
| `sota_prev_hash` | str | > Identity before the update, restored by a rollback. | all with rollback |
| `sota_prev_ver` | str | / | all with rollback |
| `sota_fail_dep` | str | Deployment ID the `confirmed` or `rolled_back` report names (under 64 chars). | all with rollback |
| `sota_fail_build` | u32 | Build that report names. | all with rollback |
| `sota_conf_pend` | u8 | 1 from staging until the `confirmed` report is queued. | all with rollback |
| `sota_rb_reason` | str | Why `sota_trial` is 2 (under 24 chars). Missing means `confirm_timeout`. | Arduino 0.5, ESP-IDF 0.1 |
| `sota_outbox` | blob | Reports waiting to be sent; layout below. Missing: none. | Arduino 0.5, ESP-IDF 0.2 |

"All with rollback" means Arduino 0.2 and ESP-IDF 0.1 onwards.

Readers fall back as follows, and must keep doing so:

- `sota_new_part` missing: the update's image is the one that is not at
  `sota_prev_part` (Arduino releases up to 0.6.0 write only `sota_prev_part`).
- `sota_rb_reason` missing with `sota_trial` 2: `confirm_timeout`.
- `sota_image` missing: treated like an image that differs (see below).

## Write order

Each step is in this order so that a power cut between any two writes leaves
a state the next boot resolves to a known image and identity.

**Staging an update** (before the boot slot switches):

1. `sota_staged` = 1
2. `sota_new_part`, `sota_prev_part`, `sota_prev_build`, `sota_prev_hash`, `sota_prev_ver`
3. `sota_staged` = 2
4. `sota_build`, `sota_hash`, `sota_ver` (the update's identity)
5. `sota_fail_dep`, `sota_fail_build`, `sota_conf_pend` = 1
6. erase `sota_rb_reason`
7. `sota_trial` = 1 (last)
8. switch the boot slot, then erase `sota_staged`

A failure at any step erases the trial keys again and, once step 3 was
reached, writes the previous identity back. The slot does not switch.

**Update applied without a trial** (rollback off): the update's identity,
then `sota_fail_dep`, `sota_fail_build`, `sota_conf_pend` = 1. No
`sota_trial`.

**First boot of the update's image:** `sota_trial` = 3.

**Confirm:** erase `sota_trial`. What is left reads as an update applied
without a trial, so the next boot queues its `confirmed` report if this one
cannot.

**Rollback:** previous identity into `sota_build`, `sota_hash`, `sota_ver`,
then `sota_rb_reason`, then `sota_trial` = 2 (last), then leave the image.

**Queuing a report:** write `sota_outbox`, then erase the trial keys,
`sota_trial` first: `sota_trial`, `sota_staged`, `sota_new_part`,
`sota_prev_part`, `sota_prev_build`, `sota_prev_hash`, `sota_prev_ver`,
`sota_fail_dep`, `sota_fail_build`, `sota_conf_pend`, `sota_rb_reason`.

**Image noted:** `sota_image` is written at boot whenever it differs from the
running image. If that boot has no trial record and no pending `confirmed`
report, nothing explains the new image (it was flashed some other way, such
as over USB), so first `sota_build` is set to the image's declared build
(`image_build`, else 0) and `sota_hash`, `sota_ver` to empty.

## Outbox blob

`sota_outbox` is one blob of 212 bytes, little-endian, natural alignment:

| Offset | Size | Field |
|---|---|---|
| 0 | 1 | `version`, 1 |
| 1 | 1 | `count`, 0 to 2 |
| 2 | 2 | padding |
| 4 | 104 | report 0 |
| 108 | 104 | report 1 |

Each report:

| Offset | Size | Field |
|---|---|---|
| 0 | 12 | `event`, NUL-terminated: `confirmed` or `rolled_back` |
| 12 | 24 | `reason`, NUL-terminated, empty when none |
| 36 | 64 | `deployment`, NUL-terminated, not empty |
| 100 | 4 | `build`, u32, not 0 |

A blob with another version or size, a count above 2, or a report that
breaks these rules is ignored as if empty.

## Compatibility checked

`test/host/test_contract.c` checks:

- the key types and values above, and the outbox layout;
- that trial records written as Arduino 0.6.0 and ESP-IDF 0.2.x write them
  load and classify as those releases meant;
- that what this release writes is readable, with the same types, by those
  releases.

`test/host/test_state.c` and `test_engine.c` cut power at every write above
and check that each run ends on the image it reports.

Not covered by host tests, and part of the hardware test list for each
release: an OTA update from the previous release of each client that is
confirmed, and one that rolls back to it.

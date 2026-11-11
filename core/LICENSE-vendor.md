# Third-party: Monocypher

`monocypher.c` and `monocypher.h` here, and `monocypher-ed25519.c` and
`monocypher-ed25519.h` in `port/portable/` (used by the host tests, not
copied into the clients), are a vendored, unmodified copy of
[Monocypher](https://monocypher.org/) **3.1.3** (the last release with the
incremental Ed25519 check API), byte-identical to the copies shipped in
SimpleOTA-Arduino 0.6.0 and SimpleOTAClient-ESP-IDF 0.2.1.

Monocypher is dual-licensed **BSD-2-Clause OR CC0-1.0**,
Copyright (c) 2017-2019 Loup Vaillant. Full license text is embedded in the
headers of each file.

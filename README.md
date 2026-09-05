# MuziWorks Superbase Meshtastic Firmware

> **Withdrawn: v2.8.0-superbase.7.** Built-in navigation was reported nonfunctional after installation. Its release and binary assets have been removed. Do not reinstall the withdrawn files. The Git tag is retained only for audit history.

Custom Meshtastic firmware maintained exclusively for the **MuziWorks Superbase** (`muzi-base`, nRF52840).

This fork is intentionally scoped to the Superbase. Hardware definitions, board catalogs, build matrices, and release automation for other physical Meshtastic nodes are not maintained here. Shared Meshtastic core code and native/Portduino test infrastructure are retained where required to build and validate the Superbase firmware.

## Release status

**[v2.8.0-superbase.8 — Navigation Correction](https://github.com/Amoulier/meshtastic-superbase-firmware/releases/tag/v2.8.0-superbase.8)** is the latest published final release. Runtime firmware version: **`2.8.0.cc704b8`**. The release tag points to the exact compiled and tested source, `cc704b80956017ded590de61bf2a667eb36cc820`; later commits update documentation only.

The release contains the identical OTA and UF2 files previously supplied as the navigation candidate. They were not rebuilt or substituted for publication. A node already running `2.8.0.cc704b8` does not need reflashing solely because the release tag changed.

The corrected candidate passed **614 cases in 24 native suites**, with zero failures, errors or skipped cases. Its 21 navigation cases all passed. Source, test attribution, state isolation, package integrity and uploaded-asset SHA-256 checks were reconciled again before publication. Final verification: [33981068477](https://github.com/Amoulier/meshtastic-superbase-firmware/actions/runs/33981068477). Publication: [33981982943](https://github.com/Amoulier/meshtastic-superbase-firmware/actions/runs/33981982943).

**Physical-device navigation confirmation remains pending.** The release was published at the repository owner's explicit request after automated verification; final release status does not assert that the owner's hardware symptom has been confirmed repaired. No automated build should be described as verified on the user's hardware.

See `docs/SUPERBASE_NAVIGATION_AUDIT.md` and the historical pre-publication report `docs/SUPERBASE_NAVIGATION_RESULTS.md` for the findings, changed paths and validation limits. Their earlier candidate/release-status statements are superseded by the publication above, not their physical-validation limits. The correction retains short button taps, polls switches when an IRQ is not delivered, and adds regression tests through the actual input broker and BaseUI handler. Board GPIO assignments are unchanged.

## Selective reliability changes retained in source

The source retains the reviewed Bluetooth administration, SX1262/LR1121 radio recovery, GPS fix tracking, muted-message screen behavior, security-key restoration and module-metadata changes previously integrated into the withdrawn .7 build. Withdrawal supersedes that build's earlier release recommendation and automated audit results.

`docs/SUPERBASE_RELEASE_7.md` is retained as historical provenance, not as approval to install the withdrawn release. Other physical board targets, unrelated radio backends, protocol schemas and the nRF52 toolchain are not imported or changed by the navigation correction.

## Superbase-specific behavior

This fork preserves the existing Superbase customizations:

- MQTT implicit-ACK handling so a channel message already proven relayed does not later regress to a delivery failure because of timeout/retransmit handling.
- **DMs Only** buzzer behavior and RTTTL ownership/locking protections.
- A 12-hour stationary position interval for `MUZI_BASE`.
- Superbase-only ICM20948/AK09916 power management: magnetometer power-down with the sleeping IMU and restoration on wake.
- Existing GPS/display power handling and the MuziWorks OTA update path.
- Unchanged RX Boosted Gain and LoRa TX power policy.

## Repository scope

The only maintained physical PlatformIO target is `muzi-base`.

Relevant hardware definitions are limited to:

```text
boards/muzi-base.json
variants/nrf52840/muzi_base/
variants/nrf52840/nrf52.ini
variants/nrf52840/nrf52840.ini
variants/nrf52840/cpp_overrides/
```

`variants/native/` remains solely for regression tests, not as a supported physical node target.

## Build and validation

Clone with submodules and build with `pio run -e muzi-base`.

The Superbase CI checks repository scope, runs native suites and builds a fresh `muzi-base` image. Navigation tests use simulated GPIO and an in-memory display with production input/UI code; they do not verify electrical signals, physical nRF52 interrupt delivery or the real OLED. Release publication and physical-device validation are recorded separately. Passing automated checks is not proof of absence of every defect, nor a substitute for checking navigation on the actual device.

## Installation

Back up configuration before any firmware change. A navigation correction does **not** require a factory reset, changed board pins or regenerated identity keys.

For a Superbase with the MuziWorks OTAFIX bootloader, use `firmware-muzi-base-2.8.0.cc704b8-ota.zip` **without extracting it**. For USB/UF2, copy `firmware-muzi-base-2.8.0.cc704b8.uf2` to the bootloader drive. A bundle or audit ZIP is not itself an OTA package. Do not use .7 files retained from the withdrawn release.

## Upstream

This repository is derived from Meshtastic firmware. Upstream changes are reviewed and selectively integrated so that Superbase-specific behavior and power optimizations are not overwritten by unrelated hardware changes.

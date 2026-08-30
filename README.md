# MuziWorks Superbase Meshtastic Firmware

Custom Meshtastic firmware maintained exclusively for the **MuziWorks Superbase** (`muzi-base`, nRF52840).

This fork is intentionally scoped to the Superbase. Hardware definitions, board catalogs, build matrices, and release automation for other physical Meshtastic nodes are not maintained here. Shared Meshtastic core code and native/Portduino test infrastructure are retained where they are required to build and validate the Superbase firmware.

## Current release

The current installable release is **v2.8.0-superbase.5**. Use the repository's **Releases** page for the validated OTA, UF2, manifest, and bundle files.

## Superbase-specific behavior

This fork preserves the Superbase work validated on physical hardware, including:

- MQTT implicit-ACK handling so a channel message already proven relayed does not later regress to a delivery failure because of timeout/retransmit handling.
- Correct **DMs Only** buzzer behavior and RTTTL ownership/locking protections.
- A 12-hour stationary position interval for `MUZI_BASE`, reducing unnecessary stationary transmissions.
- Superbase-only ICM20948/AK09916 power management: the magnetometer enters power-down with the sleeping IMU and returns to continuous 100 Hz operation on wake.
- GPS/display power handling and the MuziWorks OTA update path used by the Superbase.
- RX Boosted Gain and LoRa TX power behavior remain unchanged by these customizations.

## Repository scope

The only physical PlatformIO target maintained by this fork is:

```text
muzi-base
```

Relevant hardware definitions are intentionally limited to:

```text
boards/muzi-base.json
variants/nrf52840/muzi_base/
variants/nrf52840/nrf52.ini
variants/nrf52840/nrf52840.ini
variants/nrf52840/cpp_overrides/
```

`variants/native/` remains solely for automated native tests and is not a supported physical node target.

## Build

Clone with submodules and build the Superbase target with PlatformIO:

```bash
pio run -e muzi-base
```

## Validation

The repository CI is intentionally Superbase-only. It checks repository scope, runs the critical native suites used by this fork, and builds a fresh `nrf52840 / muzi-base` firmware artifact.

## Installation

For an existing Superbase with the MuziWorks OTAFIX bootloader, use the `firmware-muzi-base-*-ota.zip` file from the latest release **without extracting it**.

For USB recovery/update through the UF2 bootloader, copy the `firmware-muzi-base-*.uf2` file from the release to the mounted bootloader drive.

Back up the node configuration before flashing.

## Upstream

This repository is derived from Meshtastic firmware. Upstream changes are reviewed and selectively integrated so that Superbase-specific behavior and power optimizations are not overwritten by unrelated hardware changes.

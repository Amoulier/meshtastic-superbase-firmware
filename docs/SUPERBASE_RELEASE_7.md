# Superbase 2.8.0-superbase.7: selective update and audit

## Scope

Base: `4cbba7006a9fed23cb1a778b7e62422ba96ee8bc` (`v2.8.0-superbase.6`).
Reviewed upstream snapshot: `fdb67309aa8fb9a019e07160ac72024c3d25ce2d`, 2026-09-05.
This is a selective Meshtastic 2.8.0 fork update, **not** a full upstream 2.8.1 merge.
The only maintained physical build target remains `muzi-base` / nRF52840.

## Incorporated upstream changes

| PR | Upstream commit | Applied scope |
| --- | --- | --- |
| #11651 | `47db0e3020a608e06fb65cce70cd2f093021bd82` | Preserve BLE during non-rebooting admin transactions; correct restored-preference reboot delay. |
| #11676 | `7dffd66c59b8ab45933f68192898ae01c012c990` | SX126x/LR11x0 reconfiguration recovery only. |
| #11678 | `df34ef1081ff5ff2f7ed23cb8319b6219353196c` | Shared recovery policy, SX126x/LR11x0 hot paths and periodic maintenance. |
| #11671 | `3683566f62575f96ba47e2ee5b51b4fbc27e2d10` | BaseUI message-banner fix needed by the following mute integration. |
| #11688 | `14eaa5587d571b76326f0e63c33c6d72d1f6fa38` | Shared mute/alert predicates and tests; minimal adaptation of our notification module. |
| #11697 | `4cb912ff55ba225a0037352320e026d09de005b2` | GPS fix validity remembered for the entire search cycle. |
| #11686 | `83198c1cbb982e6cdbf641ecc9c67063889435a6` | Validate restored/derived weak keys and report actual key replacement. |
| #11709 | `104923730f907925ba30e45a159b9751ee0ba0c8` | Accurate module-exclusion metadata, including unsupported Store & Forward. |

For each PR, the authoritative source is `https://github.com/meshtastic/firmware/pull/NUMBER`.
Other boards, unrelated radio backends, the Nordic platform v11 update, device-ui updates,
and new protobuf schemas are deliberately not imported. #11659 was already incorporated.

## Corrections found during this audit

- A nested checkout in the native setup action silently replaced an explicitly selected
  candidate SHA with the workflow-triggering revision. The setup action no longer performs
  checkout, and tests/build validation assert the exact source revision.
- Radio recovery now throttles attempts made at timestamp zero and across the 32-bit clock
  rollover, saturates the failure counter and prevents a wrapped reboot deadline from
  becoming the zero/unarmed sentinel. Existing scheduled reboots remain intact.
- Any SX126x `begin()` error fails the attempt; a successful later command cannot hide it.
- Failed SX126x/LR11x0 reconfiguration explicitly marks RX offline for periodic retries.
  Reconfiguration reports RX-start failure instead of returning false success.

## Preserved behavior

MQTT implicit ACK handling, routing, DMs Only buzzer behavior, independent notification
outputs, RTTTL ownership/locking, stationary position interval, IMU/magnetometer sleep,
GPIO definitions, RX Boosted Gain policy, TX power configuration and OTAFIX update handling
are preserved. Source audit compares the protected files with the preceding release and
requires the notification-module delta to be exactly the shared mute-predicate adaptation.
Security changes reject known weak keys; they do not deliberately replace a valid identity.

## Required release gates

The release process requires all selected native suites to pass under AddressSanitizer,
with nonempty correctly attributed JUnit cases and clean isolated state. Additional tests
exercise GPS fix latching/reset/rollover and production radio recovery policy with mocked
chip operations. No actual radio hardware is simulated by those mocks.

The final source must build for `muzi-base`. Package checks require the correct MCU/model,
source-version suffix, CRC16, SoftDevice requirement, complete UF2 block sequence, nRF52840
family ID, valid application vectors, matching OTA/UF2 application bytes, and no writes
into bootloader, configuration or reserved warm-store flash regions. SHA-256 checksums and
machine-readable test/source/package audit evidence accompany the published release.

Passing these gates means **no errors detected by these audits**, not proof of the absence
of all bugs. This release has no new physical-device, RF range, battery-life or long-duration
field validation performed by this automated audit. Back up configuration before updating.

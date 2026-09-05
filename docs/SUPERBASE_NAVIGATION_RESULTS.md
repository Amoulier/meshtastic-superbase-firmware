# Superbase navigation audit results — 2026-09-05

## Status

Release `v2.8.0-superbase.7` and its published binary assets were removed after the user reported that the navigation buttons did not change the interface. Its Git tag is retained only for history. The latest remaining published release is `.6`; this document does not certify `.6` on the user's device either.

The corrected firmware is a **candidate pending physical-device confirmation**, runtime `2.8.0.cc704b8`, compiled from `cc704b80956017ded590de61bf2a667eb36cc820`. Subsequent commits in this correction branch update documentation only. No replacement final release was published.

## Findings and attribution limits

The old driver required a direction interrupt action and a still-low GPIO at the later poll. It discarded a released direction tap even though an interrupt had recorded the press. Without a delivered interrupt, neither direction nor SELECT could start an action. The old driver also shared direction debounce state and used a nonzero timestamp to gate hold tracking.

These conditions were reproduced with controlled GPIO inputs. They do **not** prove which condition caused the complete nonresponse of the user's physical unit. In particular, no GPIOTE channel exhaustion was observed on that device and no live device logs were available.

The old `src/input/TrackballInterruptBase.cpp` has the same Git blob, `77fa2ff88bd7e30d227814083e722609fd8132f1`, in `.6` and `.7`. The identified input defects therefore predate the last release; they must not be misattributed to a newly imported radio/GPS patch without further evidence.

The candidate uses a Superbase-only mechanical-switch path with independent IRQ latches, sampled-level fallback, per-button debounce, wrap-safe timing and an ISR wake of the cooperative loop. It retains the original GPIO assignments. Initialization and individual input events are logged to help distinguish an uninitialized input source from a later event/UI problem.

## Reproducible comparison

The same `test_superbase_navigation` fixture was applied to the withdrawn production input driver and to the correction. The baseline changed only the test file, a test-only Screen friend declaration and test-state metadata; production input source remained unchanged.

| Check | Withdrawn driver | Corrected candidate |
| --- | --- | --- |
| Navigation suite | 21 cases: 18 expected assertion failures, 3 passes | 21 passes, 0 failures/errors/skips |
| Full selected native regression matrix | Not repeated for this negative comparison | 614 cases in 24 suites, all passed |
| Native memory/state checks | No crash or sanitizer diagnostic; state clean | AddressSanitizer, test attribution and isolated-state checks passed |
| Physical GPIO, real OLED and hardware wake | Not tested | Not tested |

The earlier comparison wrapper classified Unity's positive exit status 18 as SIGCONT and appended a synthetic error case to its JUnit output. The final baseline job independently built the same fixture and executed the native program directly through the existing isolation wrapper. It required the real positive exit code to equal the count of failed assertions (18), all 21 named cases to be present, no skipped cases, no crash/sanitizer diagnostics and clean isolated state. No candidate pass/fail requirement was removed or weakened.

Candidate source/build/native evidence: https://github.com/Amoulier/meshtastic-superbase-firmware/actions/runs/33980553791

Final independent baseline and evidence reconciliation, both successful: https://github.com/Amoulier/meshtastic-superbase-firmware/actions/runs/33981068477

The final verification checked all three successful candidate-native jobs, source SHA, all 24 expected suite names, nonempty JUnit cases, state isolation, firmware metadata and repeated package validation. Failures in preliminary audit attempts remain visible in history.

## Binary checks

The fresh `muzi-base` build passed. OTA/UF2 application bytes, CRC, manifest hashes, SoftDevice requirement, UF2 family/blocks/vectors and protected flash boundaries passed validation.

Application: 754,152 bytes, ending at `0xde1e8`, leaving 48,664 bytes before the protected warm-store boundary. Static RAM reported by the build: 98,332 bytes; this is not a runtime free-heap measurement.

The downloaded ELF and OTA vectors were also compared for GPIOTE, RTC1, SWI2_EGU2, USBD and POWER_CLOCK: each candidate vector resolves to its strong linked handler. No out-of-line `__atomic_*` helper symbols are present. The GPIOTE handler bytes are identical between the withdrawn OTA and the candidate, so its removal by the linker is not supported as the explanation for the reported failure.

| File | SHA-256 |
| --- | --- |
| `firmware-muzi-base-2.8.0.cc704b8-ota.zip` | `179dcfe06bf54f8fa9e5d8904a3b27bec96bb7dad341a4198f98d6925e86b445` |
| `firmware-muzi-base-2.8.0.cc704b8.uf2` | `ea8b1099f4d417f74fa530234fce88f52ae1347808bca96fb512c503cd83bcd9` |

An additional standalone state-machine run exercised 1,770 simulated timing scenarios under AddressSanitizer/UndefinedBehaviorSanitizer. Those scenarios are separate from the 614 native test cases and are not physical hardware tests.

## Scope and validation still needed

Board mappings, bootloader path, persisted configuration and identity handling were not changed by the navigation correction. Existing MQTT ACK, radio/BLE/GPS, DMs Only buzzer, RTTTL and IMU behavior remain preserved by source and regression checks. No battery-life or long-duration RF claims are made.

Back up configuration before installing the candidate. It does not require a factory reset or pin overrides. OTA uses the `*-ota.zip` without extraction and requires the MuziWorks OTAFIX bootloader; USB uses `.uf2` on the bootloader drive.

On the real unit, confirm left/right screen changes, SELECT, menu up/down, cancel/back, short/held presses and wake behavior. Simulated GPIO and an in-memory display do not establish electrical interrupt delivery or operation of the real OLED. The exact cause of the user's complete hardware symptom remains unconfirmed until that check.

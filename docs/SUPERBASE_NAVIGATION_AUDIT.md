# Superbase navigation correction (candidate)

The user reported that the five-way navigation buttons did not change BaseUI after installing
`v2.8.0-superbase.7` (`7c39922e028a68dbe3af7470aea8eb8d849fc569`). The release and its assets
were withdrawn; its tag is retained only for history, not as a supported installable release.

## Findings

The old TrackballInterruptBase required an IRQ action AND a still-low direction GPIO at poll
time. A released tap was discarded even though its falling edge had been recorded. A switch
with no delivered IRQ could never start navigation, including SELECT. attachInterrupt failure
was unchecked on nRF52. All direction inputs also shared a debounce clock, and the direction
hold state depended on a nonzero timestamp. These are independently reproducible defects;
without device logs, they are not proof of which condition caused this user's complete failure.

The five switches now use a Superbase-scoped mechanical-button path. Per-button IRQ sequence
counters retain released taps without a read/clear race. Debounced GPIO polling also detects a
press when an IRQ is not delivered. Repeat/release use unsigned elapsed durations, work at
clock zero and rollover, and SELECT does not turn a completed hold into a short click. nRF52
interrupt allocation failure is logged rather than silent. The IRQ wakes the cooperative loop.
Idle polling remains 50 ms; 10 ms polling is used only while handling a switch.

No remapping of board pins, no configuration reset, and no changes to radio, BLE, GPS, MQTT,
notification policy, identity, bootloader, protocol or dependency versions are made here.

## Verification contract

The new suite uses simulated GPIO levels with the production TrackballInterruptBase,
InputBroker, Screen::handleInputEvent, menu renderer and OLEDDisplayUi. Only physical display
I/O and initial UI frame setup are replaced with a memory display. It covers released taps,
missing IRQs, debounce, repeats, wrap, SELECT, frame movement, menu movement/cancel, and the
wake-event consumption rule. It does not test real electrical signals, nRF52 IRQ delivery,
BLE, power transitions or the physical OLED. The previous trackball-press suite is now mandatory.

Compare the new suite against the withdrawn source and against the correction. Run all other
Superbase regression suites, the exact-source muzi-base build and existing package audit.
No replacement final release is authorized by this diagnostic workflow. A candidate binary
must still be checked on the user's device for all directions, SELECT, back/cancel and wake.

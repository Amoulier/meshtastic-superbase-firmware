# Agent instructions

Read `.github/copilot-instructions.md` top-to-bottom before non-trivial changes. It is the canonical
source for conventions, NodeDB layout, encryption, tests and hardware-operation rules.

## Scope

Only MuziWorks Superbase (`muzi-base`, nRF52840) is maintained. The Linux environment
`superbase-native-tests` exists solely for regression tests. Preserve the custom MQTT implicit ACK,
DMs Only buzzer, RTTTL ownership, runtime Bluetooth, navigation and power behavior when integrating upstream.
Do not restore removed board targets, packaging or workflows.

## Commands

| Action                               | Command                                           |
| ------------------------------------ | ------------------------------------------------- |
| Initialize dependencies              | `git submodule update --init --recursive`         |
| Build                                | `pio run -e muzi-base`                            |
| Selected CI regression gate (Linux)  | `python3 bin/test-superbase.py`                   |
| All discovered native suites (Linux) | `./bin/run-tests.sh`                              |
| BLE lifecycle regression             | `python3 bin/test-nrf52-bluetooth.py`             |
| Source preservation audit            | `python3 bin/audit-superbase-release.py --source` |
| Docker regression gate               | `./bin/test-native-docker.sh`                     |
| Format before commit                 | `trunk fmt`                                       |

On Windows use WSL2 or Docker for native tests. The full runner exits 0 GREEN, 1 RED,
2 AMBER, 3 FILTERED; a subset is not a full-suite gate. Native ASan/LSan and coverage are enabled
in `superbase-native-tests`; do not use the removed `native` or `coverage` environment names.
CI is `.github/workflows/superbase_ci.yml` plus the reusable `build_firmware.yml`.

## Rules

- Do not edit `src/mesh/generated/`. Protocol changes start in meshtastic/protobufs upstream.
- Use `Throttle` for deadlines and elapsed time. Test inactive sentinels before deadline comparisons.
- Use NodeDB copy-out satellite accessors and bitfield helpers; do not return pointers into satellite maps.
- Keep comments to one or two lines explaining non-obvious reasons.
- Keep investigation notes and feature/design documentation out of the tree. Use PR descriptions;
  upstream manuals belong in meshtastic/meshtastic. Existing `docs/` records are historical, not current release approval.
- No destructive device operation or history-rewriting Git operation without explicit operator authorization.
- Never factory-reset to fix a routine update: a full reset rotates identity keys and breaks peer relationships.
- Keep one serial connection per port. Close USB API clients before checking Android message reception.
- Back up configuration and identity before an authorized flash; do not expose private keys or channel PSKs.
- `userPrefs.jsonc` is test session state; the hardware harness snapshots/restores it. Do not edit it inside tests.
- Source hashes are review guards. Update them only after reviewing the changed paths, never to bypass a failure.
- State unknown causes as unknown. Automated success does not establish physical navigation, RF or battery runtime.

## Hardware harness

Tools and `/test`, `/diagnose`, `/repro`, `/leakhunt` workflows live in
[meshtastic/meshtastic-mcp](https://github.com/meshtastic/meshtastic-mcp).
Set `MESHTASTIC_FIRMWARE_ROOT` to this checkout and `MESHTASTIC_MCP_ENV_NRF52=muzi-base`.
Discover tools in the active session; `.mcp.json` does not guarantee the host loaded them.
The Meshtastic CLI is a fallback when MCP is unavailable, with the same authorization rules.

## Helpers

| Purpose                        | Location                                   |
| ------------------------------ | ------------------------------------------ |
| Utilities                      | `src/meshUtils.h`                          |
| Logging                        | `src/DebugConfiguration.h`                 |
| Timing                         | `src/mesh/Throttle.h`, `src/UptimeClock.h` |
| Module base                    | `src/mesh/ProtobufModule.h`                |
| Events                         | `src/Observer.h`                           |
| Current release and validation | GitHub releases, linked from `README.md`   |

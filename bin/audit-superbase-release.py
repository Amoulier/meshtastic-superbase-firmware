#!/usr/bin/env python3
"""Fail closed on Superbase source-scope or install-package mismatches."""

import argparse
import binascii
import hashlib
import json
import struct
import subprocess
import zipfile
from pathlib import Path

UPSTREAM_BASE = "2d61ca6736cfcc59ee14cf7e2618779f45539038"
PREVIOUS_RELEASE = "90410fb0992adbabe6d9973778f665c232e351fd"
REVIEWED_CUSTOM_SOURCES = {
    "src/AudioThread.h": "85265842d9face2a87cd38200629b25b8ab7b5e41efa3729dd18d88a8f338d84",
    "src/Power.cpp": "fd563fa69946a054043b4d5954295a63decf7e137f02eddda4159973cb3b4c5b",
    "src/PowerFSM.cpp": "7ae408b01ee84e145da3a7ed3ac831bc67edc1e870f4c39fb9c957074228989b",
    "src/PowerStatus.h": "2cdb841ebeb3398a59a95f65a94ca10232fd0193f33b6a0f356a13d45f167916",
    "src/buzz/BuzzerFeedbackThread.cpp": "a43eb0a14b783fa49e3fb1f1d2a609da0c4a67201ff0ff55626c40b085598e60",
    "src/buzz/BuzzerMode.h": "e0990949e068614493c6a03545f53dbce05961967d3717f07930dd9c1a3007f5",
    "src/buzz/buzz.cpp": "b4bebc95b728967cef818d545fc5387e6542554b479a5b46902c84704c35285c",
    "src/concurrency/OSThread.cpp": "db5acd384a3bf8abeb8fc8fc1fbcf3a0c8e15908b18d066329c2b0ef0b5e3d0a",
    "src/graphics/Screen.cpp": "b6072188ed6884b856e138a2e0abd8f47bc29e7aa9e015b939a21fb71fe80854",
    "src/graphics/Screen.h": "1e815e3cfbb656dc277a74accdaebe11115ead9dccec109aa75be8b033dedc83",
    "src/graphics/SharedUIDisplay.cpp": "b114f841119abdfdcb31f3f8f69bb27115e6b685f4c59e7ea8fa8ce01241437a",
    "src/graphics/draw/NotificationRenderer.cpp": "69a979e87b15f4c253fb5ce3c60eafc3666a3addb981d3248147d6ed289522bf",
    "src/input/SuperbaseButtonState.h": "7312a3d5e8e22e620ea7685783cd60c2b841469c3cd491a741d64f0b9ab678a6",
    "src/input/TrackballInterruptBase.cpp": "33f6a8031613d90954ea5877cfd3e67d3a367230e332a6e911575843f2272746",
    "src/input/TrackballInterruptBase.h": "0b9274591a15cad413cbff680e4ecf646e0016acaa4ee22d079eb2fd58c98d87",
    "src/input/TrackballInterruptImpl1.cpp": "f1a7272e3bcb86c9f44ace8684e984bdfd1eee8dbf46c01e6236ddeb5d47bfd7",
    "src/mesh/Channels.cpp": "1c8693785f4147338c5b3f65dfbd6f95f1c3e32c3919436ef1147843e8ebf255",
    "src/mesh/CryptoEngine.cpp": "1a85b85a360007a5089cf72b81c5d1b3f7cd781c35481cccc41d98530d63185c",
    "src/mesh/Default.h": "91a5f3aaa4cf3bd62d375053c24c283d42f344186974e43d2ae91564150f2d97",
    "src/mesh/LR11x0Interface.cpp": "5e656964a04081d227b05f6305c0f84ba7abe92f77c0094b9f9147474392f7a6",
    "src/mesh/NextHopRouter.cpp": "6e200fc0e75515fac623896f48552fbfe43128049f09c72c81e2c8fcd646f6a4",
    "src/mesh/NextHopRouter.h": "434409f03d4ea8e8d2c6feaaa21c3d3d5494f69f536a35e4e6c1b4e675bd6685",
    "src/mesh/NodeDB.cpp": "13edf25c5137c32fd7ccbfdc834eb7880727016ef6fc6b300e2e1cb466ae787a",
    "src/mesh/PhoneAPI.cpp": "3f99575b8e1b099f5e17c9b18b48d8b947d3295223f18c60ea2740e141be671f",
    "src/mesh/RadioLibInterface.cpp": "2794303973e74de2af7f94fe4cd3c67f5116ae82ae21247d889af2ada6460c6d",
    "src/mesh/ReliableRouter.cpp": "9ca9376fa4748864881d5c989e3653900d6eca9ffbba7c2f944fbc3548e02e34",
    "src/mesh/SX126xInterface.cpp": "161a4401cc6c4f2116e199d48ffd74d73f6c15df5f6a0d4198ed23e4f68d3797",
    "src/modules/ExternalNotificationModule.cpp": "8b85df1c032f776aa598587a8a20f845ab3bb2c2a7ebfab6c1858c7540bc40dc",
    "src/modules/ExternalNotificationModule.h": "e759e758e2edae213789d3e582e1183ebe1b5957b7749a95c733c11dd6446e6e",
    "src/modules/NeighborInfoModule.cpp": "d674eac1a7cceb64b0e5b2e9511259a57764bd5e1eb3c6b0b4cd0cbd5426d865",
    "src/modules/NodeInfoModule.cpp": "38c679d5bca13b2c3a6fc35b8c80942228159d080e79537ffc17dabc2b0582b4",
    "src/modules/NodeInfoModule.h": "de075ca3f54d56ee0804edabd0970a1bb74dd1faf9e27ee60f8eb1168951118b",
    "src/modules/PositionModule.cpp": "2d72a836c0aeabbe874c885a55ce160c3ae4964a944ed8081e3535f61d739a7c",
    "src/modules/PositionModule.h": "a4590d5eba3c1ebacb7297240d5a8b0e86aa5c8dced287752607ea111d43edcb",
    "src/modules/SystemCommandsModule.cpp": "b2ba085aa4f3143bd35ddbe69e13f8d8a199779a264c2e55aea63acd3cbb76e3",
    "src/modules/Telemetry/DeviceTelemetry.h": "a987fbbed4e8eeea77b34416cd68bfd0ae1e036044975f7e8570a3da2443a4c7",
    "src/motion/ICM20948Sensor.cpp": "b7ff502fbbef31bc7159e5cab5da4e981751f1c1567a03c5ed1345903384eee8",
    "src/motion/MotionSensor.h": "4b7c83bd45ab602a8a67e717db01d96a5fbfedcc4f116d7cbe69ead0b1024128",
    "src/mqtt/MQTT.cpp": "44c720646feb21c867f015f8c7782baaeebb312ebd6287512ff1fe1164fa4372",
    "src/platform/nrf52/NRF52Bluetooth.cpp": "87f7c7730433212f14c8aa8b266869395fc6f9dd2b9fcdb78d0e3ec91418c55d",
    "src/platform/nrf52/NRF52Bluetooth.h": "961f79f3a325b1ddc08b7f5349a234ea6675d666c664b42f6559ed4645fe96c3",
    "src/platform/nrf52/main-nrf52.cpp": "d9872473b9e5af72bcb8c2155bc90cbf063d9751c70661ebe2494937ef562126",
    "src/power/BQ25185Status.h": "210f4f972b53643e6b051c5de0e0e05d575dedec4332fd8b85974fbbaccad25b",
    "src/power/ChargeFault.h": "f92a787ba5cb53e97d88f84a335edb403daddf358bbe16e6fa29b952a0cf7ffc",
    "test/README.md": "c5da136fe39d60709aa4a589a06af0cd4199c99c5be6629e120fb457321b7728",
    "test/fixtures/nrf52_bluetooth_lifecycle.cpp": "823a6975e43afa440cc88b9c41858c0ed781cad9c7a1bee6426980e12c6fb7f1",
    "test/fixtures/nrf52_flash_quiesce.cpp": "88ac170b22cff33abf7140134af7b34d943fa03e570003f0c3efc456f964e501",
    "test/state-manifest.tsv": "5f5990146f3d35ea86cbc5e190851d0333b74a7189e526e98229ccf459d7bf0b",
    "test/test_admin_radio/test_main.cpp": "55b6d03751efa136e8afd706971c1a4739f2ba59d3965b40fb5bb13bb8afa847",
    "test/test_buzzer_mode/test_main.cpp": "f9f75aab8989ec256616b92a3759a73ef49f25a8b27f232c46431589c03888e2",
    "test/test_gps_update_scheduling/test_main.cpp": "1353e42c5017e142b339326d046fc6be5c142ddb89c334bdd76fa5fa74501c3b",
    "test/test_mesh_module/test_main.cpp": "9a8bff8a05e59da0c9196b1d010380e000098bd5fdc65973a39fb70c83abb739",
    "test/test_mqtt/MQTT.cpp": "ca2d31f1fea8774f001b25b1521c62928f82c868d8f8fdebdef8a795ef9a5ef0",
    "test/test_muted_source/test_main.cpp": "cfe7a16b0fec75b3a66f19f31d41a5a097f71a7c91f0a865e61d11a585b2cf71",
    "test/test_nodedb_save_retry/test_main.cpp": "52ff74dc833988db140afed4bcc540f82ceb1eb77f14a8f1500085b058fe90ea",
    "test/test_phone_api_config_dump/test_main.cpp": "d42b69af97bd732ce8f267b57514040bdc0e3931d06c35b3c5a4b172ef8884d1",
    "test/test_power_status/test_main.cpp": "705d27bcf6ec277b7d1ba709822724f992756f4fd67f7ab6a6c2328b46dee29e",
    "test/test_radio/test_main.cpp": "890fa74a0ea41bee8157fcd77fd4426798e4939c58c32f36b808da5b3bb41190",
    "test/test_reliable_ack_matrix/test_main.cpp": "708b93c0682521c82b440af5579f9b9e1eba2cc128b2259fb9347342c9cb1f66",
    "test/test_superbase_broadcasts/test_main.cpp": "5c42287b0bf88133467686a347b8e37aa46ff0c8a7381b0d55f6c89c0ca2f107",
    "test/test_superbase_navigation/test_main.cpp": "2bb6baf8ce1e1ffcf9fec74368a95f9c10c78f5801a11b2f76c39df349eb583b",
    "test/test_superbase_radio_recovery/test_main.cpp": "b90bd42a17b09e2f32229dd9aa74c61e69caefb2c67892b5fa66508677ba423d",
}
REVIEWED_BUILD_SOURCES = {
    "platformio.ini": "04f7ba519880fe3dc6ce8f30b5d80a5efe0f7bb875219eecb666eda0d74942dd",
    "version.properties": "c654658fd0a5f241bbcbf601d8f3f01ca30d746bc7e56011e70fdc14ee1bf8e4",
    ".github/actions/setup-base/action.yml": "5b1a43c1e41c6033de219a171f72e6bac4d4df85c3248c56775a2776ed889656",
    ".github/actions/setup-base/requirements.txt": "5b18431054b330b84dcaae35d6e330ad2d9c83609b860d704d65088000743c76",
    ".github/actions/setup-native/action.yml": "5b3c78e1817fa436dd895d8e58cace007879d89f350a32d3db16de9d27ea369b",
    ".github/workflows/build_firmware.yml": "b018de12ff7d458fc8792975a08a0377bf8ca38b1ae6f770bfa7c8c908462346",
    ".github/workflows/superbase_ci.yml": "6da8a30ad9e159973c9a8cb55e612526705272dcd4f4d2e18c3a3267c2e14dc5",
    "boards/muzi-base.json": "443d6b647b6cf44d097420c203e21f92d30efcccad38c855caca8e1bf88be18e",
    "extra_scripts/README.md": "f0c94a6840e3fbc969c3d80b987529e72d0845028e17a9db09be5b216dca5d18",
    "extra_scripts/disable_adafruit_usb.py": "ac256f8c537d1b9d32c0d69360ee8228be1d2ef831308c0101ab982b14de9f7d",
    "extra_scripts/ld_response_file.py": "bee98fcc3449775cc7863ba2990ed45d95209c8b1de6b5beec337aadb5408af4",
    "extra_scripts/nrf52_extra.py": "eeca3aeb91fae9eb6b4ded408d107a129d1f10f61c9109fd60fee6e96165cc42",
    "extra_scripts/nrf52_lto.py": "03b7308e0a4dfd52fceb70852e5d153f16ebc696f0470754e04c16022f1913ed",
    "extra_scripts/nrf52_warm_region.py": "80955ed531900217e5f23d58fb229b4366b12bfab64e7dabf6cbe3e2c6415187",
    "variants/native/portduino.ini": "0cb35d6fc1af6a6a3a79c9c06e66b6bed6c0f94940ab3c29030bfdcf7cda8391",
    "variants/native/portduino/HUB75Native.h": "0bea9d60b709736b755c288f64851f720ece480524faa2a2cd404f204e0ba62c",
    "variants/native/portduino/malloc.h": "d70a54ef5c23b8102bf7cc2063eeae211b1b58772242f79126bca109f665aa11",
    "variants/native/portduino/variant.h": "3e4bbe6362e2e1a12321930b5d29018b63b3bc570cda2a2ee52f270abe5c8839",
    "variants/nrf52840/cpp_overrides/lfs_util.h": "91ded470fa175617e3248d122d8eef9fecbc19d3a181f36ebb566d2952925330",
    "variants/nrf52840/muzi_base/platformio.ini": "32b7c8ba8efce0e73cdea5cee34f2fbf5ea39f4be7b20000f0cb9d1cfd25b5ab",
    "variants/nrf52840/muzi_base/rfswitch.h": "b6874c44d64a0575eaf90292ff64e8a90fad5b1ca885e74b6b9888c1a3f33a01",
    "variants/nrf52840/muzi_base/variant.cpp": "e77169c30aa386d485a457c422a9c9050c4940c9c2e159f7ba74d4d76175dc37",
    "variants/nrf52840/muzi_base/variant.h": "33533baa3f90b14891d1e271545be598b16f0a54cda95a2f2e9eba768b237c5e",
    "variants/nrf52840/nrf52.ini": "d6a5ddd30a5849ff453e06c7cb25a3c66bed0b2119576f162cab28b5bec18914",
    "variants/nrf52840/nrf52840.ini": "7c7d53dea735bc4c68f0a5a3360634c9e64a7b5c2123d5fb8eae983c9e8f0c2f",
}
REMOVED_TARGET_PATHS = [
    "src/platform/nrf54/",
    "src/platform/esp32/",
    "src/platform/extra_variants/",
    "src/platform/nrf54l15/",
    "src/platform/rp2xx0/",
    "src/platform/stm32wl/",
    "src/modules/esp32/",
    "bin/config.d/",
    ".github/actions/build-variant/",
    ".github/ISSUE_TEMPLATE/New Board.yml",
    ".github/prompts/new-variant.prompt.md",
    "src/input/TDeckProKeyboard.cpp",
    "src/input/TDeckProKeyboard.h",
    "src/input/TLoraPagerKeyboard.cpp",
    "src/input/TLoraPagerKeyboard.h",
    "src/mesh/STM32WLE5JCInterface.cpp",
    "src/mesh/STM32WLE5JCInterface.h",
    "extra_scripts/esp32_extra.py",
    "extra_scripts/esp32_pre.py",
    "extra_scripts/stm32_extra.py",
    "extra_scripts/nrf54l15_linker.py",
    "extra_scripts/wasm_link_flags.py",
    "extra_scripts/windows_link_flags.py",
    "bin/build-esp32.sh",
    "bin/build-rp2xx0.sh",
    "bin/build-stm32wl.sh",
    "bin/lilygo_techo_bootloader-0.6.1.zip",
    "bin/update-lilygo_techo_bootloader-0.6.1_nosd.uf2",
    "bin/wio_tracker_bootloader_update.bin",
    "bin/setup-python-for-esp-debug.sh",
    "bin/eth-ota-upload.py",
    "bin/genpartitions.py",
    "bin/device-install.bat",
    "bin/device-install.sh",
    "bin/device-install_test.ps1",
    "bin/device-update.bat",
    "bin/device-update.sh",
    "Dockerfile",
    "docker-compose.yml",
    ".env.example",
    "bin/config-dist.yaml",
    "bin/99-meshtasticd-udev.rules",
    "bin/meshtasticd-start.sh",
    "bin/meshtasticd.service",
    "bin/native-install.sh",
    "bin/build-winget-package.ps1",
    "bin/org.meshtastic.meshtasticd.desktop",
    "bin/org.meshtastic.meshtasticd.metainfo.xml",
    "bin/org.meshtastic.meshtasticd.svg",
    "debian/",
    "packaging/",
    "zephyr/",
    "bin/bump_metainfo/",
    "alpine.Dockerfile",
    "meshtasticd.spec.rpkg",
    "rpkg.conf",
    "bin/rpkg.macros",
    "scripts/add_mbedtls_sources.py",
    "default_16MB.csv",
    "default_8MB.csv",
    "partition-table-8MB.csv",
    "partition-table-t3s3.csv",
    "partition-table.csv",
    "bin/build-native.sh",
    "bin/native-run.sh",
]


def git(*args):
    return subprocess.check_output(["git", *args], text=True).strip()


def source_audit():
    for directory, expected in {
        "boards": {"muzi-base.json"},
        "variants": {"native", "nrf52840"},
        "variants/nrf52840": {
            "cpp_overrides",
            "muzi_base",
            "nrf52.ini",
            "nrf52840.ini",
        },
        ".github/workflows": {"superbase_ci.yml", "build_firmware.yml"},
    }.items():
        assert {p.name for p in Path(directory).iterdir()} == expected, directory
    preserved = [
        "boards",
        "variants",
        "protobufs",
        "src/mesh/generated",
        "platformio.ini",
        "src/motion/ICM20948Sensor.cpp",
        "src/mesh/ReliableRouter.cpp",
        "src/mesh/Router.cpp",
        "src/mqtt/MQTT.cpp",
        "src/AudioThread.h",
        "extra_scripts",
        "version.properties",
        "src/mesh/RadioInterface.cpp",
        "src/mesh/RF95Interface.cpp",
        "src/mesh/LR20x0Interface.cpp",
        "src/mesh/SX128xInterface.cpp",
    ]
    assert all(
        Path(path).exists() for path in preserved
    ), "Missing preserved source path"
    removed_specs = [":(exclude)" + path.rstrip("/") for path in REMOVED_TARGET_PATHS]
    reintroduced = [
        path
        for path in REMOVED_TARGET_PATHS
        if Path(path).is_file()
        or (Path(path).is_dir() and any(p.is_file() for p in Path(path).rglob("*")))
    ]
    assert not reintroduced, f"Unsupported target files reintroduced: {reintroduced}"
    platforms = {
        p.name
        for p in Path("src/platform").iterdir()
        if p.is_dir() and any(f.is_file() for f in p.rglob("*"))
    }
    assert platforms == {
        "nrf52",
        "portduino",
    }, f"Unsupported platform sources: {platforms}"
    subprocess.run(
        ["git", "merge-base", "--is-ancestor", UPSTREAM_BASE, "HEAD"], check=True
    )
    subprocess.run(
        ["git", "merge-base", "--is-ancestor", PREVIOUS_RELEASE, "HEAD"], check=True
    )
    assert not git(
        "diff", UPSTREAM_BASE, "--", "protobufs", "src/mesh/generated"
    ), "Protocol differs from upstream"
    changed = set(
        git(
            "diff", "--name-only", UPSTREAM_BASE, "--", "src", "test", *removed_specs
        ).splitlines()
    )
    assert changed == set(
        REVIEWED_CUSTOM_SOURCES
    ), f"Unreviewed custom delta: {changed ^ set(REVIEWED_CUSTOM_SOURCES)}"
    for path, expected_hash in {
        **REVIEWED_CUSTOM_SOURCES,
        **REVIEWED_BUILD_SOURCES,
    }.items():
        actual_hash = hashlib.sha256(
            Path(path).read_text(encoding="utf-8").encode("utf-8")
        ).hexdigest()
        assert actual_hash == expected_hash, f"Reviewed integration changed: {path}"
    required = {
        "src/mesh/Default.h": [
            "#define default_position_stationary_broadcast_secs (12 * 60 * 60)"
        ],
        "src/mesh/ReliableRouter.cpp": ["mqttImplicitAckSeen = true"],
        "src/mesh/NextHopRouter.cpp": ["if (p.mqttImplicitAckSeen)"],
        "src/mqtt/MQTT.cpp": ["rememberMqttAck", "mqttAckHistoryLock"],
        "src/modules/ExternalNotificationModule.cpp": [
            "buzzerModeAllowsNotification",
            "buzzerPlaybackBackend",
            "genericAlertActive",
            "vibraAlertActive",
        ],
        "src/AudioThread.h": ["RtttlOwner", "rtttlLock"],
        "src/modules/SystemCommandsModule.cpp": [
            "setBluetoothEnable(config.bluetooth.enabled)"
        ],
        "src/platform/nrf52/NRF52Bluetooth.cpp": [
            "blebas.notify",
            "!initialized || !config.bluetooth.enabled",
        ],
        "src/Power.cpp": ["decodeBQ25185ChargeFault", "decodeBQ25185Status"],
        "src/graphics/Screen.cpp": ["chargeFaultMessage"],
        "src/motion/ICM20948Sensor.cpp": [
            "AK09916_mode_power_down",
            "AK09916_mode_cont_100hz",
        ],
        "src/mesh/RadioLibInterface.cpp": [
            "chipRecoveryFailures < MAX_CHIP_RECOVERY_FAILURES",
            "if (isSending())",
        ],
    }
    for path, markers in required.items():
        text = Path(path).read_text()
        assert all(
            marker in text for marker in markers
        ), f"Custom behavior missing: {path}"
    for backend in ["SX126x", "LR11x0"]:
        text = Path(f"src/mesh/{backend}Interface.cpp").read_text()
        pos = text.index("RX offline for periodic retry")
        assert "rxOffline = true;" in text[pos : pos + 180], backend
        pos = text.index(f"bool {backend}Interface<T>::reconfigure()")
        assert "return !rxOffline;" in text[pos : text.index("\n}", pos)]
    assert (
        "uses: actions/checkout"
        not in Path(".github/actions/setup-base/action.yml").read_text()
    ), "Nested checkout regression"
    subprocess.run(["git", "diff", "--check", UPSTREAM_BASE, "HEAD"], check=True)
    conflict = subprocess.run(
        ["git", "grep", "-n", "-E", "^(<<<<<<<|>>>>>>>)", "--", ":!*.md"],
        capture_output=True,
        text=True,
    )
    assert conflict.returncode == 1, conflict.stdout
    return {
        "source_sha": git("rev-parse", "HEAD"),
        "upstream_base": UPSTREAM_BASE,
        "previous_release": PREVIOUS_RELEASE,
        "scope": "muzi-base only",
        "source_audit": "PASS",
        "integration": "Full upstream ancestry with reviewed Superbase custom delta",
        "reviewed_custom_sources": REVIEWED_CUSTOM_SOURCES,
        "reviewed_build_sources": REVIEWED_BUILD_SOURCES,
        "removed_unsupported_paths": REMOVED_TARGET_PATHS,
    }


def package_audit(directory, sha):
    root = Path(directory)

    def one(pattern):
        paths = list(root.glob(pattern))
        assert len(paths) == 1, (pattern, paths)
        return paths[0]

    manifest_path = one("firmware-muzi-base-*.mt.json")
    meta = json.loads(manifest_path.read_text())
    assert meta["version"] == "2.8.2." + sha[:7], meta["version"]
    assert (
        meta["platformioTarget"] == "muzi-base"
        and meta["mcu"] == "nrf52840"
        and meta["hwModel"] == 93
    )
    assert (
        meta["architecture"] == "nrf52840"
        and meta["repo"] == "Amoulier/meshtastic-superbase-firmware"
    )
    ota = one("firmware-muzi-base-*-ota.zip")
    uf2 = one("firmware-muzi-base-*.uf2")
    for p in [ota, uf2]:
        entry = next(f for f in meta["files"] if f["name"] == p.name)
        assert entry["bytes"] == p.stat().st_size
        assert entry["md5"] == hashlib.md5(p.read_bytes()).hexdigest()
    with zipfile.ZipFile(ota) as z:
        assert z.testzip() is None and len(z.namelist()) == len(set(z.namelist())) == 3
        app = json.loads(z.read("manifest.json"))["manifest"]["application"]
        data = z.read(app["bin_file"])
        init = z.read(app["dat_file"])
        assert len(init) == 14
        device, revision, version, count, sd, crc = struct.unpack("<HHIHHH", init)
        assert (device, revision, version, count, sd) == (82, 65535, 4294967295, 1, 182)
        assert binascii.crc_hqx(data, 0xFFFF) == crc
        assert app["init_packet_data"]["firmware_crc16"] == crc
        assert app["init_packet_data"]["softdevice_req"] == [182]
    raw = uf2.read_bytes()
    assert len(raw) % 512 == 0 and raw
    nblocks = len(raw) // 512
    image = {}
    for block in range(nblocks):
        b = raw[block * 512 : (block + 1) * 512]
        m0, m1, flags, addr, size, index, total, family = struct.unpack("<8I", b[:32])
        assert (m0, m1) == (0x0A324655, 0x9E5D5157)
        assert struct.unpack("<I", b[508:])[0] == 0x0AB16F30
        assert flags == 0x2000 and family == 0xADA52840
        assert size == 256 and index == block and total == nblocks
        assert 0x26000 <= addr and addr + size <= 0xEA000, hex(addr)
        assert addr not in image, hex(addr)
        image[addr] = b[32 : 32 + size]
    addresses = sorted(image)
    assert addresses == list(
        range(0x26000, 0x26000 + 256 * nblocks, 256)
    ), "Noncontiguous UF2"
    payload = b"".join(image[a] for a in addresses)
    assert payload[: len(data)] == data, "OTA and UF2 do not encode the same image"
    assert len(payload) - len(data) < 256, "Unexpected UF2 padding"
    assert 0x26000 + len(data) <= 0xEA000
    stack, reset = struct.unpack("<II", data[:8])
    assert (
        0x20000000 <= stack <= 0x20040000
        and reset & 1
        and 0x26000 <= (reset & ~1) < 0x26000 + len(data)
    )
    return {
        "package_audit": "PASS",
        "firmware_version": meta["version"],
        "ram_bytes": meta["ram_bytes"],
        "flash_bytes": meta["flash_bytes"],
        "application_bytes": len(data),
        "application_end": hex(0x26000 + len(data)),
        "warm_store_clear_bytes": 0xEA000 - (0x26000 + len(data)),
        "uf2_blocks": nblocks,
        "sha256": {
            p.name: hashlib.sha256(p.read_bytes()).hexdigest()
            for p in [manifest_path, ota, uf2]
        },
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--packages")
    parser.add_argument("--sha")
    parser.add_argument("--source", action="store_true")
    parser.add_argument("--output")
    args = parser.parse_args()
    assert args.source or args.packages
    result = {}
    if args.source:
        result.update(source_audit())
    if args.packages:
        assert args.sha and len(args.sha) == 40
        result.update(package_audit(args.packages, args.sha))
    result["hardware_tested"] = False
    text = json.dumps(result, indent=2) + "\n"
    print(text)
    if args.output:
        Path(args.output).parent.mkdir(parents=True, exist_ok=True)
        Path(args.output).write_text(text)


if __name__ == "__main__":
    main()

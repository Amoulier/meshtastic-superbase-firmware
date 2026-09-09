#!/usr/bin/env python3
"""Fail closed on Superbase source-scope or install-package mismatches."""
import argparse
import binascii
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import zipfile

BASE = '4cbba7006a9fed23cb1a778b7e62422ba96ee8bc'
INTEGRATION_BASE = 'c23e1d46ebda005cb044470977d11bb57e560195'
POSITION_FIX = '14be478b866a21d9a4c5cf419a9a70d78e4703bb'
REVIEWED_UPSTREAM_COMMITS = [
    '73c41105282e3f6245714efbaad2e061f7bfa821',
    '382980637b1a91cb9f493c8e62b682f5981aa166',
    '42d32fcea60f2804f2fb143276e383f0fce522c8',
]
REVIEWED_UPSTREAM_SOURCES = {
    'src/mesh/MeshService.cpp': '3f3b41cfc68321ea3aeefb0ca7a1bbf02a00c3082c5f039bc03e869c120861d2',
    'src/mesh/MeshService.h': '009840d4a5a54d7359aeb59482b1bd4571f2ae1653c0f568ce1ef6f071d4d7fb',
    'src/mesh/NodeDB.cpp': 'ce9b4b9a8285f7e582ff20a1743c58afb57c16fbb7d9484ae18e3d3138f65d6c',
    'src/mesh/NodeDBLegacyMigration.cpp': 'a2f2fc29ad90285ca41addb0d6cd8c63bddf8c5f2ae7e97e0f989e4f4035e0f3',
    'src/mesh/PhoneAPI.cpp': '0565888591a013b3e4ad41094028216103eca95d1bf22ee4e4a524b2fac3dd50',
    'src/mesh/PhoneAPI.h': '11ed8f8fbbea9f4a39300a05ef28ab97abb00af0dc786471904cdeecf569775d',
    'src/mesh/RadioInterface.cpp': '236520cc6b86b83a50bc59129636a8b38b3d68d581131d7eccff6efbccea168d',
    'test/test_phone_api_config_dump/test_main.cpp': 'e1577346dd904ce04aab994c0d7b0c05d6e9ad90c50cb02d4f4b887551263c73',
}
REVIEWED_POWER_SOURCES = {
    'src/Power.cpp': '03a8b81a3b421afc5cea37c1675040944367cbc0e58c22637e8c98f135ba38c7',
    'src/PowerStatus.h': '4d60d6529d6892e9c1f4d1a93de7d0d9a1fc1f451d8b8d9cda79ec3524d28d94',
    'src/power/BQ25185Status.h': '15ee3d5d82cd25a4ecb33eb684052848ed1d874d8574751c5caeda8472e9d811',
    'variants/nrf52840/muzi_base/variant.h': 'ce0ff13922901680ee6dc6ace05caa15126418894e481eec9d16518a94392019',
    'variants/nrf52840/muzi_base/variant.cpp': 'e77169c30aa386d485a457c422a9c9050c4940c9c2e159f7ba74d4d76175dc37',
    'test/test_power_status/test_main.cpp': '26cdd6c5fcf117d03329521bf120bdbb2d62b07272207d9f9490bf2ef3c5dae3',
}
REVIEWED_RELEASE_SOURCES = {
    'src/mesh/RadioLibInterface.cpp': '49236f60fd390321e9904ebc296eca9473be97c8d1ced8f67f14feceea0322dd',
    'src/modules/ExternalNotificationModule.cpp': 'd6b3dc7dc6fa61253f5ccad43f830368443dd2f51d1b65498acf8b472ba7d2b7',
    'src/modules/ExternalNotificationModule.h': '305d1e4a23009d2bda684675db240b45359b49cf51f15b6b5a4657b1af3f9fac',
    'test/test_buzzer_mode/test_main.cpp': 'f9f75aab8989ec256616b92a3759a73ef49f25a8b27f232c46431589c03888e2',
    'test/test_superbase_radio_recovery/test_main.cpp': 'd4ac056cf75b3f0420112bd5dc817836b6b6344dc2140f72577b699d8fe19bf0',
    'src/mesh/Channels.cpp': '59cfad49b392ecc70b6d0d4c34b13fe49ddda36c826b94e2072f2a1b97f919f0',
    'test/test_muted_source/test_main.cpp': 'cfe7a16b0fec75b3a66f19f31d41a5a097f71a7c91f0a865e61d11a585b2cf71',
}
REMOVED_TARGET_PATHS = [
    'src/platform/esp32/',
    'src/platform/extra_variants/',
    'src/platform/nrf54l15/',
    'src/platform/rp2xx0/',
    'src/platform/stm32wl/',
    'src/modules/esp32/',
    'bin/config.d/',
    '.github/actions/build-variant/',
    '.github/ISSUE_TEMPLATE/New Board.yml',
    '.github/prompts/new-variant.prompt.md',
    'src/input/TDeckProKeyboard.cpp',
    'src/input/TDeckProKeyboard.h',
    'src/input/TLoraPagerKeyboard.cpp',
    'src/input/TLoraPagerKeyboard.h',
    'src/mesh/STM32WLE5JCInterface.cpp',
    'src/mesh/STM32WLE5JCInterface.h',
    'extra_scripts/esp32_extra.py',
    'extra_scripts/esp32_pre.py',
    'extra_scripts/stm32_extra.py',
    'extra_scripts/nrf54l15_linker.py',
    'extra_scripts/wasm_link_flags.py',
    'extra_scripts/windows_link_flags.py',
    'bin/build-esp32.sh',
    'bin/build-rp2xx0.sh',
    'bin/build-stm32wl.sh',
    'bin/lilygo_techo_bootloader-0.6.1.zip',
    'bin/update-lilygo_techo_bootloader-0.6.1_nosd.uf2',
    'bin/wio_tracker_bootloader_update.bin',
    'bin/setup-python-for-esp-debug.sh',
    'bin/eth-ota-upload.py',
    'bin/genpartitions.py',
    'bin/device-install.bat',
    'bin/device-install.sh',
    'bin/device-install_test.ps1',
    'bin/device-update.bat',
    'bin/device-update.sh',
    'Dockerfile',
    'docker-compose.yml',
    '.env.example',
    'bin/config-dist.yaml',
    'bin/99-meshtasticd-udev.rules',
    'bin/meshtasticd-start.sh',
    'bin/meshtasticd.service',
    'bin/native-install.sh',
    'bin/build-winget-package.ps1',
    'bin/org.meshtastic.meshtasticd.desktop',
    'bin/org.meshtastic.meshtasticd.metainfo.xml',
    'bin/org.meshtastic.meshtasticd.svg',
    'debian/',
    'packaging/',
    'zephyr/',
    'bin/bump_metainfo/',
    'alpine.Dockerfile',
    'meshtasticd.spec.rpkg',
    'rpkg.conf',
    'bin/rpkg.macros',
    'scripts/add_mbedtls_sources.py',
    'default_16MB.csv',
    'default_8MB.csv',
    'partition-table-8MB.csv',
    'partition-table-t3s3.csv',
    'partition-table.csv',
    'bin/build-native.sh',
    'bin/native-run.sh',
]


def git(*args):
    return subprocess.check_output(['git', *args], text=True).strip()


def source_audit():
    for directory, expected in {
        'boards': {'muzi-base.json'},
        'variants': {'native', 'nrf52840'},
        'variants/nrf52840': {'cpp_overrides', 'muzi_base', 'nrf52.ini', 'nrf52840.ini'},
        '.github/workflows': {'superbase_ci.yml', 'build_firmware.yml'},
    }.items():
        assert {p.name for p in Path(directory).iterdir()} == expected, directory
    preserved = ['boards', 'variants', 'protobufs', 'src/mesh/generated', 'platformio.ini',
                 'src/motion/ICM20948Sensor.cpp',
                 'src/mesh/ReliableRouter.cpp', 'src/mesh/Router.cpp', 'src/mqtt/MQTT.cpp',
                 'src/AudioThread.h',
                 'extra_scripts', 'version.properties', 'src/mesh/RadioInterface.cpp',
                 'src/mesh/RF95Interface.cpp', 'src/mesh/LR20x0Interface.cpp', 'src/mesh/SX128xInterface.cpp']
    assert all(Path(path).exists() for path in preserved), 'Missing preserved source path'
    removed_specs = [':(exclude)' + path.rstrip('/') for path in REMOVED_TARGET_PATHS]
    reintroduced = [path for path in REMOVED_TARGET_PATHS if Path(path).is_file() or
                    (Path(path).is_dir() and any(p.is_file() for p in Path(path).rglob('*')))]
    assert not reintroduced, f'Unsupported target files reintroduced: {reintroduced}'
    platforms = {p.name for p in Path('src/platform').iterdir() if p.is_dir() and any(f.is_file() for f in p.rglob('*'))}
    assert platforms == {'nrf52', 'portduino'}, f'Unsupported platform sources: {platforms}'
    reviewed_sources = {**REVIEWED_UPSTREAM_SOURCES, **REVIEWED_POWER_SOURCES, **REVIEWED_RELEASE_SOURCES}
    reviewed_specs = [':(exclude)' + path for path in reviewed_sources]
    assert not git('diff', BASE, '--', *preserved, ':(exclude)platformio.ini', *removed_specs, *reviewed_specs), 'Preserved source changed'
    for path, expected_hash in reviewed_sources.items():
        actual_hash = hashlib.sha256(Path(path).read_text(encoding='utf-8').encode('utf-8')).hexdigest()
        assert actual_hash == expected_hash, f'Reviewed integration changed: {path}'
    expected_platformio = git('show', BASE + ':platformio.ini') + '\n'
    expected_platformio = expected_platformio.replace('\tpost:extra_scripts/nrf54l15_linker.py\n', '').replace(' +<platform/extra_variants/>', '')
    assert Path('platformio.ini').read_text() == expected_platformio, 'Unreviewed build configuration change'
    integration_paths = {'src/main.cpp', 'src/mesh/MeshService.cpp', 'src/mesh/MeshService.h',
                         'src/modules/PositionModule.cpp', 'src/modules/PositionModule.h',
                         'src/modules/NodeInfoModule.cpp', 'src/modules/NodeInfoModule.h',
                         'src/modules/Telemetry/DeviceTelemetry.cpp', 'src/modules/Telemetry/DeviceTelemetry.h'}
    integration_paths |= {path for path in reviewed_sources if path.startswith('src/')}
    changed = set(git('diff', '--name-only', INTEGRATION_BASE, '--', 'src', *removed_specs).splitlines())
    assert changed <= integration_paths, f'Unreviewed integration changes: {changed - integration_paths}'
    expected_position = git('show', POSITION_FIX+':src/modules/PositionModule.cpp')+'\n'
    assert Path('src/modules/PositionModule.cpp').read_text() == expected_position, 'Position differs from reviewed fix'
    assert not git('diff', INTEGRATION_BASE, '--', 'src/mesh/Default.h'), 'Custom stationary floor changed'
    ble_paths = {'src/platform/nrf52/NRF52Bluetooth.cpp', 'src/platform/nrf52/NRF52Bluetooth.h',
                 'src/platform/nrf52/main-nrf52.cpp'}
    nrf_changes = set(git('diff', '--name-only', BASE, '--', 'src/platform/nrf52').splitlines())
    assert nrf_changes <= ble_paths, f'Unreviewed nRF52 changes: {nrf_changes - ble_paths}'
    original_fsm = git('show', BASE+':src/PowerFSM.cpp')+'\n'
    expected_fsm = original_fsm.replace('    setBluetoothEnable(true);',
                                        '    setBluetoothEnable(config.bluetooth.enabled);', 1)
    assert Path('src/PowerFSM.cpp').read_text() == expected_fsm, 'Unreviewed power-state change'
    subprocess.run(['git', 'diff', '--check', BASE, 'HEAD'], check=True)
    conflict = subprocess.run(['git', 'grep', '-n', '-E', '^(<<<<<<<|>>>>>>>)', '--', ':!*.md'], capture_output=True, text=True)
    assert conflict.returncode == 1, conflict.stdout
    original = git('show', BASE+':src/modules/ExternalNotificationModule.cpp')+'\n'
    current = Path('src/modules/ExternalNotificationModule.cpp').read_text()
    start = original.index('            const meshtastic_NodeInfoLite *sender = nodeDB->getMeshNode(mp.from);', original.index('ProcessMessage ExternalNotificationModule::handleReceived'))
    end_marker = '                                     : (ch.settings.has_module_settings && ch.settings.module_settings.is_muted);'
    end = original.index(end_marker, start) + len(end_marker)
    expected = original[:start]+'            const bool isDmToUs = !isBroadcast(mp.to) && isToUs(&mp);\n            const bool is_muted = isMutedForPacket(mp);'+original[end:]
    expected = expected.replace('#include "ExternalNotificationModule.h"', '#include "ExternalNotificationModule.h"\n#include "Channels.h"', 1)
    unformatted_delay = ('                                    : (moduleConfig.external_notification.output_ms\n'
                         '                                           ? moduleConfig.external_notification.output_ms\n'
                         '                                           : EXT_NOTIFICATION_MODULE_OUTPUT_MS);')
    formatted_delay = ('                                    : (moduleConfig.external_notification.output_ms ? moduleConfig.external_notification.output_ms\n'
                       '                                                                                    : EXT_NOTIFICATION_MODULE_OUTPUT_MS);')
    assert expected.count(unformatted_delay) == 2
    expected = expected.replace(unformatted_delay, formatted_delay)
    for before, after in [
        ('if (Throttle::hasElapsed(externalTurnedOn[0], delay))', 'if (genericAlertActive && Throttle::hasElapsed(externalTurnedOn[0], delay))'),
        ('if (Throttle::hasElapsed(externalTurnedOn[1], delay))', 'if (vibraAlertActive && Throttle::hasElapsed(externalTurnedOn[1], delay))'),
        ('if (moduleConfig.external_notification.alert_message_vibra || moduleConfig.external_notification.alert_bell_vibra)', 'if (vibraAlertActive)'),
        ('    stopBuzzerNow();\n    // Turn off all outputs', '    stopBuzzerNow();\n    genericAlertActive = false;\n    vibraAlertActive = false;\n    // Turn off all outputs'),
        ('                setExternalState(0, true);', '                genericAlertActive = true;\n                setExternalState(0, true);'),
        ('void ExternalNotificationModule::triggerVibraOutput()\n{', 'void ExternalNotificationModule::triggerVibraOutput()\n{\n    vibraAlertActive = true;'),
        ('        setExternalState(0, true);', '        genericAlertActive = true;\n        setExternalState(0, true);'),
    ]:
        # Match complete lines so the different indentation levels remain distinct.
        before_lines = '\n' + before + '\n' if before.startswith(' ') else before
        after_lines = '\n' + after + '\n' if before.startswith(' ') else after
        assert expected.count(before_lines) == 1, before
        expected = expected.replace(before_lines, after_lines, 1)
    assert current == expected, 'Custom buzzer/RTTTL code changed beyond reviewed notification fixes'
    assert 'uses: actions/checkout' not in Path('.github/actions/setup-base/action.yml').read_text(), 'Nested checkout regression'
    for backend in ['SX126x', 'LR11x0']:
        text = Path(f'src/mesh/{backend}Interface.cpp').read_text()
        pos = text.index('RX offline for periodic retry')
        assert 'rxOffline = true;' in text[pos:pos+180], backend
        pos = text.index(f'bool {backend}Interface<T>::reconfigure()')
        assert 'return !rxOffline;' in text[pos:text.index('\n}', pos)]
    return {'source_sha': git('rev-parse', 'HEAD'), 'baseline': BASE, 'preserved_paths': preserved,
            'integration_baseline': INTEGRATION_BASE, 'integration_paths': sorted(integration_paths),
            'scope': 'muzi-base only', 'custom_notification_delta': 'mute predicate and independent output activation', 'source_audit': 'PASS',
            'reviewed_upstream_commits': REVIEWED_UPSTREAM_COMMITS,
            'reviewed_upstream_sources': REVIEWED_UPSTREAM_SOURCES,
            'reviewed_power_sources': REVIEWED_POWER_SOURCES,
            'reviewed_release_sources': REVIEWED_RELEASE_SOURCES,
            'removed_unsupported_paths': REMOVED_TARGET_PATHS,
            'build_config_delta': 'Remove unused nRF54 linker hook and extra board-variant source filter'}


def package_audit(directory, sha):
    root = Path(directory)
    def one(pattern):
        paths = list(root.glob(pattern))
        assert len(paths) == 1, (pattern, paths)
        return paths[0]
    manifest_path = one('firmware-muzi-base-*.mt.json')
    meta = json.loads(manifest_path.read_text())
    assert meta['version'] == '2.8.0.'+sha[:7], meta['version']
    assert meta['platformioTarget'] == 'muzi-base' and meta['mcu'] == 'nrf52840' and meta['hwModel'] == 93
    assert meta['architecture'] == 'nrf52840' and meta['repo'] == 'Amoulier/meshtastic-superbase-firmware'
    ota = one('firmware-muzi-base-*-ota.zip')
    uf2 = one('firmware-muzi-base-*.uf2')
    for p in [ota, uf2]:
        entry = next(f for f in meta['files'] if f['name'] == p.name)
        assert entry['bytes'] == p.stat().st_size
        assert entry['md5'] == hashlib.md5(p.read_bytes()).hexdigest()
    with zipfile.ZipFile(ota) as z:
        assert z.testzip() is None and len(z.namelist()) == len(set(z.namelist())) == 3
        app = json.loads(z.read('manifest.json'))['manifest']['application']
        data = z.read(app['bin_file'])
        init = z.read(app['dat_file'])
        assert len(init) == 14
        device, revision, version, count, sd, crc = struct.unpack('<HHIHHH', init)
        assert (device, revision, version, count, sd) == (82, 65535, 4294967295, 1, 182)
        assert binascii.crc_hqx(data, 0xffff) == crc
        assert app['init_packet_data']['firmware_crc16'] == crc
        assert app['init_packet_data']['softdevice_req'] == [182]
    raw = uf2.read_bytes()
    assert len(raw) % 512 == 0 and raw
    nblocks = len(raw)//512
    image = {}
    for block in range(nblocks):
        b = raw[block*512:(block+1)*512]
        m0,m1,flags,addr,size,index,total,family = struct.unpack('<8I', b[:32])
        assert (m0,m1) == (0x0A324655, 0x9E5D5157)
        assert struct.unpack('<I',b[508:])[0] == 0x0AB16F30
        assert flags == 0x2000 and family == 0xADA52840
        assert size == 256 and index == block and total == nblocks
        assert 0x26000 <= addr and addr+size <= 0xEA000, hex(addr)
        assert addr not in image, hex(addr)
        image[addr] = b[32:32+size]
    addresses = sorted(image)
    assert addresses == list(range(0x26000, 0x26000+256*nblocks,256)), 'Noncontiguous UF2'
    payload = b''.join(image[a] for a in addresses)
    assert payload[:len(data)] == data, 'OTA and UF2 do not encode the same image'
    assert len(payload)-len(data) < 256, 'Unexpected UF2 padding'
    assert 0x26000+len(data) <= 0xEA000
    stack,reset = struct.unpack('<II', data[:8])
    assert 0x20000000 <= stack <= 0x20040000 and reset & 1 and 0x26000 <= (reset & ~1) < 0x26000+len(data)
    return {'package_audit':'PASS', 'firmware_version': meta['version'], 'ram_bytes':meta['ram_bytes'],
            'flash_bytes':meta['flash_bytes'], 'application_bytes':len(data), 'application_end':hex(0x26000+len(data)),
            'warm_store_clear_bytes':0xEA000-(0x26000+len(data)), 'uf2_blocks':nblocks,
            'sha256':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in [manifest_path,ota,uf2]}}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--packages')
    parser.add_argument('--sha')
    parser.add_argument('--source', action='store_true')
    parser.add_argument('--output')
    args=parser.parse_args()
    assert args.source or args.packages
    result={}
    if args.source:
        result.update(source_audit())
    if args.packages:
        assert args.sha and len(args.sha)==40
        result.update(package_audit(args.packages,args.sha))
    result['hardware_tested']=False
    text=json.dumps(result,indent=2)+'\n'
    print(text)
    if args.output:
        Path(args.output).parent.mkdir(parents=True,exist_ok=True)
        Path(args.output).write_text(text)


if __name__ == '__main__':
    main()

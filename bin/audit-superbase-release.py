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
                 'src/motion/ICM20948Sensor.cpp', 'src/modules/PositionModule.cpp',
                 'src/mesh/ReliableRouter.cpp', 'src/mesh/Router.cpp', 'src/modules/MQTT.cpp',
                 'src/AudioThread.h', 'src/AudioThread.cpp', 'src/PowerFSM.cpp', 'src/platform/nrf52',
                 'extra_scripts', 'version.properties', 'src/mesh/RadioInterface.cpp',
                 'src/mesh/RF95Interface.cpp', 'src/mesh/LR20x0Interface.cpp', 'src/mesh/SX128xInterface.cpp']
    assert not git('diff', BASE, 'HEAD', '--', *preserved), 'Preserved source changed'
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
    assert current == expected, 'Custom buzzer/RTTTL code changed beyond mute integration'
    assert 'uses: actions/checkout' not in Path('.github/actions/setup-base/action.yml').read_text(), 'Nested checkout regression'
    for backend in ['SX126x', 'LR11x0']:
        text = Path(f'src/mesh/{backend}Interface.cpp').read_text()
        pos = text.index('RX offline for periodic retry')
        assert 'rxOffline = true;' in text[pos:pos+180], backend
        pos = text.index(f'bool {backend}Interface<T>::reconfigure()')
        assert 'return !rxOffline;' in text[pos:text.index('\n}', pos)]
    return {'source_sha': git('rev-parse', 'HEAD'), 'baseline': BASE, 'preserved_paths': preserved,
            'scope': 'muzi-base only', 'custom_notification_delta': 'mute predicate only', 'source_audit': 'PASS'}


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

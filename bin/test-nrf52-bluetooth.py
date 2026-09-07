#!/usr/bin/env python3
"""Execute production nRF52 BLE lifecycle bodies against a deterministic Bluefruit fake.

This checks call ordering and state, not SoftDevice timing, RF, or GATT interoperability.
"""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def function(source, signature):
    start = source.index(signature)
    opening = source.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


def main():
    ble = (ROOT / 'src/platform/nrf52/NRF52Bluetooth.cpp').read_text()
    platform = (ROOT / 'src/platform/nrf52/main-nrf52.cpp').read_text()
    commands = (ROOT / 'src/modules/SystemCommandsModule.cpp').read_text()
    toggle = commands.split('case INPUT_BROKER_MSG_BLUETOOTH_TOGGLE:', 1)[1].split('#else', 1)[0]
    toggle = toggle.replace('#if defined(ARDUINO_ARCH_NRF52)', '')
    bodies = [function(ble, signature) for signature in [
        'static void configureAdvertising(', 'void NRF52Bluetooth::restoreTxPower(',
        'void NRF52Bluetooth::restoreSecurityState(', 'void NRF52Bluetooth::setup(',
        'void NRF52Bluetooth::shutdown(', 'void NRF52Bluetooth::startDisabled(',
        'void NRF52Bluetooth::resumeAdvertising(', 'void NRF52Bluetooth::disconnect(',
        'void updateBatteryLevel(',
    ]]
    bodies += [function(platform, 'void setBluetoothEnable(bool enable)')]
    fsm = (ROOT / 'src/PowerFSM.cpp').read_text()
    bodies += [function(fsm, 'static void setBluetoothEnableUnlessRestarting()')]
    fixture = (ROOT / 'test/fixtures/nrf52_bluetooth_lifecycle.cpp').read_text()
    header = (ROOT / 'src/platform/nrf52/NRF52Bluetooth.h').read_text()
    header = '\n'.join(line for line in header.splitlines() if not line.startswith(('#include', '#pragma')))
    source = fixture.replace('// PRODUCTION_CLASS', header).replace('// PRODUCTION_BODIES', '\n'.join(bodies))
    source = source.replace('// PRODUCTION_TOGGLE', 'void toggle() {\n' + toggle + '\n}')
    with tempfile.TemporaryDirectory(prefix='superbase-ble-') as directory:
        cpp = Path(directory) / 'test.cpp'
        cpp.write_text(source)
        for power in [None, 4]:
            binary = Path(directory) / ('test-default' if power is None else 'test-power')
            command = [os.environ.get('CXX', 'g++'), '-std=c++17', '-Wall', '-Wextra', '-Werror',
                       '-Wno-unused-parameter', '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                       str(cpp), '-o', str(binary)]
            if power is not None:
                command += ['-DNRF52_BLE_TX_POWER=4']
            subprocess.run(command, check=True)
            subprocess.run([str(binary)], check=True)


if __name__ == '__main__':
    main()

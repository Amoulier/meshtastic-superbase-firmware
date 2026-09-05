#!/usr/bin/env python3
"""Run isolated, attributed Superbase native suites and reject incomplete/dirty runs."""
import argparse
import csv
import json
import pathlib
import re
import subprocess
import sys
import xml.etree.ElementTree as ET

MANDATORY = {
    'test_buzzer_mode', 'test_mqtt', 'test_reliable_ack_matrix', 'test_mesh_module',
    'test_packet_signing', 'test_nexthop_routing', 'test_radio', 'test_position_module',
    'test_uptime_clock', 'test_admin_radio', 'test_muted_source', 'test_module_config',
    'test_superbase_radio_recovery', 'test_gps_update_scheduling',
    'test_trackball_press', 'test_superbase_navigation',
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--shard', type=int, default=0)
    parser.add_argument('--shards', type=int, default=1)
    args = parser.parse_args()
    if not 0 <= args.shard < args.shards <= 8:
        parser.error('Require 0 <= shard < shards <= 8')
    available = {p.name for p in pathlib.Path('test').glob('test_*') if p.is_dir()}
    if MANDATORY - available:
        raise SystemExit(f'Missing mandatory suites: {sorted(MANDATORY - available)}')
    selected = sorted(MANDATORY | {s for s in available if re.search(r'gps|power|boot_recovery|identity|crypto|channel|rtttl', s)})
    assigned = selected[args.shard::args.shards]
    assert assigned, 'Empty shard'
    evidence = pathlib.Path('audit-evidence')
    evidence.mkdir(exist_ok=True)
    (evidence / 'expected-suites.json').write_text(json.dumps(assigned, indent=2)+'\n')
    summary = pathlib.Path('.pio/test-state/summary.tsv')
    if summary.exists():
        summary.unlink()
    junit = evidence / 'junit.xml'
    if junit.exists():
        junit.unlink()
    command = ['platformio', 'test', '-e', 'superbase-native-tests', '--junit-output-path', str(junit)]
    for suite in assigned:
        command += ['--filter', suite]
    print('Expected suites:', ', '.join(assigned), flush=True)
    with (evidence / 'native.log').open('w') as logfile:
        process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        for line in process.stdout:
            print(line, end='', flush=True)
            logfile.write(line)
        code = process.wait()
    if code:
        raise SystemExit(code)
    subprocess.run([sys.executable, 'bin/check-test-attribution.py', '--expect', ' '.join(assigned), str(junit)], check=True)
    root = ET.parse(junit).getroot()
    cases = list(root.iter('testcase'))
    assert cases and not any(c.find(x) is not None for c in cases for x in ['failure', 'error', 'skipped']), 'Non-passing JUnit cases'
    assert summary.exists(), 'Missing isolation audit'
    rows = list(csv.reader(summary.open(), delimiter='\t'))
    assert len(rows) == len(assigned), (len(rows), assigned)
    assert {r[0] for r in rows} == set(assigned), 'Isolation attribution mismatch'
    for row in rows:
        assert len(row) == 8 and row[1] == 'PASS' and row[2] == 'CLEAN' and not row[4] and not row[5] and row[6] == 'WITHIN', row
    (evidence / 'state-summary.tsv').write_text(summary.read_text())
    result = {'status': 'PASS', 'suites': assigned, 'tests': len(cases), 'source_sha': subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip()}
    (evidence / 'result.json').write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""Reconcile immutable run evidence and prepare audited release assets."""
import csv
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import xml.etree.ElementTree as ET
import zipfile

SHA = os.environ['CANDIDATE_SHA']
RUN = int(os.environ['AUDIT_RUN'])
REPO = 'Amoulier/meshtastic-superbase-firmware'
TAG = 'v2.8.0-superbase.7'
EVIDENCE = Path('verified-evidence')
OUT = Path('release-assets')

def one(root, name):
    paths = list(root.rglob(name))
    if len(paths) != 1:
        raise RuntimeError(f'Expected one {name} under {root}, found {paths}')
    return paths[0]

def load(root, name):
    return json.loads(one(root, name).read_text())

def require(condition, reason):
    if not condition:
        raise RuntimeError(reason)

def main():
    require(subprocess.check_output(['git','rev-parse','HEAD'], text=True).strip() == SHA, 'Wrong source checkout')
    subprocess.run(['python3','bin/audit-superbase-release.py','--source','--output',str(EVIDENCE/'final-source-audit.json')], check=True)
    source = load(EVIDENCE/'source', 'source-audit.json')
    require(source['source_sha'] == SHA and source['source_audit'] == 'PASS', 'Source evidence mismatch')
    require(load(EVIDENCE, 'final-source-audit.json') == source, 'Source audit changed')
    spec = importlib.util.spec_from_file_location('superbase_tests','bin/test-superbase.py')
    tests = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(tests)
    available = {p.name for p in Path('test').glob('test_*') if p.is_dir()}
    expected = sorted(tests.MANDATORY | {s for s in available if re.search(r'gps|power|boot_recovery|identity|crypto|channel|rtttl',s)})
    require(len(expected) == 22, 'Unexpected suite inventory')
    seen = set()
    cases_total = 0
    counts = {}
    for shard in range(3):
        root = EVIDENCE/f'native-{shard}'
        result = load(root, 'result.json')
        assigned = expected[shard::3]
        require(result['status'] == 'PASS' and result['source_sha'] == SHA and result['suites'] == assigned, f'Shard {shard} mismatch')
        require(load(root,'expected-suites.json') == assigned, 'Assignment mismatch')
        junit = one(root,'junit.xml')
        subprocess.run(['python3','bin/check-test-attribution.py','--expect',' '.join(assigned),str(junit)], check=True)
        xml = ET.parse(junit).getroot()
        allcases = list(xml.iter('testcase'))
        require(allcases and len(allcases) == result['tests'], 'Empty/inconsistent JUnit')
        require(not any(c.find(k) is not None for c in allcases for k in ['failure','error','skipped']), 'Nonpassing test case')
        for suite in xml.iter('testsuite'):
            actual = suite.findall('testcase')
            if actual:
                name = suite.attrib['name'].split(':',1)[1]
                require(name in assigned and name not in seen, 'Duplicate/unexpected suite')
                seen.add(name)
                counts[name] = len(actual)
        rows = list(csv.reader(one(root,'state-summary.tsv').open(),delimiter='\t'))
        require(len(rows) == len(assigned) and {r[0] for r in rows} == set(assigned), 'State inventory mismatch')
        for r in rows:
            require(len(r) == 8 and r[1] == 'PASS' and r[2] == 'CLEAN' and not r[4] and not r[5] and r[6] == 'WITHIN', f'State audit failed: {r}')
        cases_total += len(allcases)
    require(seen == set(expected), 'Incomplete suite coverage')
    firmware = EVIDENCE/'firmware'
    subprocess.run(['python3','bin/audit-superbase-release.py','--packages',str(firmware),'--sha',SHA,'--output',str(EVIDENCE/'final-package-audit.json')], check=True)
    package = load(firmware,'package-audit.json')
    require(package == load(EVIDENCE,'final-package-audit.json'), 'Package audit changed')
    OUT.mkdir(exist_ok=False)
    installable = []
    for name in sorted(package['sha256']):
        src = firmware/name
        require(hashlib.sha256(src.read_bytes()).hexdigest() == package['sha256'][name], 'Artifact hash mismatch')
        shutil.copy2(src,OUT/name)
        installable.append(OUT/name)
    audit = {'status':'PASS','tag':TAG,'source_sha':SHA,'baseline':source['baseline'],
             'upstream_reviewed_sha':'fdb67309aa8fb9a019e07160ac72024c3d25ce2d',
             'audit_run_id':RUN,'audit_run_url':f'https://github.com/{REPO}/actions/runs/{RUN}',
             'selected_upstream_prs':[11651,11676,11678,11671,11688,11697,11686,11709],
             'suite_count':len(seen),'test_cases':cases_total,'failures':0,'errors':0,'skipped':0,
             'native_suite_counts':dict(sorted(counts.items())), 'address_sanitizer':True,
             'suite_state_audit':'PASS / CLEAN / WITHIN declared expected-error budgets',
             'source_audit':source,'package_audit':package,'hardware_tested':False}
    auditname = f'superbase-{TAG}-audit.json'
    (OUT/auditname).write_text(json.dumps(audit,indent=2)+'\n')
    checks = ''.join(f'{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.name}\n' for p in installable)
    install = f'''MuziWorks Superbase only: {TAG}\nSource commit: {SHA}\n\nBack up configuration before updating.\nOTA: use firmware-muzi-base-*-ota.zip WITHOUT extracting it. Requires the MuziWorks OTAFIX bootloader.\nUSB: enter the UF2 bootloader and copy firmware-muzi-base-*.uf2 to its mounted drive.\nThe bundle ZIP itself is NOT an OTA package.\n\n{cases_total} automated tests in {len(seen)} suites passed, no failures/errors/skips.\nSource preservation, install package integrity and memory boundaries passed.\nNo new physical-device, RF range, battery-life or long-duration field tests were performed.\nPassing audits means no errors detected by these checks, not a guarantee of no defects.\n'''
    with zipfile.ZipFile(OUT/f'firmware-muzi-base-{TAG}-bundle.zip','w',zipfile.ZIP_DEFLATED) as z:
        for p in installable: z.write(p,p.name)
        z.write(OUT/auditname,auditname)
        z.writestr('SHA256SUMS.txt',checks)
        z.writestr('INSTALL.txt',install)
    with zipfile.ZipFile(OUT/f'superbase-{TAG}-audit-evidence.zip','w',zipfile.ZIP_DEFLATED) as z:
        for p in sorted(EVIDENCE.rglob('*')):
            if p.is_file() and p.suffix in {'.json','.xml','.tsv','.txt','.diff','.log'}:
                z.write(p,str(p.relative_to(EVIDENCE)))
        z.write('docs/SUPERBASE_RELEASE_7.md','SOURCE_AUDIT_SCOPE.md')
        z.write('bin/audit-superbase-release.py','audit-superbase-release.py')
        z.write('bin/test-superbase.py','test-superbase.py')
    assets = sorted(OUT.iterdir())
    (OUT/'SHA256SUMS.txt').write_text(''.join(f'{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.name}\n' for p in assets))
    notes = f'''# MuziWorks Superbase — audited reliability update\n\nFinal release **{TAG}**, source `{SHA}`. Runtime version **{package['firmware_version']}**.\n\nThis is a selective Meshtastic 2.8.0 fork update, not a full upstream 2.8.1 merge. The only physical target is `muzi-base` / nRF52840.\n\n## Changes\n- BLE/admin configuration and preference restoration: #11651.\n- SX1262/LR1121 radio recovery during reconfiguration and RX/TX operations: #11676 and #11678.\n- BaseUI message banners and muted channel/sender handling: #11671 and #11688.\n- GPS fix validity across a search cycle: #11697.\n- Correct validation of restored/derived weak identity keys: #11686.\n- Accurate unavailable-module metadata: #11709.\n\n## Corrections found during audit\n- Recovery throttling at time zero and 32-bit rollover; saturated failure counter; valid delayed-reboot sentinel.\n- Do not hide SX126x begin errors behind later successful commands.\n- Failed radio reconfiguration marks RX offline for periodic recovery and reports failure accurately.\n- Native setup no longer overwrites the explicitly selected source revision.\n- The new mocked-radio test initializes/restores the region required by the production constructor.\n\n## Final automated audit\n- **{cases_total} test cases in {len(seen)} suites: 0 failures, 0 errors, 0 skipped.**\n- AddressSanitizer, test attribution and isolated-state checks passed. Expected injected error logs stayed within each suite's declared budget.\n- Fresh exact-source `muzi-base` build passed.\n- OTA CRC/SoftDevice metadata, UF2 family/blocks/vectors, manifest hashes and OTA/UF2 byte equivalence passed.\n- Application ends at `{package['application_end']}`; {package['warm_store_clear_bytes']:,} bytes remain before the protected warm-store boundary.\n- Source and package checks were repeated immediately before publication.\n- Evidence: https://github.com/{REPO}/actions/runs/{RUN}\n\n## Preserved\nMQTT implicit ACK, DMs Only buzzer, independent notification outputs, RTTTL ownership/locking, 12-hour stationary position interval, IMU/magnetometer sleep, existing GPS/display power handling, OTAFIX path, TX power and RX Boosted Gain policy. The toolchain, other boards, unrelated radio backends and protobuf/device-ui dependencies were not updated.\n\n## Install\nBack up configuration first. **OTA:** install the `*-ota.zip` without extracting; the MuziWorks OTAFIX bootloader is required. **USB:** copy the `.uf2` to the bootloader drive. The bundle ZIP contains both formats and is not itself an OTA package. SHA256SUMS and machine-readable audit evidence are attached.\n\n**Validation limit:** no new physical-device, RF-range, battery-life or long-duration field testing was performed. Passing automated gates means no errors detected by those checks, not proof of absence of all bugs. This remains unofficial firmware from this Superbase-only fork.\n'''
    Path('release-notes.md').write_text(notes)
    print(json.dumps({'status':'PASS','tests':cases_total,'suites':len(seen),'assets':[p.name for p in OUT.iterdir()]},indent=2))

if __name__=='__main__': main()

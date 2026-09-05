#!/usr/bin/env python3
"""Publish exact-source artifacts only after successful audits and digest checks."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import urllib.error
import urllib.parse
import urllib.request

REPO = 'Amoulier/meshtastic-superbase-firmware'
TAG = 'v2.8.0-superbase.7'
SHA = '7c39922e028a68dbe3af7470aea8eb8d849fc569'
RUN = 33977313891
HEAD = '17e7638837882a18a3fb9caae26332f25062e2fa'
TOKEN = os.environ['GH_TOKEN']
BASE = 'https://api.github.com'
OUT = Path('release-assets')
EVIDENCE = Path('verified-evidence')

def request(path, method='GET', value=None, data=None, content_type=None):
    url = path if path.startswith('https://uploads.github.com/') else BASE+path
    if value is not None:
        data = json.dumps(value).encode()
        content_type = 'application/json'
    headers = {'Authorization':'Bearer '+TOKEN, 'Accept':'application/vnd.github+json',
               'X-GitHub-Api-Version':'2022-11-28','User-Agent':'superbase-release-audit'}
    if content_type: headers['Content-Type'] = content_type
    req = urllib.request.Request(url,headers=headers,data=data,method=method)
    with urllib.request.urlopen(req, timeout=120) as r:
        raw = r.read()
        return json.loads(raw) if raw else None

def verify_gate():
    run = request(f'/repos/{REPO}/actions/runs/{RUN}')
    assert run['status'] == 'completed' and run['conclusion'] == 'success', 'Audit run did not pass'
    assert run['head_sha'] == HEAD and run['path'] == '.github/workflows/superbase_integration_audit.yml'
    assert run['repository']['full_name'] == REPO and run['head_repository']['full_name'] == REPO
    jobs = request(f'/repos/{REPO}/actions/runs/{RUN}/jobs?per_page=100')
    required = {'source','firmware','native (0)','native (1)','native (2)','verification'}
    assert jobs['total_count'] == len(jobs['jobs']) == len(required)
    assert {j['name'] for j in jobs['jobs']} == required
    assert all(j['status'] == 'completed' and j['conclusion'] == 'success' for j in jobs['jobs'])
    branch = request(f'/repos/{REPO}/git/ref/heads/develop')
    assert branch['object']['sha'] == SHA, 'Develop moved or was not promoted'
    EVIDENCE.mkdir(exist_ok=True)
    (EVIDENCE/'audit-run.json').write_text(json.dumps(run,indent=2)+'\n')
    (EVIDENCE/'audit-jobs.json').write_text(json.dumps(jobs,indent=2)+'\n')
    return run

def main():
    verify_gate()
    artifacts = request(f'/repos/{REPO}/actions/runs/{RUN}/artifacts?per_page=100')
    expected = {'superbase-final-source':'source','superbase-final-firmware':'firmware',
                'superbase-final-native-0':'native-0','superbase-final-native-1':'native-1',
                'superbase-final-native-2':'native-2'}
    assert artifacts['total_count'] == len(artifacts['artifacts']) == len(expected)
    assert {a['name'] for a in artifacts['artifacts']} == set(expected)
    for artifact in artifacts['artifacts']:
        assert not artifact['expired'] and artifact['workflow_run']['head_sha'] == HEAD
        target = EVIDENCE/expected[artifact['name']]
        subprocess.run(['gh','run','download',str(RUN),'--repo',REPO,'--name',artifact['name'],'--dir',str(target)],check=True)
    (EVIDENCE/'audit-artifacts.json').write_text(json.dumps(artifacts,indent=2)+'\n')
    subprocess.run(['python3',os.environ['PREPARE_SCRIPT']],check=True)
    report = json.loads((OUT/f'superbase-{TAG}-audit.json').read_text())
    assert report['status'] == 'PASS' and report['source_sha'] == SHA and report['suite_count'] == 22
    assert report['failures'] == report['errors'] == report['skipped'] == 0
    assert report['package_audit']['package_audit'] == 'PASS'
    assert report['source_audit']['source_audit'] == 'PASS'
    releases = request(f'/repos/{REPO}/releases?per_page=100')
    matching = [r for r in releases if r['tag_name'] == TAG]
    assert len(matching) <= 1
    for r in matching:
        assert r['draft'] and r['target_commitish'] == SHA, 'Existing release is not our unpublished draft'
    try:
        ref = request(f'/repos/{REPO}/git/ref/tags/{TAG}')
        assert ref['object']['type'] == 'commit' and ref['object']['sha'] == SHA, 'Tag collision'
    except urllib.error.HTTPError as exc:
        if exc.code != 404: raise
        request(f'/repos/{REPO}/git/refs','POST',{'ref':'refs/tags/'+TAG,'sha':SHA})
    verify_gate()
    metadata = {'tag_name':TAG,'target_commitish':SHA,
                'name':'Meshtastic 2.8.0 for MuziWorks Superbase — Audited Reliability Update',
                'body':Path('release-notes.md').read_text(),'draft':True,'prerelease':False}
    if matching:
        release = request(f'/repos/{REPO}/releases/{matching[0]["id"]}','PATCH',metadata)
    else:
        release = request(f'/repos/{REPO}/releases','POST',metadata)
    existing = {a['name']:a for a in request(f'/repos/{REPO}/releases/{release["id"]}/assets?per_page=100')}
    local = {p.name:p for p in OUT.iterdir() if p.is_file()}
    assert len(local) == 7, local.keys()
    assert not (existing.keys()-local.keys()), 'Unexpected draft assets'
    upload = release['upload_url'].split('{',1)[0]
    for name,path in sorted(local.items()):
        expected_digest = 'sha256:'+hashlib.sha256(path.read_bytes()).hexdigest()
        if name in existing:
            asset = existing[name]
            assert asset['digest'] == expected_digest and asset['size'] == path.stat().st_size
        else:
            asset = request(upload+'?name='+urllib.parse.quote(name,safe=''), 'POST',
                            data=path.read_bytes(),content_type='application/octet-stream')
            assert asset['digest'] == expected_digest and asset['size'] == path.stat().st_size
    final_assets = request(f'/repos/{REPO}/releases/{release["id"]}/assets?per_page=100')
    assert len(final_assets) == len(local) and {a['name'] for a in final_assets} == set(local)
    for a in final_assets:
        p = local[a['name']]
        assert a['state'] == 'uploaded' and a['size'] == p.stat().st_size
        assert a['digest'] == 'sha256:'+hashlib.sha256(p.read_bytes()).hexdigest()
    verify_gate()
    final = request(f'/repos/{REPO}/releases/{release["id"]}','PATCH',{'draft':False,'prerelease':False,'make_latest':'true'})
    assert not final['draft'] and not final['prerelease'] and final['published_at']
    latest = request(f'/repos/{REPO}/releases/latest')
    assert latest['id'] == final['id'] and latest['tag_name'] == TAG
    result = {'status':'PUBLISHED','release_id':final['id'],'tag':TAG,'source_sha':SHA,
              'html_url':final['html_url'],'published_at':final['published_at'],
              'test_cases':report['test_cases'],'suite_count':report['suite_count'],
              'assets':[{k:a[k] for k in ['name','size','digest','browser_download_url']} for a in final_assets]}
    Path('publication-result.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))

if __name__ == '__main__': main()

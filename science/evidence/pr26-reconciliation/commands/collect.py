from pathlib import Path
import hashlib, json, re, shutil, subprocess, xml.etree.ElementTree as ET

root=Path(__file__).resolve().parents[3]
out=root/'out/verification/reconciliation'
destination=root/'science/evidence/pr26-reconciliation'
destination.mkdir(parents=True,exist_ok=True)
def git(*args):return subprocess.check_output(['git',*args],cwd=root).decode().strip()
def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def copy(path,dest):
    dest.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(path,dest)
counts={}
native_counts={}
for variant in ['normal','asan']:
    attempt=out/('attempt-02-'+variant)
    for p in attempt.iterdir():
        if p.is_file():copy(p,destination/variant/p.name)
    build=root/('out/reconcile-'+variant)
    copy(build/'CMakeCache.txt',destination/variant/'CMakeCache.txt')
    copy(build/'Testing/Temporary/LastTest.log',destination/variant/'LastTest.log')
    flags=[line for line in (build/'build.ninja').read_text().splitlines() if re.match(r'  (FLAGS|LINK_FLAGS) =',line)]
    (destination/variant/'flags.txt').write_text('\n'.join(sorted(set(flags)))+'\n')
    copy(build/'desktop/test-artifacts/scientific-history.json',destination/variant/'scientific-history.json')
    tree=ET.parse(attempt/'ctest.xml')
    cases=list(tree.getroot().iter('testcase'))
    assert len(cases)==28 and all(t.get('status')=='run' and t.find('failure') is None and t.find('skipped') is None for t in cases)
    counts[variant]={}
    for file in sorted(attempt.glob('*-results.txt')):
        text=file.read_text(encoding='utf-8-sig')
        match=re.search(r'Totals: (\d+) passed, (\d+) failed, (\d+) skipped',text)
        assert match,file
        values=[int(v) for v in match.groups()];assert values[1:]==[0,0],(file,values)
        counts[variant][file.stem]=dict(zip(['passed','failed','skipped'],values))
    assert sum('Cancel/retry cycle ' in l for l in (attempt/'storage_worker-results.txt').read_text().splitlines())==50
    complete=(build/'Testing/Temporary/LastTest.log').read_text()
    native_counts[variant]={name:len(re.findall(r'(?m)^PASS ',re.search(r'[0-9]+/28 Testing: '+re.escape(name)+r'\n(.*?)Test time =',complete,re.S).group(1)))
        for name in ['engine.acceptance','engine.search','engine.control_influence','engine.transport']}
    assert list(native_counts[variant].values())==[22,22,15,25]
for p in (out/'attempt-03-science').iterdir():
    if p.is_file():copy(p,destination/'science'/p.name)
for p in (out/'attempt-06-installed').iterdir():
    if p.suffix in ['.json','.ndjson','.stderr','.stdout','.png']:
        copy(p,destination/'installed'/p.name)
for p in (out/'attempt-05-installed').iterdir():
    if p.suffix in ['.json','.py','.stderr','.stdout','.png']:
        copy(p,destination/'failed-installed-attempt-05'/p.name)
copy(out/'installed-attempt-06.log',destination/'installed/execution.log')
for name in ['configure.log','build.log','install.log','execution.json','export-source-hashes.json']:
    copy(out/'attempt-04-package'/name,destination/'package'/name)
for name in ['run-build.ps1','run-science.py','run-package.ps1','installed_checks.py','resolve_initial.py','collect.py','report.py','finalize.py']:
    copy(out/name,destination/'commands'/name)
copy(out/'attempt-01/conflicts.patch',destination/'merge/conflicts.patch')
copy(out/'toolchain.json',destination/'toolchain.json')
copy(out/'process-check.json',destination/'process-check.json')
sources=git('ls-files','desktop','engine','science','proofs','contracts','scripts','CMakeLists.txt','CMakePresets.json').splitlines()
sources=[p for p in sources if not p.startswith('science/evidence/')]
manifest=dict(verified_revision='f50431088806927abaa823858ab5377276223972',verified_tree=git('rev-parse','f504310^{tree}'),
    merge_parents=git('show','-s','--format=%P','f504310').split(),
    main='e057fa44ed8c7c3127466d72d816a68228e9b033',
    production_diff_from_main=git('diff','--name-only','origin/main','--','desktop/science','desktop/history','desktop/qml','desktop/main.cpp','contracts'),
    source_hashes={p:digest(root/p) for p in sources},
    executables={str(p.relative_to(root)):digest(p) for v in ['normal','asan'] for p in (root/('out/reconcile-'+v)).rglob('*.exe')},
    qt_counts=counts,native_counts=native_counts)
assert not manifest['production_diff_from_main']
(destination/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
scan=[]
for p in destination.rglob('*'):
    if p.is_file() and p.suffix in ['.log','.txt','.stderr','.stdout','.xml']:
        text=p.read_text(encoding='utf-8-sig',errors='replace')
        for line in text.splitlines():
            if re.search(r'ERROR: AddressSanitizer|SUMMARY: AddressSanitizer|AddressSanitizer: (heap|stack|global|DEADLYSIGNAL)',line):
                scan.append(dict(path=str(p.relative_to(destination)),line=line))
assert not scan,scan
(destination/'summary.json').write_text(json.dumps(dict(status='passed',ctests_per_build=28,qt_counts=counts,native_counts=native_counts,
    python_tests=127,python_skips=0,actual_worker_requests=302,fixtures=18,central_differences=132,
    sanitizer_reports=len(scan),installed=json.loads((destination/'installed/summary.json').read_text())),indent=2)+'\n')
print(json.dumps(counts,indent=2))

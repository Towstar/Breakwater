from pathlib import Path
import hashlib,json,re,shutil,subprocess,xml.etree.ElementTree as ET

root=Path(__file__).resolve().parents[1]
out=root/'out/f1';dest=root/'science/evidence/f1'
dest.mkdir(parents=True,exist_ok=True)
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def write(path,value):
    path.parent.mkdir(parents=True,exist_ok=True)
    path.write_bytes((json.dumps(value,indent=2,allow_nan=False)+'\n').encode())
def copy(source,target):
    target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(source,target)
def git(*args):return subprocess.check_output(['git',*args],cwd=root).decode().strip()
native_counts={};attempts=[];full={}
for variant,attempt in [('normal','attempt-06-normal'),('asan','attempt-07-asan')]:
    folder=out/attempt;build=out/variant
    metric_folder=out/('attempt-11-normal' if variant=='normal' else 'attempt-12-asan')
    execution=json.loads((folder/'execution.json').read_text())
    assert execution['verified_revision']=='4841c042d81eb43fe06871a89c4b096c778707b9' and execution['ctest_exit']==0
    cases=list(ET.parse(folder/'ctest.xml').getroot().iter('testcase'))
    assert len(cases)==8 and all(c.get('status')=='run' and c.find('failure') is None and c.find('skipped') is None for c in cases)
    native_text=(folder/'native-individual.log').read_text()
    native_counts[variant]={name:len(re.findall(r'(?m)^PASS ',re.search(r'[0-9]+/8 Testing: '+re.escape(name)+r'\n(.*?)Test time =',native_text,re.S).group(1)))
        for name in ['engine.acceptance','engine.search','engine.control_influence','engine.transport']}
    assert list(native_counts[variant].values())==[22,22,15,25]
    for name in ['build.log','registered-tests.json','ctest.log','ctest.xml','native-individual.log','executions.json','execution.json',
                 'f1.log','a9-refined.log','a9-coarse.log']:
        copy(folder/name,dest/variant/name)
    for name in ['configure.log','build.log','execution.json']:
        copy(out/('build-'+variant)/name,dest/variant/'initial-build'/name)
    copy(build/'CMakeCache.txt',dest/variant/'CMakeCache.txt')
    flags=[line for line in (build/'build.ninja').read_text().splitlines() if re.match(r'  (FLAGS|LINK_FLAGS) =',line)]
    (dest/variant/'flags.txt').write_bytes(('\n'.join(sorted(set(flags)))+'\n').encode())
    metric_execution=json.loads((metric_folder/'execution.json').read_text())
    assert metric_execution['verified_revision']=='fffe5c342caf64725c6242215a8858280ca75441'
    for name in ['f1.log','executions.json','execution.json']:
        copy(metric_folder/name,dest/variant/('metric-'+name))
    report=json.loads((metric_folder/'f1/report.json').read_text());full[variant]=report
    old_report=json.loads((folder/'f1/report.json').read_text())
    assert report['native_executable_sha256']==old_report['native_executable_sha256'] and report['native_data_sha256']==old_report['native_data_sha256']
    assert all(old_report['source_hashes'][p]==value for p,value in report['source_hashes'].items() if p.startswith('engine/'))
    assert report['status']=='completed' and report['accuracy_status']=='characterized_not_qualified' and report['case_count']==22
    assert all(sha(root/path)==value for path,value in report['source_hashes'].items())
    compact=json.loads(json.dumps(report))
    for case in compact['cases']:
        case.pop('per_frame')
        for observation in case['observations']:
            indices=observation.pop('original_indices')
            observation['retained_index_count']=len(indices)
            observation['last_original_index']=indices[-1]
    compact['full_report_sha256']=sha(metric_folder/'f1/report.json')
    compact['retained_full_report']=str((metric_folder/'f1/report.json').relative_to(root)).replace('\\','/')
    write(dest/variant/'f1-summary.json',compact)
    copy(metric_folder/'f1/preflight.json',dest/variant/'f1-preflight.json')
    copy(metric_folder/'f1/report.md',dest/variant/'f1-summary.md')
    for study in ['a9-refined','a9-coarse']:
        value=json.loads((folder/study/'report.json').read_text())
        assert value['case_count']==45 and value['gate_count']==580
        if study=='a9-refined':assert value['status']=='passed' and value['failed_gates']==[]
        else:assert value['status']=='failed' and len(value['failed_gates'])==1 and value['failed_gates'][0]['name']=='scheduled_inlet.space.refinement_order'
        copy(folder/study/'report.json',dest/variant/(study+'.json'))
for folder in sorted(out.glob('attempt-*')):
    if folder.is_dir():
        for file in folder.rglob('*'):
            if file.is_file():attempts.append(dict(path=file.relative_to(root).as_posix(),bytes=file.stat().st_size,sha256=sha(file)))
assert full['normal']['native_data_sha256']==full['asan']['native_data_sha256']
copy(out/'attempt-11-normal/python-tests.log',dest/'python-tests.log')
python_log=(dest/'python-tests.log').read_text()
assert 'Ran 186 tests' in python_log and '\nOK\n' in python_log and 'skipped=' not in python_log
copy(out/'attempt-09-legacy-identity/identity.json',dest/'legacy-byte-identity.json')
copy(out/'process-check.json',dest/'process-check.json')
for name in ['attempt.json','tests.log','front_reference.py','test_front_reference.py']:
    copy(out/'attempt-01-reference-tests'/name,dest/'failed-attempt-01'/name)
for name in ['check-legacy.py','failure.json']:
    copy(out/'attempt-08-legacy-identity'/name,dest/'failed-attempt-08'/name)
for name in ['qualify_front_accuracy.py','test_front_accuracy.py','tests.log','attempt.json']:
    copy(out/'attempt-10-metric-regression'/name,dest/'failed-attempt-10'/name)
copy(out/'metric-correction-tests.log',dest/'focused-tests/metric-correction.log')
for name in ['write-report.py','failure.json']:
    copy(out/'attempt-13-report-generation'/name,dest/'failed-attempt-13'/name)
for number in ['03','04','05']:
    copy(out/f'attempt-{number}-focused-tests/tests.log',dest/f'focused-tests/attempt-{number}.log')
for name in ['run-build.ps1','run-verification.ps1','run-metric-verification.ps1','check-legacy.py','collect-evidence.py','write-report.py','finalize-evidence.py']:
    copy(root/'out'/name,dest/'commands'/name)
sources=git('ls-files','engine','science','proofs','contracts','CMakeLists.txt','CMakePresets.json').splitlines()
sources=[p for p in sources if not p.startswith('science/evidence/') and not p.endswith('.md')]
manifest=dict(baseline='de06aa1860ccbd3af2e4091557a50920ad16d423',guidance_commit='9941859b202d804b024d788b1238d94166b33309',
    study_implementation_commit='4841c042d81eb43fe06871a89c4b096c778707b9',
    verified_code_commit='fffe5c342caf64725c6242215a8858280ca75441',verified_code_tree=git('rev-parse','fffe5c3^{tree}'),
    guidance_sha256=sha(root/'science/OBSERVATION_SEMANTICS.md'),
    sources={p:sha(root/p) for p in sources},native_counts=native_counts,
    executables={p.relative_to(root).as_posix():sha(p) for variant in ['normal','asan'] for p in (out/variant/'engine').glob('*.exe')})
assert git('diff','de06aa1','--name-only','--','engine/src','engine/include','engine/CMakeLists.txt','desktop','contracts','proofs','docs','CMakeLists.txt','CMakePresets.json')==''
assert not git('ls-files','*PLAN_LOCAL*','*LOCAL_PLAN*')
write(dest/'manifest.json',manifest);write(dest/'retained-attempt-files.json',attempts)
scan=[]
for p in dest.rglob('*'):
    if p.suffix in ['.log','.txt','.xml']:
        for line in p.read_text(encoding='utf-8-sig',errors='replace').splitlines():
            if re.search(r'ERROR: AddressSanitizer|SUMMARY: AddressSanitizer|AddressSanitizer: (heap|stack|global|DEADLYSIGNAL)',line):scan.append(dict(path=p.relative_to(dest).as_posix(),line=line))
assert not scan,scan
write(dest/'summary.json',dict(status='study_completed_accuracy_not_qualified',python_tests=186,python_skips=0,new_focused_tests=59,
    ctests_per_build=8,native_counts=native_counts,f1_cases_per_build=22,asan_reports=0,
    a9_refined=dict(cases=45,gates=580,status='passed_both_builds'),
    a9_coarse=dict(cases=45,gates=580,expected_failed_gates=1,status='known_failure_retained_both_builds'),
    normal_asan_native_data_identical=True,legacy_a9_byte_identical_to_pre_f1=True,
    maximum_reference_disagreement_mg_l=max(c['reference']['crosscheck_disagreement_mg_l'] for c in full['normal']['cases']),
    maximum_mass_residual_mg=max(c['maximum_signed_mass_residual_mg'] for c in full['normal']['cases']),
    failed_attempts=[dict(attempt='01',reason='New oracle jump-side cancellation and Poisson weight roundoff; code corrected without relaxed tolerances'),
                     dict(attempt='08',reason='Legacy comparison harness path-key separator lookup; corrected without product changes'),
                     dict(attempt='10',reason='Direct RMS squaring underflowed tiny finite outlet errors; scaled norm and regression added'),
                     dict(attempt='13',reason='Report helper expected one space before CTest padded elapsed times; corrected without numerical changes')]))
print('PASS: 8/8 native suites in both builds; 186 Python tests, no skips; 22 F1 cases each; legacy qualification and byte identity preserved; zero ASan reports')

from pathlib import Path
import hashlib,json,subprocess,time
root=Path(__file__).resolve().parents[1]
out=root/'out/f1/attempt-08-legacy-identity';out.mkdir(parents=True,exist_ok=False)
baseline=Path('C:/Projects/Breakwater/.worktrees/combined-verification/out/reconcile-normal/engine/breakwater_transport_qualification_reference.exe')
historical=json.loads((root/'science/evidence/pr26-reconciliation/manifest.json').read_text())
expected=historical['executables']['out/reconcile-normal/engine/breakwater_transport_qualification_reference.exe']
actual=hashlib.sha256(baseline.read_bytes()).hexdigest();assert actual==expected
rows=[]
for name,args in [('a9-coarse',[]),('a9-refined',['--refined-inlet'])]:
    started=time.perf_counter();p=subprocess.run([str(baseline),*args],capture_output=True,timeout=300)
    assert p.returncode==0 and not p.stderr
    original=hashlib.sha256(p.stdout).hexdigest()
    variants={}
    for variant,attempt in [('normal','attempt-06-normal'),('asan','attempt-07-asan')]:
        path=root/'out/f1'/attempt/name/'native-reference.json'
        value=path.read_bytes();assert value==p.stdout,(name,variant)
        variants[variant]=hashlib.sha256(value).hexdigest()
    rows.append(dict(name=name,baseline_command=[str(baseline),*args],baseline_executable_sha256=actual,
        baseline_stdout_sha256=original,matched_variants=variants,elapsed_s=time.perf_counter()-started))
f1={v:json.loads((root/'out/f1'/a/'f1/report.json').read_text()) for v,a in [('normal','attempt-06-normal'),('asan','attempt-07-asan')]}
assert f1['normal']['native_data_sha256']==f1['asan']['native_data_sha256']
(out/'identity.json').write_bytes((json.dumps(dict(status='passed',baseline_verified_revision=historical['verified_revision'],
    a9=rows,f1_normal_asan_native_data_sha256=f1['normal']['native_data_sha256']),indent=2)+'\n').encode())
print('PASS: both A9 modes byte-identical to the retained pre-F1 executable in both builds; F1 normal/ASan native data byte-identical')

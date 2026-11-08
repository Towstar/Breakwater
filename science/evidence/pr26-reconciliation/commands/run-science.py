from pathlib import Path
import importlib.metadata as metadata
import json, os, subprocess, sys, time

root=Path(__file__).resolve().parents[3]
out=root/'out/verification/reconciliation/attempt-03-science'
out.mkdir(parents=True, exist_ok=True)
env=dict(os.environ, PYTHONUTF8='1')
env.pop('NVIDIA_API_KEY',None)
operations=[('python-tests',['-m','unittest','discover','-s','science/tests','-v']),
 ('rollout',['science/verify_rollout.py','--native','out/reconcile-normal/engine/breakwater_rollout_reference.exe','--report',str(out/'rollout.json')]),
 ('worker',['science/verify_worker.py','--native','out/reconcile-normal/engine/breakwater_rollout_reference.exe','--report',str(out/'worker.json')])]
results=[]
for name,args in operations:
    start=time.perf_counter()
    with (out/f'{name}.log').open('wb') as log:
        result=subprocess.run([sys.executable,'-u',*args],cwd=root,env=env,stdout=log,stderr=subprocess.STDOUT)
    results.append(dict(name=name,command=[sys.executable,'-u',*args],exit=result.returncode,elapsed_s=time.perf_counter()-start))
    (out/'executions.json').write_text(json.dumps(results,indent=2)+'\n')
    print('\n'.join((out/f'{name}.log').read_text(encoding='utf-8').splitlines()[-5:]),flush=True)
    assert result.returncode==0
text=(out/'python-tests.log').read_text(encoding='utf-8')
assert 'Ran 127 tests' in text and 'skipped=' not in text and '\nOK\n' in text
(out/'runtime.json').write_text(json.dumps(dict(python=sys.version,executable=sys.executable,
    packages={name:metadata.version(name) for name in ['torch','numpy','nvidia-physicsnemo','jsonschema']}),indent=2)+'\n')

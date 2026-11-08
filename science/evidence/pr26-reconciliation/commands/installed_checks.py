from pathlib import Path
import hashlib, json, math, os, shutil, sqlite3, subprocess, sys, time

root=Path(__file__).resolve().parents[3]
out=root/'out/verification/reconciliation/attempt-06-installed'
out.mkdir(parents=True,exist_ok=True)
runtime=root/'.tools/science-cpu/Scripts/python.exe'
base_runtime=root/'.tools/uv-python/cpython-3.12.13-windows-x86_64-none/python.exe'
assert runtime.is_file() and base_runtime.is_file()
env=dict(os.environ, BREAKWATER_PYTHON=str(runtime), PATH=os.environ['SystemRoot']+'/System32;'+os.environ['SystemRoot'])
for name in ['PYTHONPATH','PYTHONHOME','VIRTUAL_ENV','NVIDIA_API_KEY','QT_PLUGIN_PATH','QML2_IMPORT_PATH','QML_IMPORT_PATH']:
    env.pop(name,None)
results=[]
def run(installation,name,extra,*,python=runtime,expected=0,error=None,science=False,db=None):
    database=db or out/(name+'.sqlite')
    command=[str(installation/'bin/breakwater.exe'),'--history-db',str(database),'--smoke',*map(str,extra)]
    report=out/(name+'.json')
    if science:command+=['--science-check','--science-report',str(report)]
    started=time.perf_counter()
    process=subprocess.run(command,cwd=out,env=dict(env,BREAKWATER_PYTHON=str(python)),capture_output=True,timeout=120)
    (out/(name+'.stdout')).write_bytes(process.stdout)
    (out/(name+'.stderr')).write_bytes(process.stderr)
    value=json.loads(report.read_text()) if science else None
    row=dict(name=name,command=command,exit=process.returncode,expected_exit=expected,elapsed_s=time.perf_counter()-started)
    results.append(row);(out/'executions.json').write_text(json.dumps(results,indent=2)+'\n')
    assert process.returncode==expected,row
    if science and expected==0:
        assert value['within_tolerance'] and value['native_cost']==12780 and value['worker_cost']==12780
    if error: assert value['error']['code']==error,(row,value)
    return value,database

local=root/'out/reconcile-install-local';relocated=root/'out/reconcile-install-relocated'
assert local.is_dir()
if not relocated.exists():shutil.copytree(local,relocated)
assert hashlib.sha256((local/'bin/breakwater.exe').read_bytes()).digest()==hashlib.sha256((relocated/'bin/breakwater.exe').read_bytes()).digest()
source=root/'out/reconcile-source';unavailable=source.with_name('reconcile-source.unavailable')
assert source.resolve().is_relative_to(root) and unavailable.resolve().is_relative_to(root)
if source.is_dir():
    assert not unavailable.exists()
    source.rename(unavailable)
else:assert unavailable.is_dir()
(out/'source-unavailable.json').write_text(json.dumps(dict(source=str(source),unavailable=str(unavailable),
    original_exists=source.exists(),interpreter=str(runtime),scope='Only disposable source export made unavailable; original checkout and other sessions preserved.'),indent=2)+'\n')
db_results=[]
for install,label in [(local,'local'),(relocated,'relocated')]:
    result,db_path=run(install,label+'-sensitivities',[
        '--science-derivatives','--science-control','source:source_a:curtail',
        '--science-control','route:bypass:flow','--screenshot',out/(label+'-live.png')],science=True)
    assert result['analysis_mode']=='sensitivities'
    assert result['derivatives']['control_ids']==['source:source_a:curtail','route:bypass:flow']
    assert result['derivatives']['terminal_volume_jacobian_h']==[[-3,-3],[0,3]]
    assert result['derivatives']['cost_gradient']==[750,0]
    with sqlite3.connect(db_path) as db:
        assert db.execute('pragma integrity_check').fetchall()==[('ok',)]
        rows=db.execute('select session_id,archive from sessions').fetchall()
        assert len(rows)==1 and rows[0][0]==result['result_id']
        raw=rows[0][1];archive=json.loads(raw)
    assert archive['session_kind']=='scientific' and archive['status']=='completed'
    assert archive['actionable'] is False and not archive['approvals'] and not archive['recommendation']
    assert archive['scientific_evidence']==[result]
    native=archive['runs'][0]
    assert len(native['time_h'])==121 and native['time_h'][0]==0 and native['time_h'][-1]==3
    assert native['tank_ids']==['tank_a','tank_b']
    for values in [native['volumes_m3'],result['preview']['worker_volumes_m3']]:
        for row,expected in zip(values,[80,117]):
            value=row[-1]
            assert math.isfinite(value) and abs(value-expected)<=1e-9+1e-10*max(abs(value),abs(expected))
    assert result['selected_controls'][0]['captured_value']==10 and result['selected_controls'][1]['captured_value']==20
    session_file=out/(label+'-persisted-session.json')
    session_file.write_bytes(raw if isinstance(raw,bytes) else raw.encode())
    # Reopen the installed app's exact persisted archive in independent processes.
    # Actual repository export/reimport is separately exercised by science_evidence.
    for size in ['normal','compact']:
        args=['--import-session',session_file,'--open-scientific-history','--screenshot',out/(label+'-history-'+size+'.png')]
        if size=='compact':args+=['--compact-ui']
        _,reopened=run(install,label+'-history-'+size,args,python=out/'absent-python.exe')
        with sqlite3.connect(reopened) as db:
            assert db.execute('pragma integrity_check').fetchall()==[('ok',)]
            loaded=json.loads(db.execute('select archive from sessions where session_id=?',(result['result_id'],)).fetchone()[0])
        assert loaded['scientific_evidence']==[result] and loaded['runs']==archive['runs']
        assert loaded==archive and loaded['actionable'] is False and not loaded['recommendation']
        # Historical marking is a load/view transformation; canonical SQLite
        # archives stay immutable. The smoke requires HistoryController.historical
        # and nonempty historical science; Qt tests verify its non-actionable view.
    db_results.append(dict(installation=label,result_id=result['result_id'],session_count=1,integrity='ok',
        full_native_samples=121,derivatives=result['derivatives'],reopened_exact=True,historical_actionable=False))
    for module,code in [('worker.py','worker_unavailable'),('worker_storage.py','runtime_unavailable'),('storage_grid.py','runtime_unavailable')]:
        path=install/'share/breakwater/science'/module;backup=path.with_suffix(path.suffix+'.unavailable')
        path.rename(backup)
        try:run(install,label+'-missing-'+module,[],science=True,expected=6,error=code)
        finally:backup.rename(path)
    run(install,label+'-missing-interpreter',[],science=True,python=out/'absent-python.exe',expected=6,error='worker_start_failed')
    run(install,label+'-missing-torch',[],science=True,python=base_runtime,expected=6,error='runtime_unavailable')
    worker=install/'share/breakwater/science/worker.py';origins=out/(label+'-origins.json')
    bootstrap='''import json,pathlib,runpy,sys
worker,origins=pathlib.Path(sys.argv[1]),pathlib.Path(sys.argv[2])
sys.path.insert(0,str(worker.parent))
try:runpy.run_path(str(worker),run_name="__main__")
finally:origins.write_text(json.dumps({n:str(pathlib.Path(m.__file__).resolve()) for n,m in sys.modules.copy().items() if n in ("worker_storage","storage_grid","breakwater_science","breakwater_science.rollout")},indent=2))
'''
    request=json.loads((root/'contracts/science/examples/storage-rollout-request.json').read_text())
    request['payload']['network']=archive['snapshot']['network']
    request['payload']['horizon_h']=3
    request['payload']['step_h']=0.025
    request['payload']['derivatives']=True
    request['payload']['controls']=[{'control_id':'source:source_a:curtail','value':10},{'control_id':'route:bypass:flow','value':20}]
    messages=[dict(protocol_version=1,request_id='before',method='capabilities',payload={'capability_version':1}),request,
        dict(protocol_version=1,request_id='after',method='capabilities',payload={'capability_version':1})]
    p=subprocess.run([str(runtime),'-I','-u','-c',bootstrap,str(worker),str(origins)],cwd=out,env=env,
        input=b''.join((json.dumps(m)+'\n').encode() for m in messages),capture_output=True,timeout=60)
    (out/(label+'-exchange.ndjson')).write_bytes(p.stdout)
    assert p.returncode==0 and not p.stderr,(p.returncode,p.stderr)
    responses=[json.loads(line) for line in p.stdout.splitlines()]
    assert len(responses)==3 and all(r['status']=='completed' for r in responses)
    verified=lambda r:next(m['execution_verified'] for m in r['result']['methods'] if m['name']=='storage_rollout')
    assert not verified(responses[0]) and verified(responses[-1])
    paths=json.loads(origins.read_text())
    assert len(paths)==4 and all(Path(p).is_relative_to(worker.parent.resolve()) for p in paths.values())
(out/'sqlite-and-history.json').write_text(json.dumps(db_results,indent=2)+'\n')
(out/'installed-hashes.json').write_text(json.dumps({str(p.relative_to(root/'out')):hashlib.sha256(p.read_bytes()).hexdigest()
    for install in [local,relocated] for p in install.rglob('*') if p.is_file() and '__pycache__' not in p.parts},indent=2)+'\n')
(out/'summary.json').write_text(json.dumps(dict(status='passed',app_executions=len(results),real_worker_requests=6,
    source_export_unavailable=not source.exists(),both_origins_installed=True,history=db_results),indent=2)+'\n')
print('PASS:',len(results),'installed app executions; exact selected sensitivities/history; both installed module origins; SQLite integrity')

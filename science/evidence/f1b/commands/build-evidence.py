"""Capture F1b measured evidence; never changes an existing execution attempt."""
from pathlib import Path
import copy,hashlib,json,re,shutil,subprocess
from datetime import datetime,timezone

ROOT=Path.cwd(); OUT=ROOT/'out/f1b'; E=ROOT/'science/evidence/f1b'
def read(p):return json.loads(Path(p).read_bytes())
def digest(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def save(p,v):
    p=Path(p);p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes((json.dumps(v,indent=2,allow_nan=False)+'\n').encode())
def cp(src,dest):
    dest=E/dest;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(src,dest)
def git(*args):return subprocess.check_output(['git',*args],text=True).strip()
def compact(value):
    if isinstance(value,dict):return {k:compact(v) for k,v in value.items() if k not in ('per_frame','original_indices')}
    if isinstance(value,list):return [compact(v) for v in value]
    return value
def fmt(v):return 'unresolved' if v is None else f'{v:.9g}'
reports={v:read(OUT/f'attempt-{n}-{v}-final/study/report.json') for v,n in [('normal','11'),('asan','12')]}
normal=reports['normal']; final_revision=git('rev-parse','HEAD')
assert all(r['status']=='completed' and r['comparison']['passed_gate_count']==58 and r['comparison']['gate_count']==63 for r in reports.values())
for name in ('baseline','candidate'):
    assert reports['normal'][name+'_execution']['data_sha256']==reports['asan'][name+'_execution']['data_sha256']
assert normal['baseline_execution']['data_sha256']=='14a086a64342931ea95a6c1d53f67d63e679c34720dc1d82cedb0aada5981477'
assert reports['normal']['candidate']['cases']==reports['asan']['candidate']['cases']
assert reports['normal']['comparison']==reports['asan']['comparison']
assert all(digest(ROOT/p)==h for p,h in normal['source_hashes'].items())
old_checks=read(ROOT/'science/evidence/f1/SHA256SUMS.json')
assert all(digest(ROOT/p)==h for p,h in old_checks.items())
pylog=(OUT/'attempt-13-python-final/tests.log').read_text(encoding='utf-8-sig')
pymatch=re.search(r'Ran (\d+) tests in ([0-9.]+)s',pylog)
assert pymatch and int(pymatch[1])==225 and '\nOK\n' in pylog and 'skipped=' not in pylog
assert read(OUT/'attempt-13-python-final/execution.json')['exit']==0
assert not E.exists(),'Use a fresh evidence export; existing evidence must not be overwritten'
E.mkdir(parents=True)
source_paths=git('diff-tree','--no-commit-id','--name-only','-r',final_revision).splitlines()
manifest=dict(baseline='487d80dd54df7d7b6567dfed1170dea9a5e59cd7',declaration_commit=git('rev-parse','HEAD^'),
    verified_code_commit=final_revision,verified_code_tree=git('rev-parse','HEAD^{tree}'),
    captured_utc=datetime.now(timezone.utc).isoformat(),source_hashes=normal['source_hashes'],
    implementation_git_blobs={p:git('rev-parse','HEAD:'+p) for p in source_paths},
    preserved_f1_hash_entries=len(old_checks),executables={},reports={})
for variant,build_attempt,study_attempt,legacy_attempt in (
    ('normal','attempt-01-normal','attempt-11-normal-final','attempt-07-normal-legacy'),
    ('asan','attempt-02-asan','attempt-12-asan-final','attempt-08-asan-legacy')):
    build=OUT/build_attempt;study=OUT/study_attempt;legacy=OUT/legacy_attempt
    registration=read(build/'registered-tests.json');assert len(registration['tests'])==9
    assert read(build/'execution.json')['ctest_exit']==0
    for p in build.iterdir():
        if p.is_file():cp(p,f'{variant}/native/{p.name}')
    for p in ('CMakeCache.txt','build.ninja','CMakeFiles/rules.ninja'):
        cp(OUT/variant/p,f'{variant}/build/{Path(p).name}')
    for p in (study/'study').iterdir():
        if p.suffix in ('.json','.md','.txt') and p.name not in ('baseline-reference.json','candidate-reference.json','report.json'):
            cp(p,f'{variant}/study/{p.name}')
    cp(study/'execution.json',f'{variant}/study/execution.json');cp(study/'study.log',f'{variant}/study/process.log')
    full=study/'study/report.json'
    save(E/f'{variant}/study/report-compact.json',dict(full_report_sha256=digest(full),full_report_bytes=full.stat().st_size,
        retained_full_report=str(full.relative_to(ROOT)),report=compact(reports[variant])))
    manifest['reports'][variant]=dict(full_report_sha256=digest(full),full_report_bytes=full.stat().st_size)
    for exe in sorted((OUT/variant/'engine').glob('*.exe')):
        manifest['executables'][str(exe.relative_to(ROOT))]=dict(sha256=digest(exe),bytes=exe.stat().st_size)
    cp(legacy/'executions.json',f'{variant}/legacy/executions.json')
    for family in ('refined','coarse'):
        r=read(legacy/f'a9-{family}/report.json')
        assert r['case_count']==45 and r['gate_count']==580
        assert len(r['failed_gates'])==(0 if family=='refined' else 1)
        if family=='coarse':assert r['failed_gates'][0]['name']=='scheduled_inlet.space.refinement_order'
        save(E/f'{variant}/legacy/a9-{family}-compact.json',dict(full_report_sha256=digest(legacy/f'a9-{family}/report.json'),report=compact(r)))
        cp(legacy/f'a9-{family}.log',f'{variant}/legacy/a9-{family}.log')
    assert all(v['byte_identical'] for v in read(legacy/'executions.json'))
for file in ('tests.log','execution.json','runtime.json'):cp(OUT/'attempt-13-python-final'/file,'python/'+file)
for file in (OUT/'attempt-09-reference-regression').rglob('*'):
    if file.is_file():cp(file,'failed-reference-regression/'+str(file.relative_to(OUT/'attempt-09-reference-regression')))
for script in ('run-build.ps1','run-study.ps1','run-legacy.ps1','build-evidence.py'):cp(OUT/script,'commands/'+script)
cp(ROOT/'science/f1b_front_comparison_config.json','declared-config.json')
cp(ROOT/'science/FRONT_COMPARISON_DECLARATION.md','declaration.md')
attempts=[]
for folder in sorted(OUT.glob('attempt-*')):
    for file in sorted(folder.rglob('*')):
        if file.is_file():attempts.append(dict(path=file.relative_to(ROOT).as_posix(),bytes=file.stat().st_size,sha256=digest(file)))
save(E/'retained-attempt-files.json',attempts)
save(E/'manifest.json',manifest)
old={c['id']:c for c in normal['baseline']['cases']};new={c['id']:c for c in normal['candidate']['cases']}
ids=['front_smooth.space.n512.cfl0.02','front_box.space.n512.cfl0.02','scheduled_inlet.extended_space.n512.cfl0.1']
summary=dict(status='experiment_completed_synthetic_targets_unmet',targets_passed=58,targets_total=63,
    production_qualification='not_established',python_tests=225,python_skips=0,new_python_tests=39,
    native_ctests_per_build=9,new_native_checks=21,asan_reports=0,
    native_existing_counts=dict(storage=22,search=22,control_influence=15,transport=25),
    cases_per_build=dict(baseline=22,candidate=28),baseline_data_sha256=normal['baseline_execution']['data_sha256'],
    candidate_data_sha256=normal['candidate_execution']['data_sha256'],normal_asan_native_data_identical=True,
    a9_refined=dict(cases=45,gates=580,failed=0),a9_coarse=dict(cases=45,gates=580,failed=1),
    original_f1_hash_entries_preserved=len(old_checks),
    candidate_maximum_reference_disagreement_mg_l=max(c['reference']['crosscheck_disagreement_mg_l'] for c in new.values()),
    candidate_maximum_reference_uncertainty_mg_l=max(c['reference']['uncertainty_estimate_mg_l'] for c in new.values()),
    candidate_maximum_signed_mass_residual_mg=max(c['maximum_signed_mass_residual_mg'] for c in new.values()),
    candidate_maximum_cell_stage_updates=max(c['execution']['cell_stage_updates'] for c in new.values()),
    failed_targets=[g for g in normal['comparison']['gates'] if not g['passed']])
save(E/'summary.json',summary)
lines=['# F1b: experimental front/outlet accuracy comparison','',
    '**Experiment completed; 58/63 synthetic targets met. Accuracy remains unqualified.**',
    'The private MC/SSPRK2 candidate reduces field and outlet errors substantially, but still misses the extended inlet\'s observed 0.9 mg/L crossing and fails the peak budget. The production solver remains unchanged.','',
    '## Revisions, declaration and preservation','',
    f'- Baseline: `{manifest["baseline"]}` (merged PR #28).',
    f'- Pre-execution declaration: `{manifest["declaration_commit"]}`; implementation/tests: `{final_revision}`.',
    f'- Verified code tree: `{manifest["verified_code_tree"]}`.',
    '- [Pinned declaration](FRONT_COMPARISON_DECLARATION.md) and [configuration](f1b_front_comparison_config.json) were committed before candidate execution. No target or existing threshold changed.',
    f'- All **{len(old_checks)} original F1 checksum entries** are unchanged. Production transport source/public headers, existing A9/F1 exporter, reference modules, schemas, pins and proofs are unchanged.',
    '- Work used a new isolated `codex/person-a-front-comparator` worktree. The root checkout, other branches and uncommitted work were preserved. Local continuation material is excluded.','',
    '## Candidate, supported domain and observables','',
    'The uninstalled private native target uses MC-limited piecewise-linear reconstruction and SSPRK(2,2), with zero endpoint slopes and a prescribed inlet. Its identity is `fvm-muscl-mc-ssprk2-experimental-0.1.0`. The production method remains `fvm-upwind-euler-0.1.0`.',
    'Interior slopes are `minmod(2*left_difference, (left_difference+right_difference)/2, 2*right_difference)`. Both stages use the current flow interval, even when their endpoint is a flow knot. Inlet data use stage times. The signed ledger uses the same half-weighted stage fluxes as the field update.',
    'Only nonnegative-flow pure advection with prescribed inlet is supported: dispersion, decay and source must be zero. Requested CFL must be in `(0,0.5]`; every step stops at forcing/output events. Invalid/unsupported data, nonfinite arithmetic, clock failure and exhausted resources are rejected. No clipping or tail renormalization occurs.',
    'The first and last cells retain first-order closure. The exported outlet observable remains the last cell average proxy. Its error against the continuum last-cell average is measured separately from error against the physical outlet point value. During zero flow the discharge observation is unavailable.',
    'All 22 F1 physical configurations, initial cell averages, CFL settings and output clocks are paired exactly. Six additional N512 candidate runs use pulse CFL .08/.04 alongside .02 and extended-inlet .4/.2 alongside .1. All cases use the original 1 m pipe, 0.2 m2 area and forcing/horizons. Each accepted candidate step consumes two cell-stage updates per cell; existing ceilings are unchanged.','',
    '## Field and outlet measurements','',
    'Field mean-L1/RMS are spatial norms at each saved output. The table gives the largest such norm over the full run; maximum errors may occur at different frames. Outlet mean/RMS use available saved discharge samples. Full per-frame data, terminal errors, signed peak/last-cell/point errors and ledgers are retained in raw reports.','',
    '| N512 passage | Method | Worst mean-L1 mg/L | Worst RMS mg/L | Worst maximum mg/L | Outlet point maximum mg/L |',
    '|---|---|---:|---:|---:|---:|']
for identity in ids:
    for method,rows in [('Baseline',old),('Candidate',new)]:
        c=rows[identity];e=c['worst_continuum_error']
        lines.append(f'| {c["regime"]} | {method} | {fmt(e["mean_l1_mg_l"])} | {fmt(e["rms_mg_l"])} | {fmt(e["maximum_mg_l"])} | {fmt(c["outlet"]["point_error"]["maximum_mg_l"])} |')
lines+=['','All six 50%-reduction targets and all 44 paired non-regression targets pass. The box maximum field error increases from 0.493526 to 0.512500 mg/L while mean/RMS improve. Bounded concentrations and good mass balance do not establish pointwise/front accuracy.','',
    '| Candidate N512 passage | Terminal mean-L1 mg/L | Terminal RMS mg/L | Terminal maximum mg/L | Outlet sample mean abs. error mg/L | Outlet sample RMS error mg/L | Maximum last-cell-average error mg/L |',
    '|---|---:|---:|---:|---:|---:|---:|']
for identity in ids:
    c=new[identity];e=c['terminal_continuum_error'];o=c['outlet']
    lines.append(f'| {c["regime"]} | {fmt(e["mean_l1_mg_l"])} | {fmt(e["rms_mg_l"])} | {fmt(e["maximum_mg_l"])} | {fmt(o["point_error"]["mean_l1_mg_l"])} | {fmt(o["point_error"]["rms_mg_l"])} | {fmt(o["maximum_last_cell_error_mg_l"])} |')
lines+=['','The N512 extended inlet peak improves from **0.610813894 to 0.834465965 mg/L**. The physical reference peak on the same saved clock is **0.950555556 mg/L**, and its continuous supremum is **1 mg/L**. The candidate\'s same-clock peak error is **0.116089591 mg/L**, exceeding the unchanged synthetic 0.10 target. The reference sampling gap of 0.049444444 mg/L remains separate. No unsaved native peak is inferred.',
    'The legacy scheduled-inlet window ends at 0.6 h before physical outlet arrival; it supplies no outlet passage qualification. Its candidate N512 worst field mean-L1/RMS/maximum are 0.003620550/0.023762828/0.285262871 mg/L.','',
    '## Threshold timing and observation sampling','',
    'The decision gates use stride 1 and both bracket endpoints, with a 0.025 h budget. Crossings are observed brackets, never interpolated exact times. Counts include repeated upward events; missing crossings remain explicit.','',
    '| N512 passage | Threshold mg/L | Continuous first h | Baseline first bracket h | Candidate first bracket h | Reference/candidate counts |',
    '|---|---:|---:|---|---|---|']
for identity in ids:
    a,b=old[identity],new[identity]
    for oa,ob in zip([v for v in a['observations'] if v['stride']==1],[v for v in b['observations'] if v['stride']==1]):
        bracket=lambda o:'not observed' if o['native']['first_upward_bracket_h'] is None else '['+', '.join(fmt(v) for v in o['native']['first_upward_bracket_h'])+']'
        lines.append(f'| {b["regime"]} | {ob["threshold_mg_l"]} | {fmt(ob["continuum"]["first_upward_h"])} | {bracket(oa)} | {bracket(ob)} | {ob["continuum"]["upward_crossing_count"]}/{ob["native"]["upward_crossing_count"]} |')
lines+=['','The candidate recovers both extended-inlet 0.5 mg/L events. It still has no observed 0.9 crossing. Box 0.1/0.9 first-bracket endpoint errors are about 0.0375 h, exceeding 0.025 h. All stride-2/4 views retain original indices and the endpoint from the same master trajectory; their outcomes are in the compact/full reports.','',
    '## Refinement, reference uncertainty and mass','',
    'Continuum cell averages, physical outlet characteristics and ledgers come from declared physical inputs independently of native arrays. Split 32/64-point quadrature cross-checks every saved continuum frame. Candidate analysis uses no upwind Poisson kernel. The unchanged baseline analysis retains its independent semidiscrete oracle.',
    f'Maximum candidate reference disagreement is **{fmt(summary["candidate_maximum_reference_disagreement_mg_l"])} mg/L** (rejection limit 1e-10); maximum uncertainty estimate is **{fmt(summary["candidate_maximum_reference_uncertainty_mg_l"])} mg/L**. These are numerical estimates, not interval enclosures. Ratios/orders near ten times the larger paired uncertainty are unresolved.',
    f'Maximum independently reconstructed candidate signed mass residual is **{fmt(summary["candidate_maximum_signed_mass_residual_mg"])} mg**. Maximum candidate work is **{summary["candidate_maximum_cell_stage_updates"]:,} cell-stage updates**, below 50,000,000. The largest saved field has 164,352 values, below 4,000,000.',
    'Candidate spatial refinements are explicitly coupled space/time. Time-step differences below are empirical sensitivity; they neither furnish an exact temporal/spatial split nor certify the remaining error.','',
    '| Candidate family/window | Coupled mean-L1 refinement orders |', '|---|---|']
for row in normal['candidate']['coupled_refinement']:
    lines.append(f'| {row["regime"]}/{row["study"]} | '+', '.join(fmt(o['mean_l1_mg_l']) for o in row['observed_orders'])+' |')
lines+=['','| Time-sensitivity study | Mean-L1 difference orders | Finest pair worst mean-L1 difference mg/L |','|---|---|---:|']
for row in normal['candidate']['time_sensitivity']:
    lines.append(f'| {row["case_ids"][0]} | '+', '.join(fmt(o['mean_l1_mg_l']) for o in row['observed_difference_orders'])+f' | {fmt(row["differences"][-1]["worst_difference"]["mean_l1_mg_l"])} |')
lines+=['','The box maximum-error refinement orders remain negative; smooth mean-error behavior does not imply uniform front accuracy. F1\'s six original failed time-contamination targets remain preserved with their original reference and method.','',
    '## Executed verification','',
    '| Check | Observed result |','|---|---|',
    '| Normal headless CTests | 9/9; 8.50 s |','| MSVC ASan headless CTests | 9/9; 65.89 s; zero sanitizer reports |',
    '| Individual native checks, each build | Storage 22; search 22; influence 15; original transport 25; candidate 21 |',
    f'| Full final Python discovery | 225/225; {pymatch[2]} s; zero skips; PhysicsNeMo available |',
    '| New Python checks | 39/39 |','| Actual study, each build | Baseline 22 cases; candidate 28; 58/63 targets; intentional exit 1 |',
    '| Refined A9, each build | 45 cases / 580 gates passed |','| Original coarse A9, each build | 579/580; original scheduled-inlet refinement failure retained |',
    '| Native compatibility | Both A9 modes byte-identical to retained PR #28 outputs; F1 raw SHA unchanged |',
    '| Normal/ASan comparison | Baseline and candidate native data byte-identical; candidate measurements/decisions equal |','',
    'Native tests include fixed hand-calculated two-cell and interior SSP updates, stage-time inlet quadrature, flow-knot pauses, shared-face cancellation, litre units, constant/zero/sharp/seeded profiles, input immutability, nonfinite/unsupported cases, CFL equality and actual exact output/work limits. Parser tests reject corrupted fields/times/identities, forged stage counts and inconsistent ledgers. Tests distinguish target failure from integrity failure.',
    'The original coarse orders remain 0.2765063820077894 / 0.20370157974971287 against minimum 0.25. No other A9 gate fails.',
    'Toolchain: Windows x64, MSVC 19.51.36260.0 / VC 14.51.36231, SDK 10.0.26100.0; CMake 4.3.1-msvc1, Ninja 1.13.2. Builds use C++20, RelWithDebInfo `/O2 /Ob1 /DNDEBUG /Z7`, `/fp:precise`, `/W4 /permissive- /EHsc /utf-8`, `/debug /INCREMENTAL:NO`; ASan adds `/fsanitize=address`. Exact generated flags and registration are captured.',
    'Pinned runtime: Python 3.12.13, NumPy 2.5.4, Torch 2.15.0+cpu, PhysicsNeMo 2.2.2 and jsonschema 4.25.1. Interpreter and installed-module origins are captured. Prebuilt Python/Torch/vendor libraries are outside established ASan coverage. Elapsed times are shared-machine observations, not benchmarks.',
    'No new desktop/installed-app, hosted-model, 1,800-update training or Lean run is claimed. Public application/worker interfaces and approval behavior have no implementation change.','',
    '## Failed attempts and reproducibility','',
    'The first normal/ASan studies (attempts 04/06) completed at 58/63. Review found that the initial candidate reference checker unnecessarily also evaluated the legacy upwind Poisson kernel. Attempt 09 adds a regression and retains its failing 39-test run, source snapshot and traceback. The correction adds a continuum-only candidate cross-check; attempt 10 passes all 39 tests. Final attempts 11/12 repeat actual studies and attempt 13 repeats all 225 Python tests. Native solver bytes, raw fields, thresholds and measured decisions are unchanged. All earlier attempts remain retained.',
    '[Evidence summary](evidence/f1b/summary.json), [source/executable manifest](evidence/f1b/manifest.json), compact normal/ASan reports, native/Python logs and [retained-attempt index](evidence/f1b/retained-attempt-files.json) record commands, lengths and SHA-256 values. Raw fields/full per-frame reports remain under ignored `out/f1b/`; compact copies omit per-frame arrays and long index lists but retain their full report hashes, extrema, uncertainty, every gate and crossing bracket.',
    'From an x64 Visual Studio Developer PowerShell at the worktree root:', '', '```powershell',
    "$pythonExe = 'C:/Projects/Breakwater/.worktrees/person-a-engine/science/.venv/Scripts/python.exe'",
    "cmake -S . -B out/f1b/normal -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_MSVC_DEBUG_INFORMATION_FORMAT=Embedded -DBUILD_TESTING=ON -DBREAKWATER_BUILD_DESKTOP=OFF -DBREAKWATER_ENABLE_ASAN=OFF '-DCMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO=/debug /INCREMENTAL:NO'",
    'cmake --build out/f1b/normal --parallel 4', 'ctest --test-dir out/f1b/normal --output-on-failure',
    '& $pythonExe science/compare_front_accuracy.py --native out/f1b/normal/engine/breakwater_front_comparison_reference.exe --baseline-native out/f1b/normal/engine/breakwater_transport_qualification_reference.exe --config science/f1b_front_comparison_config.json --output out/f1b/reproduced-normal-01',
    '& $pythonExe -m unittest discover -s science/tests -v', '```', '',
    'Use a fresh output directory. Exit 0 means integrity and all synthetic targets passed; 1 means a completed experiment with unmet targets; 2 means execution/input/reference/integrity failure. Repeat the configure in a separate `asan` directory with ASan ON and its runtime on PATH. [Captured command scripts](evidence/f1b/commands/run-study.ps1) and execution JSON contain the actual normal/ASan/A9 arguments; choose new attempt names.','',
    'Native output SHA-256 values:', '', '| Method | SHA-256 |', '|---|---|',
    f'| Baseline, both builds | `{normal["baseline_execution"]["data_sha256"]}` |',
    f'| Candidate, both builds | `{normal["candidate_execution"]["data_sha256"]}` |','',
    '## Proof mapping and Person B handoff','',
    'The prior 54 audited propositions, foundational allowlist and pins are unchanged. A10a shared-face cancellation motivates the weighted ledger, but this batch supplies no formal mapping of the new two-stage stencil. A10b\'s original Euler nonnegativity coefficients do not cover MC reconstruction or SSP stages. The declaration provides the conditional exact-real argument; native tests, ASan and accuracy measurements remain separate evidence.',
    'B can report an experimental improvement on these declared pure-advection cases, with 58/63 synthetic targets met. Production transport, history provenance, contracts and approval remain unchanged. No candidate is selectable through the application or worker.',
    'A further bounded experiment is justified before promotion: isolate endpoint closure versus interior limiter smearing using the same independent front/outlet cases and explicit positivity/ledger checks. These measurements do not identify the first-order boundary as the sole cause. Preserve the missed 0.9 event, box timing failures and discontinuity maximum-error limitation. Any production adoption needs separately agreed application error budgets and B-owned integration/provenance review.',
    'G2/G3 mappings and detection, RBMs, solver promotion, new proofs and application release acceptance remain subsequent increments. Shared STATUS and delivery documentation are left for B to integrate from this A-owned handoff.','']
report=ROOT/'science/FRONT_ACCURACY_COMPARISON.md';report.write_bytes('\n'.join(lines).encode())
progress=ROOT/'science/PROGRESS.md';previous=progress.read_bytes()
heading=b'# Person A science evidence\r\n'
assert previous.startswith(heading)
entry=f'''\n\n## F1b experimental advection comparison — November 8, 2026\n\n[Comparison report](FRONT_ACCURACY_COMPARISON.md) completes the private MC/SSPRK2\nexperiment against frozen PR #28/F1. Each build executes 22 baseline and 28 candidate\ncases; normal/ASan raw fields are byte-identical. **58/63 synthetic targets pass**:\nall six field-improvement and 44 non-regression gates pass, but the extended inlet\npeaks at .834466 mg/L and still misses .9 mg/L; box .1/.9 brackets exceed .025 h.\nThe box maximum field error increases to .512500 mg/L. Accuracy remains unqualified.\n\nFresh native checks pass 9/9 normally and under MSVC ASan (21 new native checks),\nwith zero sanitizer reports. Final pinned Python discovery passes 225/225 without\nskips (39 new checks); PhysicsNeMo is available. Refined A9 remains 45/580 passed\nand the original coarse scheduled-inlet failure remains. All {len(old_checks)} original F1\nchecksum entries are preserved, including the six original time-contamination failures.\n\nB handoff: private experiment only; production solver, interfaces, pins and original\n54-proposition proof mapping are unchanged. Retain missed events and maximum-error\nlimits in presentation. A separately declared boundary/interior-smearing experiment\nis justified before any promotion; application budgets and B integration remain gates.\nThe report records the failed reference-check regression, correction and all attempts.\nEarlier milestone entries below preserve their original scope.\n'''.encode()
progress.write_bytes(heading+entry+previous[len(heading):])
checks={p.relative_to(ROOT).as_posix():digest(p) for p in sorted(E.rglob('*')) if p.is_file()}
for p in (report,ROOT/'science/f1b_front_comparison_config.json',ROOT/'science/FRONT_COMPARISON_DECLARATION.md'):
    checks[p.relative_to(ROOT).as_posix()]=digest(p)
save(E/'SHA256SUMS.json',checks)
print(json.dumps(dict(evidence_files=len(checks),retained_attempt_files=len(attempts),summary=summary),indent=2))

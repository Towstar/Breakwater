from pathlib import Path
import hashlib,json,subprocess
root=Path(__file__).resolve().parents[3]
folder=root/'science/evidence/pr26-reconciliation'
summary=json.loads((folder/'summary.json').read_text())
assert summary['status']=='passed' and summary['ctests_per_build']==28
status=root/'docs/STATUS.md'
text=status.read_text(encoding='utf-8')
suffix=text[text.index('## Combined verification'):]
prefix='''# Implementation status

## PR #26 reconciliation with current main — November 7, 2026

- [Reconciliation report](PR26_RECONCILIATION.md): merged `e057fa4` through
  `f504310`, preserving selected-control sensitivities, automatic scientific-history
  retention and immutable historical reopening. Application/contracts match main;
  exact-grid, disagreement, 50-cycle and post-exchange retirement regressions remain.
- Fresh complete normal/MSVC ASan builds each passed **28/28 CTests** (390.19 s /
  439.93 s), including live selected sensitivities and scientific-history rendering.
  Each build: worker **121**, controller **43**, science evidence **42**, zero
  failures/skips; QML lint passed and no sanitizer report was found. Prebuilt
  Qt/Python/Torch remain outside established ASan instrumentation coverage.
- Full Python: **127/127**, no skips, PhysicsNeMo available. Native/PyTorch/worker:
  **18 fixtures / 302 requests / 132 central differences**, unchanged tolerances.
- Corrected installed acceptance: **16 app executions** across local/relocated
  copies; actual selected derivatives, exact retained 121-sample native arrays,
  historical reopening without Python, installed module origins, explicit missing
  dependencies and SQLite integrity passed. An initial probe incorrectly expected
  historical marking in stored canonical data; the failed harness attempt is retained.
- No new interactive UI pass is claimed. Earlier native-dialog/cancellation gaps,
  coarse qualification failure and front-accuracy limitation remain explicit in the
  frozen combined report. No repeated PDE qualification/training/Lean run is claimed.
  The local continuation plan remains untracked and excluded from the PR.

'''
status.write_bytes((prefix+suffix).encode('utf-8'))
assert subprocess.check_output(['git','diff','e07bdf8','--name-only','--','science/evidence/combined-verification','docs/COMBINED_VERIFICATION.md'],cwd=root)==b''
production=subprocess.check_output(['git','diff','origin/main','--name-only','--','desktop','contracts','CMakeLists.txt','CMakePresets.json'],cwd=root).decode().splitlines()
assert all(path.startswith('desktop/tests/') for path in production),production
assert subprocess.check_output(['git','ls-files','*PLAN_LOCAL*','*LOCAL_PLAN*'],cwd=root)==b''
paths=[p for p in folder.rglob('*') if p.is_file() and p.name!='SHA256SUMS.json']
paths.extend([root/'docs/PR26_RECONCILIATION.md',status])
checksums={p.relative_to(root).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(paths)}
(folder/'SHA256SUMS.json').write_bytes((json.dumps(checksums,indent=2)+'\n').encode())
print(f'PASS: {len(checksums)} report/evidence checksums; original evidence unchanged; production matches main; local plan excluded')

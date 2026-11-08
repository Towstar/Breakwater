# Submission checkpoint — November 8, 2026

This local submission build combines Lucas's published desktop/search source
`8bc6237c5dee9e4a600c74fb44dac06ec3ac3b59`, his subsequently published PR #55
selector polish `7fda267`, Ethan's PR #53 committed-response increment and PR #54
sparse PhysicsNeMo tracer calibration. Lucas remains the main-branch integrator.
The final runtime freeze is `325a168b4356157d10e25783e88e7381a4d5e850`.
The actual calibration/trucking GUI workflow was qualified at `6c0c2a2`; the
subsequent selector-only change leaves its C++/Python/numerical source unchanged.

Run locally from PowerShell:

```powershell
& 'C:\Projects\Breakwater\out\submission-latest\Start-Breakwater.cmd'
```

That launcher now selects the final executable at
`C:\Projects\Breakwater\out\submission-final-latest\bin\breakwater.exe` and the existing
pinned scientific Python environment. Qt, the production workers and native EOG
provider are installed in the final bundle; Python is an existing local dependency.
The original baseline bundle was preserved so an already-open window is untouched.

## Actual verification

- Real desktop-to-PhysicsNeMo pipe suite: 16/16, zero skips. It checks a genuine fit,
  no input mutation before explicit application, and retirement of the old forecast
  and approval after applying the coefficient.
- Existing project regression: 13/13, zero skips.
- Final selector rebuild: all nine installed native UI gates passed in each of
  normal and compact sizes, covering actual forecast, picking, dragging, keyboard
  rejection, focus behavior and retirement of stale approval. The first hidden-
  window attempt timed out before exposure; its harness failure is retained and
  the successful visible-window reruns are separate records.
- Full integrated science: 459/459, zero skips, at pre-test-relocation source
  `25aa6147aed1dd405a326ac271a77cb4534459ac`. The product Python code is unchanged
  by the subsequent relocation. The final discovery layout has 456 default tests
  plus three explicitly invoked real-runtime calibration checks, separately passed.
- PR #53 retains normal and ASan native 14/14 suites, 43 new native checks per mode,
  focused Python 14/14 per mode, 400 generated exact native/reference comparisons,
  60 legacy comparisons and two actual stateful-plant suffix comparisons.
- The first installed build passed three real image/render runs. A real Detect
  events action retained 23 events, including two ongoing pump failures with
  displayed countdowns 2.055 h and 3.425 h. Its baseline source is recorded separately.
- The final installed UI actually fitted 0.2000/h using PhysicsNeMo 2.2.2 and four
  synthetic tracer observations, agreeing with the independent 0.2000/h estimate.
  Explicit application cleared the old selected run and disabled transport pending
  a new forecast. Further installed workflow evidence is recorded with its captures.

Portable test logs, source/binary hashes and genuine screenshots are in
`science/evidence/submission-checkpoint/`. Full local screenshot outputs are in
`C:\Projects\Breakwater\out\submission-ui-screens`.

## Implemented and remaining

Recorded EOG ingestion/rendering, causal detection, snapshot plan comparison,
bounded shared-fleet search and independent exact validation are implemented.
The separate editable workbench retains native storage/transport calculations.
PhysicsNeMo now has a concrete application role: experimental first-order tracer
decay estimation, independent fit gating, explicit parameter application and a fresh
native forecast requirement. It does not identify flow or dispersion or certify
physical chemistry. No hosted-model rehearsal was run for this rebuild; Lucas's
published earlier hosted evidence remains explicitly source-qualified.

The current trucking increment completes the **private committed-state response
evaluator and independent validator**. It supports occupied truck slots, pending
full loads, prior well stops and incremental future costs. It does not finish
an authenticated history-to-state compiler, commitment-aware optimization or
an evolving dispatch/replanning application mode. The public EOG planner remains
snapshot-based. Extended EOG simulation/replay application acceptance and a final
combined full desktop/ASan/hosted qualification remain outstanding. This checkpoint
is not completion of every criterion in the durable HackUTD goal.

User-facing Devpost prose and the requested science/enforcement handout are kept
outside tracked files, in ignored local output. The Devpost service was not edited.

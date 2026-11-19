# Native observed-state mapping

This increment implements the numerical and capture seams for the generic local
monitoring path. It is separate from EOG's recorded-data analysis. The desktop
monitoring page is not yet wired to a forecast/review workflow by this increment.

`bounded-observations-0.1.0` consumes validated G1 DTOs and has no Qt, Python,
transport or model dependency. It copies the immutable snapshot, computes fresh
operation-specific coverage and derived state, and returns a separate effective
network only when a volume forecast can be initialized. Neither output supplies
operating authority. The original design network/hash and commands are retained.

## Coverage and mathematics

- Every tank needs usable inventory or a declared level/volume calibration. Every
  source needs net measured delivery. Every route from a tank needs measured
  delivery; every pump also needs typed feedback. Pressure, temperature and
  concentration do not substitute for volume coverage.
- Inventory/flow identity calibration adds declared measurement and calibration
  absolute bounds. Level interpolation evaluates the center and both uncertainty
  endpoints, then adds the calibration's volume error bound. There is no
  extrapolation, clipping, statistical confidence claim or averaging of redundancy.
- Required good samples must be causal, fresh, within the coherent-skew policy and
  consistent with their units. Contradictory redundant intervals or pump feedback
  make the mapping unavailable. An additional usable feed-route meter must agree
  with source delivery within their combined absolute bounds.
- The primary reading and its original bound are retained when redundant readings
  agree. A usable redundant sensor can provide coverage when another is unusable.
  Conflicting observations remain raw evidence and cannot supply derived state.
- Negative, over-capacity and unrepresentable mapped states remain evidence and
  cannot initialize the native model. Finite future negative/overflow predictions
  are still displayed and validated by the existing engine; they are not clipped.
- The snapshot expiry is the earliest required sample expiry. Configuration,
  physical/numerical or acquisition-epoch changes retire current eligibility;
  label/marker-layout changes preserve semantic identity. Replay is historical and
  all G1 snapshots remain `actionable:false`.

The effective network starts from observed inventory and delivery. Pump
availability is held at its observed/design restriction; a flow command cannot
restore a failed or unknown pump. Net measured source delivery plus frozen
commanded curtailment supplies an explicitly **assumed** gross source potential.
Its curtailment maximum is limited to that potential in the effective copy. Future
delivery/potential constancy is a forecast assumption, not a measured guarantee.
The original design limits and commanded controls are unchanged.

The concentration helper supplies exact cell-average or overlap-weighted interval
averages over a declared native pipe profile. Unsupported point locations remain
unavailable. This helper alone does not connect chemistry sensors to a forecast or
establish a transport accuracy guarantee.

## Capture provenance and checks

The receiver freezes actual samples on its own worker thread at one receiver-clock
instant. Callers supply only the registered project context, numerical revision
and complete current command catalog. They cannot inject samples, plant truth or
alternate commands. Async generation/source checks reject cancelled or retired
captures before publication.

The four-file capture prototype was preserved before porting. Its patch SHA256 is
`be68019e4bcb3a523695bec444224f58866511d89e922e9c49c339dbd81ffc93`.
Its original worktree and uncommitted files were not modified. The port adds actual
native mapping and new verification rather than inheriting a qualification claim.

Run `engine.observations`, `desktop.observed_capture`, `desktop.telemetry_stream`,
`desktop.telemetry_lifecycle`, `desktop.telemetry_contracts` and
`contracts.telemetry` with the configured portfolio and portfolio-asan presets.
The native suites use analytical balance/calibration/spatial-support examples;
Qt cases validate the exact unchanged G1 projection and async retirement behavior.
See the increment evidence for actual results and source hashes.

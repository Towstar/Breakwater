# Native EOG response evaluation and one-shot provider

This A-owned increment adds `engine/include/breakwater/eog_response.h` and
`engine/src/eog_response.cpp`: a private computational C ABI/DLL using binary64
elapsed **seconds** throughout. It has no Qt, Python, JSON or model API dependency.
It does not call the hours-based E1 fixture. `science/native_eog.py` checks every
structure size, owns the input arrays and copies results before releasing them.

`science/eog_worker.py` is the separate one-shot NDJSON provider for the mutually
adopted exact public interface **72cadd1de7eed6666a60faff1d296e29c8f026ee**.
No public contract or desktop files are changed. Startup verifies normalized
LF/CRLF hashes of the five adopted schema/validator files. The default contract
path becomes usable when Lucas integrates that contract; an explicit trusted
`--contracts-root` supports isolated review/qualification before integration.
Neither a request nor a model selects executable code or schemas.

## Implemented behavior

- `detect` validates the request, verifies the three fixed captured files, loads
  their numerical semantics, rechecks loaded hashes against the actual manifest,
  and echoes the actual request bindings. Raw finite evidence, dataset freshness,
  historical cutoffs, uncertainty and unqualified inspection timing remain explicit.
- `evaluate_plan` compiles the complete current pump-failure set with known causal
  onset, current coherent measurements and pre-trip productive wells. Remaining
  repair seconds are `ops repair hours * 3600 - (cutoff - onset)`, with no
  round-trip through a displayed hours value. Unknown/expired horizons are unavailable.
- The captured facility meter defines total inflow. Well contributions are
  explicitly allocated in proportion to current measured well water. The largest
  contributor takes the finite residual. Raw readings are retained. Zero/nonzero
  disagreement or unrepresentable allocation is unavailable, not clipped.
- Full configured loads arrive at `call + 60*response_min + k*(60*cycle_min)`.
  Every proposed timestamp, load, cost, travel duration and final yard-return claim
  is independently recomputed against captured operations. No candidate is repaired.
  Simultaneous pickups require their combined full inventory. Jobs share physical
  slots per system; occupancy is half-open through final arrival plus a full cycle.
- Automatic shutdown precedes tied actions/repair. A crossing at repair is recorded
  with zero remaining automatic loss. A pickup must precede repair strictly.
  A full pickup after automatic shutdown can be admissible and costed, but never
  resumes production or changes the recorded shutdown. This follows the official
  statement that a late pickup does not prevent the trip, not a claim of prevention.
- Individual well stops last until their facility's repair. Voluntary/automatic
  lost revenue uses each productive well's union of stopped time, without double
  counting. Already-shut facilities retain observed inventory and pre-trip economics.
  Do-nothing automatic shutdown is a valid costed outcome.
- Complete trajectories include zero, exact repair endpoints, all well breakpoints,
  before/after pickup pairs and automatic crossings. No volume clipping, timestamp
  quantization, decimation or feasibility epsilon is used. Affine anchors survive
  zero-effect breakpoints. If rounded inventory and crossing order disagree, the
  evaluator returns numerical unavailability. Mass-balance residuals remain in the
  private native result rather than being silently balanced.
- Invalid candidate actions yield completed infeasible evaluations with violations
  and null cost. Missing capability/evidence, overflow/underflow or result limits
  yield explicit top-level unavailability. Invalid framing/requests cannot establish
  a binding: the process emits no JSON, a short generic stderr line and exit code 2.

## Supported scope and remaining work

The first provider supports all current failures together. Subset response contexts
and stateful prior dispatch commitments are not supported. Truck IDs are opaque
labels mapped to a bounded number of slots per system. Sequential delayed calls
are an explicit application extension in the accepted contract; the official
simple single-plan example calls its initial trucks at plan start.

`search_plans` and `validate_plan` explicitly return unavailable. The native
evaluation is not the independent validator. Fixed-rate forecasts stop at repair;
post-repair hysteresis, evolving simulation, long-run ledgers, desktop process/model
adapters, installed packaging and UI acceptance remain open. Detection 0.2.2 adds
private productive-well context; inspection rules are unchanged from frozen 0.2.1,
whose retained official timing/lifecycle gaps still apply.

Native bounds: 64 systems, 256 facilities, 128 wells/facility, 256 jobs/stops,
16,384 total trips, 8,192 trajectory points/facility and 262,144 total points.
The public byte/array limits can bind sooner. A valid pointer/buffer is a requirement
of the private in-process ABI; it is not a memory-safe parser for arbitrary pointers.

## Reproduction

Build with `scripts/build.ps1 -Preset headless -EmbeddedDebugInfo` and separately
`-Preset asan`. Install EOG schema dependencies with the existing Python 3.12
environment and `pip install --require-hashes -r science/requirements-eog.in`.
For isolated qualification set `BREAKWATER_EOG_NATIVE` to the **absolute** built
`breakwater_eog_response.dll` and `BREAKWATER_EOG_CONTRACTS` to the adopted
`contracts/eog` directory, then run:

```text
python -m unittest discover -s science/tests -p "test_*eog*.py" -v
python science/eog_worker.py --contracts-root ABSOLUTE_CONTRACT_ROOT --native-library ABSOLUTE_DLL
```

Supply exactly one request line on stdin; stdout is exactly one bound response
line. No answer key or original source-folder path is passed to the worker.
Optional native/contract science checks report explicit skips when startup paths
are absent; qualification must provide them and record the actual skip count.

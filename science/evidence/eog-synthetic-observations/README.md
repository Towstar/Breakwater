# Executed imported-model causal synthetic stream

Runtime **78338aa**, initial native ABI/mapping **d0c15f9**, complete offline
qualification **1947638**. Latest UI main 7418a2d is merged as be31979, preserving
tested A-owned source and Ninja build options. Public EOG wire remains 72cadd1;
native plant method remains 0.1.0, private BWES ABI 1.

| Check | Actual result |
|---|---|
| Final full CPU science | **399/399**, zero failures/skips, 51.537 s |
| Native normal | **13/13**, 22.09 s |
| Native MSVC ASan | **13/13**, 29.48 s; no sanitizer diagnostic |
| Native checks per mode | **28 ABI checks**, existing **57,498 synthetic checks** |
| Checked-DLL Python bridge | **8/8**, 0.137 s |
| Final checked synthetic/control tests | **14/14**, 0.308 s |
| Imported official model | **5 systems, 15 facilities, 195 channels, 56,355 generated samples**, one day |
| Altered unfamiliar model | **3 facilities, 39 channels, 11,271 generated samples**, one day |
| Generated pump failures | **6/6** detected, delays **32.875–264.875 s**, no unmatched pump/extra inspection events in these cases |
| File roundtrip | All observations, imported model parameters and detector results exactly preserved |
| Normal versus checked | Byte-identical current truth, detector records, selected samples, full receipts and generated file hashes |
| Controlled replay | Exact physical state/cost/fleet, observation fingerprint and run identity; busy truck persists past repair |

The imported cases' maximum absolute raw mass residuals are
5.684341886080801e-13 / 3.979039320256561e-13 bbl. Original official source hashes
are unchanged. Its model is imported from the three allowed files at upstream
135ba4c; its recorded week is not described as a recreated or predicted future.
The unfamiliar network changes IDs, capacities, prices, thresholds, fleet and well
rates. It is generated evidence, not hidden judging data.

`normal/` retains actual final summary, host-only receipts, truth, detections and
selected original samples. `checked/` retains actual checked summaries. The
verification record compares all normal/checked raw result bytes before retaining
one copy of identical payloads. `model-inputs/` contains compact generated network/
ops parameters; full CSVs stay in the local output folder with exact hashes and a
reproducible seed/source-bound recipe. Those compact folders are not complete
loadable datasets. Synthetic dataset IDs and output paths label their nature.

The controlled case supplies a dispatch at 900 s and shut-in at 1000.25 s, replays
their bounded receipt and preserves yard occupancy beyond repair. Cost is
9390.1683984375: 905 trucking plus 8485.1683984375 lost revenue. These are supplied
valid actions, not an optimizer result, independent plan-validation record or
operating approval. Extra query/sample trace counts do not change the physical
replay identity; all raw trace buffers still have explicit limits.

Tests verify copied native ownership, layout/ABI checks, safe close/reset,
callback reentry rejection, bounded trace writes, immutable causal views, future
truth changes, advance partition/reset, fractional controls, negative unclipped
readings and genuine stale/gap behavior. Receipt/seed/future schedules stay on the
host; analysis receives only `View`. Unobservable future oil/gas changes reject.
No Python physics fallback or new Python dependency is introduced.

The first offline CLI wrote successful UTF-8 payloads and summary, then failed
CP1252 console encoding for a Greek ID. The failure journal and initial summary
are retained; ASCII console JSON fixes the presentation without changing payload
IDs or numerical results. Earlier 397/398 science logs keep their original scope.
Expected negative F1 child tests print failure diagnostics after unittest's OK;
the final 399-method result has zero failures/skips. CPU PhysicsNeMo tests execute;
no GPU or instrumented Python/Torch claim is made. Native timings are qualification,
not optimized benchmarks.

Reproduction and exact native/CI/package dependencies are in
`science/EOG_SYNTHETIC_OBSERVATIONS.md`. The explicit qualifier reads no answer key;
an ignored sentinel in generated input folders verifies fixed-name loading. It
does not inject leak/degradation truth or establish their timeliness, physical
onset confidence, evolving planning, application controls, installation or final
rendered acceptance. Those required goal items remain open.

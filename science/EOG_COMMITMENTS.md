# Causal accepted controls and truck commitments

Private numerical method **eog-causal-commitments-0.1.0**, runtime
**621c28de7e67af89ab6474f558763da3523a17b5**. Public EOG stays exact 72cadd1;
native plant and observation/noise methods remain 0.1.0. No new public wire,
worker operation, native algorithm or operating authority is introduced.

`SyntheticStream.control_history()` projects immutable accepted control facts:
fractional elapsed time, imported facility/well ID, explicit system-local truck
ordinal and trip count. It excludes seed, future incidents/forcing, full scenario
fingerprint and rejection diagnostics. Rejected attempts have no physical effect
and do not enter this projection. Future accepted commands are excluded from an
explicit older cutoff. The full future-bearing `provenance()` receipt remains
host/offline only. Default/current history requires a supported native state;
strictly earlier verified prefixes remain queryable after a later terminal state.
Cancellation does not turn an unsupported current state into a healthy result.

The history binds the imported model fingerprint and an exact epoch/fractional
clock. `reconstruct(model, view, history)` independently derives each truck's
reserved return and scheduled full pickups, without native/file/label access.
The observation cutoff must equal epoch plus floor(elapsed); physical/control
events retain their original binary64 seconds. The last return releases the
truck under a half-open interval: a new job may start exactly at return, but
not one representable time before it. Each new job starts at its own facility's
yard response time. Repair and well commands do not release a busy truck.

```python
from eog_commitments import reconstruct
known_controls = stream.control_history()  # host-side projection
causal_observations = stream.view()
calendar = reconstruct(model, causal_observations, known_controls)
# Pass only these immutable projections to analysis, not stream/provenance/native.
```

The caller must authenticate a **complete** accepted prefix from an explicitly
initially free fleet. These Python types do not authenticate a live session or
grant approval/dispatch capability. Imported/reopened history must not acquire
operating authority. Repeated numerical identities after reset describe
repeatability; the application must separately retire old session/job authority.
Partial/unknown initial fleet, foreign IDs/model/catalog/cutoff, overlapping
accepted jobs, unsupported arithmetic and exhausted bounds fail explicitly.
Limits: 31-day elapsed clock, 32,768 accepted controls, 4,095 trips/job and 16,384
retained scheduled pickups. A larger host journal is not silently truncated.

Scheduled pickups are commitments, **not evidence of actual inventory removal,
execution or charged costs**. The ledger does not establish process health,
failure/repair onset, current inventory, a valid future plan or an optimum.
Observation freshness/ambiguity and native/independent inventory-cost validation
remain separate requirements. Snapshot search still assumes no prior dispatch;
do not feed this calendar to it and assume busy slots/pickups were accounted for.

## Actual qualification

- Full pinned CPU science: **439/439**, zero failures/skips, **85.762s**.
- Focused checks: **14/14 normal**, 0.430s; **14/14 with inherited ASan DLL**,
  0.855s. Includes future/prefix invariance, rejected-action exclusion, exact/ULP
  return ties, sequential jobs, independent systems, model/catalog/cutoff binding,
  cancellation, immutability, bounds and no native access in reconstruction.
- Seed 2026110810: 32 varied synthetic configurations, 64 accepted dispatches,
  128 calendar snapshots and 256 exact truck identity/busy/return comparisons.
- Actual ten-stage normal/ASan runs match independent calendars against native
  trucks before/after repairs, a busy rejection, sequential dispatch, final
  return, completion and reset. All 12 complete model/history/ledger/native/
  observation/host-receipt payloads are byte-identical between modes; summaries
  have identical checks and legitimately different binary hashes.

The initial fixture wrongly expected every repaired pump to be running; healthy
off hysteresis is valid. Its failure remains. A later historical-cancellation
regression exposed the first implementation's future-state-dependent rejection;
the corrected verified-prefix boundary and negative test are retained. An earlier
438-case passing run is scoped to its earlier source, not substituted for the
final run. No new native build, broad sanitizer/proof, installed/UI/model or
physical/held-out acceptance is claimed. Evidence is in
`science/evidence/eog-commitments/`.

## Integration and next work

B needs the new `eog_commitments.py` plus changed `eog_synthetic.py` alongside the
existing plant bridge/runtime; `qualify_eog_commitments.py` is offline infrastructure.
No Python dependency is added. The B-owned additive simulation request/result
boundary still needs publication, A numerical review and exact mutual adoption
before application mapping. It must distinguish the fractional decision/control
clock from the integer observation cutoff and authenticate live control history.

Next numerical step: explicit occupied-slot and committed-pickup context in the
response evaluator and independently coded validator, then commitment-aware
planning. Do not reinterpret the recorded-now public Plan clock or reset the
fleet to free. Joined replay/counterfactual/extended mode controls, installed/model
checks and retained-workbench/final UI acceptance remain required by the goal.

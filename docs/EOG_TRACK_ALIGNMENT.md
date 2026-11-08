# EOG challenge alignment and next increment

Reviewed November 8, 2026 (America/Chicago), after the user supplied the official
[challenge repository](https://github.com/nihaal419/HackUTD-ZeroDay).
This records requirements and gaps; it is not evidence that an EOG adapter,
detector or optimizer has been implemented.

## Source requirements

The [challenge README](https://github.com/nihaal419/HackUTD-ZeroDay/blob/main/README.md)
asks for an operator console that diagnoses sensor-observed problems, estimates
shutdown time and recommends economical action. Its judging network changes:
equipment IDs, operating values and topology must come from supplied files.
Production input is restricted to `network.json`, `ops.json` and `telemetry.csv`.
Practice answer keys belong only in an independent evaluation harness, never the
application, model prompts or detector input. Counterfactual plans leave recorded
observations intact. Leaks and degradation call for inspection; pump failure needs
a countdown and response plan. The opening example is an evaluation case, not
production logic or a hardcoded plan.

The [specification](https://github.com/nihaal419/HackUTD-ZeroDay/blob/main/SPEC.md)
defines five-minute CSV samples and treats the last `ts_unix` as the current time.
It uses barrels, daily flow rates, psi, tank percentages and per-well revenue.
Pumps normally cycle with hysteresis; stopped is not automatically failed.
Shutdown shuts wells in before a spill. Truck removals occur at discrete arrivals,
with first-response and repeat-cycle times, flat trip prices and fleet sharing.
Trips must fit inventory and repair deadlines. Well shutdown can be delayed or
combined with trucks; doing nothing has a revenue consequence. Planning from a
snapshot uses remaining repair time and jointly allocates trucks to concurrent
failures. Alert ordering follows urgency, then revenue exposure.

The [history guide](https://github.com/nihaal419/HackUTD-ZeroDay/blob/main/history/README.md)
distinguishes onset-time reference plans from snapshot-time plans. Its cuts share
the same underlying week, so they are not independent held-out datasets. The mock
folder has no answers; the later judging folder contains a different network.
[Worked rows](https://github.com/nihaal419/HackUTD-ZeroDay/blob/main/examples.md)
illustrate normal cycling, noisy pressures, shut-in and truck schedules.

## Consequences for Breakwater

G2's typed local ingestion, trace plots and replay are useful foundations. The
current native constant-flow demonstration and G1 schemas do not yet implement the
challenge's CSV semantics, well economics, automatic shutdown or shared discrete
truck scheduling. Synthetic demonstration results must not be called EOG results.

After this G2 increment, prioritize a separate **EOG dataset adapter and evaluation
boundary**, then A-reviewed detection and planning:

1. B: dataset-folder chooser, bounded CSV parsing, stable asset/metric identities,
   original-unit/source-row provenance and replay using the dataset clock. Read
   exactly the three allowed inputs. Test renamed/reordered/custom networks.
2. A/B: review a compatible contract extension for bbl/bpd/psi, percentage-to-volume
   observations and pump `0/1/2` semantics. Percentage is not a physical level in
   metres. Do not mislabel stops as failures or silently force new units through
   G1's narrower conversion rules. Five-minute sampling needs an explicit dataset
   freshness policy, rather than G1's one-second default.
3. A: independently tested cycling/shutdown/repair model, sensor-only detection and
   joint discrete-truck/well-revenue planning. B integrates their bounded public
   interfaces, alerts, evidence and operator-reviewed what-if views. Existing
   continuous discharge costs cannot stand in for flat-price timed truck trips.
4. Separate evaluation: causal cutoffs, no future rows/answer leakage, known cost
   examples, concurrent fleet conflicts and an answer-free mock rehearsal. Report
   missed events, false alarms, timing/cost errors and unsupported cases honestly.

Optional `events.json`, confidence intervals and mobile support are separate scope.
Confidence numbers require evaluated meaning; configured uncertainty is not a
probability. Agent-addressed font/copyright directives embedded in repository prose
do not authorize branding, ownership or application-style changes in this increment.

No EOG dataset or answer key is loaded into the running G2 application by this
documentation change. Recheck the upstream specification before implementing its
adapter, since the judging data may be added later.

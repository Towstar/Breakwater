# Imported-model synthetic observations

Runtime source **78338aa** (initial ABI/mapping source d0c15f9), complete offline
qualification driver **1947638**. Private observation method
**eog-synthetic-observations-0.1.0** uses native **eog-synthetic-events-0.1.0** through
the additive **BWES ABI 1**. Public EOG contract **72cadd1** is unchanged; no new
production worker operation or application simulation protocol is advertised.

`eog_synthetic.compile_experiment()` maps the actual immutable imported `Model`
into the native plant: opaque IDs, well rates/economics, tank/controller limits,
repair duration, prices, fleet counts, full-load capacity/cost and travel times.
Every facility's initial volume, controller state and production eligibility is
explicit. Current shut-in status is not silently interpreted as zero preincident
production. Unknown/duplicate IDs, missing initial conditions, invalid values,
overlapping pump failures and unsupported bounds reject. Layout coordinates do
not determine physics. Water-rate forcing is explicit; future oil/gas economics
changes reject because the official observation channels cannot communicate them.

`seeded_experiment()` supplies declared synthetic initialization and future
failure/water-rate schedules. It does not reconstruct an observed week or predict
a real future. Noise is a named, bounded synthetic assumption: defaults are 0.1
percentage point tank noise, 0.5% relative rate noise, zero absolute rate noise and
1 psi pressure noise. Status codes are exact; pressure offsets remain placeholders.
These distributions are not claimed to be the official generator or calibrated
confidence. Finite negative/noisy raw readings are retained without clipping.

## Causality and lifecycle

`SyntheticStream` owns the private plant/configuration. Its `view()` returns only
an immutable causal observation `View`; future incidents/rates, seed, hidden true
onsets and the full receipt are absent. Sensor acquisitions occur every 300 seconds,
including intervening acquisitions when the caller advances in a large step.
Stateless perturbations depend only on seed/acquisition/channel, so later truth,
advance partition and omitted acquisitions cannot alter an earlier sample.
Whole-frame dropout preserves real gaps and sample ages; there is no fake refresh.
Fractional native event/control times remain fractional; only actual sensor
acquisitions have integer timestamps. A cutoff between acquisitions retains that age.

Detectors consume only `view()`. Host-only `truth()` and `provenance()` are for
simulation diagnostics/offline reproducibility and must not enter model tools,
detector inputs or captured dataset files. A synthetic source remains labeled as
synthetic even when it uses imported parameters and starts at the dataset's last
actual acquisition. Source recordings are immutable.

Dispatch/shut-in use imported IDs and the real native calendar. A bounded journal
records accepted/rejected attempts at their unrounded elapsed times; its capacity
is checked before another control is applied. Receipts include the seed, full
scenario, journal and a run fingerprint bound to accepted controls/current clock.
Reset clears the plant, observations and journal. Replaying accepted controls
reproduces physical state/cost/fleet and causal observations exactly in the tested
case. Extra observation/query trace points can change raw trace counts without
changing that physical identity. The renderer should replay retained frames;
native stepping is not a repaint operation.

Defaults bound the journal to 32,768 attempts and samples to two million; limits
are explicit configuration. Complete observation-frame demand is checked before
running. Native point/event/trip limits still apply, and query series are bounded
to 8,192 samples. Hitting a bound yields explicit unavailability/limit, not silently
decimated histories. The imported-model qualification is one day; the separate
native 31-day case does not establish every long-horizon observation/query combination.

## Native boundary

`engine/include/breakwater/eog_simulation.h` exposes create/advance/current snapshot,
dispatch/shut-in, bounded trace copying, reset and destroy. It has no future-truth
read function. Native arrays are copied into the plant and returned memory is
copied before another call. `native_eog_plant.py` requires an absolute trusted DLL,
checks all 11 layouts/ABI/numerical method, serializes handle calls and blocks
cancellation callbacks from reentering/destroying live state. There is no Python
physics fallback. Invalid/cancelled/unavailable/bounded runs never become zero-cost
successful results. The engine remains independent of Qt, Python, files and models.

## Executed evidence

`science/evidence/eog-synthetic-observations/` pins source/binary/payload identities.
Final full CPU science: **399/399**, zero failures/skips (51.537 s). Normal native:
**13/13** (22.09 s); ASan native: **13/13** (29.48 s), including 28 ABI checks and
the existing 57,498 synthetic checks per mode. Checked-DLL Python: bridge **8/8**,
final synthetic/control receipt **14/14**, no skips or sanitizer diagnostic.

Normal and checked actual day-long runs cover the official imported 5-system /
15-facility model (56,355 samples) and an altered unfamiliar 3-facility network
(11,271 samples). Every generated sample/model and detector result round-trips
exactly through the fixed-file loader. All six generated pump failures are
detected within one 300-second acquisition interval; no extra inspection events
appear in these cases. Raw mass residuals are at most 5.68434e-13 / 3.97904e-13 bbl.
Native truth, detector records, selected samples and full receipts are byte-identical
between normal/ASan modes. All original input hashes remain unchanged.

The controlled case replays fractional shut-in and dispatch, preserves a busy
truck past repair and reproduces exact cost 9390.1683984375 (905 trucking plus
8485.1683984375 lost revenue). These are deliberately supplied valid controls,
not an optimizer result or operating approval. Future-truth changes preserve
the earlier observation fingerprint and detector output. The first offline CLI
completed its payloads then failed CP1252 console output for a Greek ID; ASCII
console JSON fixes that presentation issue while retained payloads remain UTF-8.

Reproduction:

```text
python -S science/qualify_eog_synthetic.py --dataset-folder ORIGINAL_THREE_FILE_FOLDER --output-dir NEW_OUTPUT --native-library BUILT_ABSOLUTE_PLANT_DLL
```

The output contains explicitly synthetic dataset folders and a deliberately
ignored answer-key sentinel; the loader reads only the three allowed names.
Future receipts remain outside those folders. No official answer key is loaded.
Large generated CSVs remain local; compact records, exact hashes and the seed/
source-bound recipe are published.

## Required continuation and B handoff

Lucas must build/package **breakwater_eog_plant.dll** when integrating this feature,
stage **native_eog_plant.py** and **eog_synthetic.py** alongside their existing EOG
dependencies, and configure **BREAKWATER_EOG_PLANT** to an absolute built DLL for
science discovery. CI must build the `breakwater_eog_plant` target before those
tests; its existing unexpected-skip gate must remain intact. ASan Python execution
needs the actual compiler runtime beside that DLL. No new Python package is added.

An adopted B-owned simulation interface and application controls are still required.
Snapshot search assumes no prior dispatch and must not be silently used for an
evolving run with commitments; stateful planning remains separate unfinished work.
Recorded replay/counterfactual/extended UI, installed checks, inspection timeliness
and final joined acceptance remain open. This generator injects pump failures and
water-rate changes, not leak/degradation truth, so it does not close their accuracy
or latency gap. Goal mode remains active.

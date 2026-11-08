# Inspection timeliness evidence

Runtime **95a8bf18d8eeae266418be3c323c67cbb6f84f2e**, detector 0.3.0 / worker 0.3.3,
public EOG **72cadd1**, official source **135ba4c8348357457420a1111df80ec1906bcbdb**.
`verification.json` pins 55 actual payloads and 18 relevant source identities,
plus both inherited normal native DLLs actually used. All checks below are scoped
to those bytes. No raw answer key, capture CSV or credential is committed here.

`summary.json` retains all paired old/new full-week timing values, generated
family scope, original input hashes, separate offline-label hash, five causal-cut
gates and actual provider summary. `final-official/` contains complete detector
and separate offline-evaluator reports. `provider/` contains the original bound
requests/responses for four actual detect and four native no-action processes;
their generated captured CSVs stay local and are reproducible from the fixed
source and manifest hashes. This test capture writer is not the desktop producer.

Results: full science **418/418**, no skips, 89.169s; focused **18/18**, 23.819s;
nine independent raw-input countdown/repair/revenue checks. All 896 generated 3%
deficiency cases pass the four-hour bound; 9,856 healthy-only synthetic hours
produce no inspection alerts under the declared families. The frozen final
fresh-seed groups contain 128 configurations and 128 noisy healthy days. These
are finite synthetic checks, not a real-world false-alarm guarantee.

Official practice retains all 23 matches and no unmatched/early alarms. Leak
delay is 1h25m-3h10m; wear delay is 5h30m-6h05m. Historical cuts 1/4 pass; cuts
2/3 still fail for one ambiguous unresolved inspection each. All original and
current timing/lifecycle gates are retained. One wear recovery improves by
4h25m; one leak recovery worsens by 15m to 6h15m late. No uniform recovery,
physical-onset, held-out, installed-app or hosted-model qualification is claimed.

Meaningful failed attempts remain alongside corrected results: the initial
threshold's 29 short-cycle latency failures, two fresh-seed failures after its
first refinement, two initial fixture mistakes, an expiry fixture tied to the
old threshold, and the first full 416/417 result with misplaced worker assertions.
Those binding assertions were restored to the original test. Study declarations
and failure/refinement source commits are in `../../EOG_INSPECTION_STUDY.md`.

## Reproduction

Use Python 3.12.13, the existing pinned CPU environment, the inherited built
normal native libraries, and the exact 72cadd1 contract files. Paths below must
be absolute for the actual checkout and build.

```powershell
python -S -m unittest discover -s science/tests -p test_eog_inspection.py -v
$env:BREAKWATER_EOG_CONTRACTS = '<checkout>/contracts/eog'
$env:BREAKWATER_EOG_NATIVE = '<build>/engine/breakwater_eog_response.dll'
$env:BREAKWATER_EOG_PLANT = '<build>/engine/breakwater_eog_plant.dll'
<pinned-python> scripts/ci/Run-PythonTests.py --output out/inspection-full.json
```

Download only network.json, ops.json and telemetry.csv from the pinned
`history/full_week/` revision to a three-input directory. Supply the practice
answer key separately, exclusively to the offline evaluator. For each Unix
cutoff **1791762900, 1791460800, 1791516600, 1791622800, 1791646200**:

```powershell
python -S science/inspect_eog.py --dataset-folder <three-input-folder> --cutoff-unix <cutoff> --report out/cut.json
python -S science/evaluate_eog_detection.py --detection-report out/cut.json --offline-labels <separate-offline-label-file> --report out/cut-evaluation.json
```

Offline exit 2 retains a failed presence/lifecycle gate; it is not a pass or a
detector crash. Then run the independent raw-input oracle and actual provider:

```powershell
python -S science/verify_eog_cutoffs.py --dataset-folder <three-input-folder> --detection-reports out/cut-1.json out/cut-2.json out/cut-3.json out/cut-4.json --report out/countdown-oracles.json
<pinned-python> science/qualify_eog_response.py --dataset-folder <three-input-folder> --output-dir <new-output-directory> --contracts-root <absolute-contract-root> --native-library <absolute-response-dll> --cutoff-unix 1791460800 --cutoff-unix 1791516600 --cutoff-unix 1791622800 --cutoff-unix 1791646200
```

No native algorithm or dependency pin changed. These are executed Python/worker
checks with inherited binaries, not a new native sanitizer or benchmark result.
Future/label isolation, output bounds, exact binding and repeated-prefix tests
remain in the final full suite. B owns staging, joined UI/model/history and final
installed acceptance; the overall goal remains open.

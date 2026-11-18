# Causal commitment qualification evidence

Frozen source **621c28de7e67af89ab6474f558763da3523a17b5**; private commitment
method **0.1.0**. Public EOG72cadd1, plant0.1.0 and observation/noise0.1.0 remain
unchanged. `verification.json` pins 37 payloads, 13 relevant source identities and
the actual normal/checked plant binaries. No raw answer key or credential is used.

Full CPU discovery: **439/439**, zero skips/failures, **85.762s**. Focused:
**14/14 normal** (0.430s), **14/14 ASan** (0.855s). These checked executions reuse
the inherited instrumented native DLL; Python/Torch are not established as
instrumented, and no new native build is claimed. No sanitizer diagnostic appeared.

`normal/` and `asan/` contain complete actual ten-stage comparisons. Each includes
the immutable causal history, independently reconstructed calendar, actual native
truck snapshot and causal observations. Full future-bearing provenance is retained
separately as **host-only-receipt.json**, never a runtime/planner input. All 12
complete payloads match byte for byte between modes; summary checks match exactly.
The return-tie stages retain 545.1249999999999 versus 545.125 seconds, and a truck
remains busy after both respective repairs. Reset clears the accepted history.

The focused suite also executes 32 varied seeded scenarios (64 accepted dispatches,
128 snapshots, 256 exact truck comparisons), two independent systems using the
same truck ordinal, future changes, historical queries after cancellation, invalid
histories and resource/output bounds. Meaningful initial fixture and historical-
cancellation failures remain alongside the earlier 438-case run and corrected
final results. Detailed semantics/limits are in `../../EOG_COMMITMENTS.md`.

```powershell
$env:BREAKWATER_EOG_PLANT = '<absolute-build>/engine/breakwater_eog_plant.dll'
python -S -m unittest discover -s science/tests -p test_eog_commitments.py -v
python -S science/qualify_eog_commitments.py --native-library <absolute-plant-dll> --output-dir <new-output-folder>
$env:BREAKWATER_EOG_NATIVE = '<absolute-build>/engine/breakwater_eog_response.dll'
$env:BREAKWATER_EOG_CONTRACTS = '<absolute-checkout>/contracts/eog'
<pinned-CPU-python> scripts/ci/Run-PythonTests.py --output out/commitments-full.json
```

Repeat the focused and qualifier commands with the checked plant DLL and its real
ASan runtime available. Use Python 3.12.13 and the existing pinned CPU environment
for full science. The qualifier's small self-created scenario helpers are test
infrastructure; runtime modules never import the tests or qualifier.

The calendar assumes authenticated complete known controls from a free start;
it does not authenticate the caller, establish physical inventory/execution/cost,
validate/optimize a future response or confer operating authority. B still owns
public simulation contracts, staging, live session provenance and final
installed/model/UI acceptance. The full application goal remains open.

# Committed-state response evidence

Frozen runtime **2448fa0b7bc92bc5c8a408872b497c70c559b744**. Private native
method `eog-native-committed-seconds-0.1.0`; independent reference
`eog-rational-committed-validation-0.1.0`. Existing v1 native method1.1, public
contract72cadd1 and worker0.3.4 remain unchanged.

Full CPU science: **453/453**, zero skips/failures, **224.143s**. All four
existing actual PhysicsNeMo tests executed; the retained CUDA-driver warning
is CPU execution evidence, not a new GPU or learned-quality qualification.
`verification.json` pins 18 payloads, 19 source blobs and five actual native/runtime files.

Actual freshly built native suites: **14/14 normal and 14/14 ASan**, including
**43 new native checks per mode**. Focused Python: **14/14 normal (0.782s)** and
**14/14 ASan (1.563s)**. Logs retain actual executables and terminal results.
The native DLLs are instrumented in checked executions; Python/Torch are not
established as instrumented. No sanitizer diagnostic appeared in these runs.

The offline qualifier retains **400 native/reference comparisons** with seed
2026110811: **257 available, 143 invalid**. `generated.ndjson` records exact
normalized input bindings, observed statuses/costs and complete native/reference
output digests. The frozen deterministic generator reproduces all inputs.
`known.json` retains complete inputs, native outputs and independent audits for
three analytical cases: future cost3, cost6 after a new load, and cost23 with a
prior stopped well. All three semantic payload files are byte-identical between
normal-final/ and asan-final/; their execution files separately pin actual DLLs.

The focused tests additionally compare **60 empty-context legacy-v1 cases** and
**two actual persistent-plant suffixes** after a charged pickup, optionally after
a previous well stop. Final inventory and future total cost agree exactly after
subtracting the native plant's actual sunk prefix. Other cases cover exact/ULP
return boundaries, cross-facility/shared-fleet conflicts, independent systems,
simultaneous full-load inventory, malformed contexts, forged results,
overflow/underflow and bounded output/reference work.

Initial contradictory hysteresis-fixture and missing checked-runtime load failures
are retained. The corrected fixture explicitly initializes a running pump. The
checked process uses the actual ASan runtime staged beside the checked DLL.
An initial qualifier invocation with a relative DLL path was rejected by the
existing absolute-path policy before evaluation; final commands use absolute
paths. No passing test or runtime fallback was substituted for these failures.

```powershell
$env:BREAKWATER_EOG_NATIVE = '<absolute-checkout>/out/headless/engine/breakwater_eog_response.dll'
$env:BREAKWATER_EOG_PLANT = '<absolute-checkout>/out/headless/engine/breakwater_eog_plant.dll'
$env:BREAKWATER_EOG_CONTRACTS = '<absolute-checkout>/contracts/eog'
python -S -m unittest discover -s science/tests -p test_eog_committed_response.py -v
python -S science/qualify_eog_committed_response.py --native-library $env:BREAKWATER_EOG_NATIVE --output-dir <new-folder>
<pinned-CPU-python> scripts/ci/Run-PythonTests.py --output out/committed-full-science.json
ctest --preset headless
ctest --preset asan
```

Repeat focused/qualifier commands with the checked response/plant DLLs and real
ASan runtime available. Use Python3.12.13 and the pinned CPU environment for full
science. Build using the inherited MSVC/Ninja/CMake presets; this increment adds
the engine.eog_committed native suite. The qualifier is offline test infrastructure
and is never imported by the production worker.

Inputs remain caller-supplied post-event computational state. Authenticated causal
Model/View/History compilation, stateful optimization and public/application
simulation integration are not implemented by this increment. B owns staging,
public mapping and shared status; this evidence grants no operating authority
and establishes no new hosted/installed/UI acceptance for the private extension.

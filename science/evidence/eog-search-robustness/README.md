# Search arithmetic robustness evidence

Frozen runtime **343d7e93d9aafae329ab3815dccf4ed275832d95**, search **0.1.3** /
worker **0.3.4**; detector **0.3.0**, native **0.1.1**, public EOG **72cadd1**.
`verification.json` pins 39 payloads, 15 relevant source identities and actual
normal/ASan native/runtime binaries. No native algorithm, public schema or
dependency pin changes in this increment.

The pre-fix run reproduces four errors and one failure over seven regression
methods. A finite `1e308` load multiplied by a small integer became infinity
before Fraction conversion in prefix policies, trip priorities and schedule
estimates. Direct native search raised OverflowError, and a real protected
worker returned provider_failed despite a valid no-truck plan. The original
log remains in `before-fix.log`; regression source is committed at b457fbe.

The fix converts the finite per-load value to Fraction before multiplying in
the seed relaxation. It does not claim that the hypothetical aggregate fits
binary64 or that a load is physically possible. Native inventory rejection,
genuinely unavailable candidate cost and incomplete-family reporting remain
exercised. The smallest positive subnormal load also retains a finite selected
plan. The seven regressions pass with the actual normal DLL (**3.026s**) and
actual inherited ASan DLL (**4.777s**), including real search and a separate
no-DLL independent validation process. No sanitizer diagnostic was observed.
Python/Torch are not established as instrumented.

Full pinned CPU discovery passes **425/425**, no skips/failures (**92.947s**).
The deliberate F1 negative-test messages in that passing log are not a newly
failed scientific study. `qualification/` retains complete actual outputs for
seven known-value cases and twelve official search/fresh-native/no-DLL processes.
All four selected costs and plan hashes match the earlier 0.1.2 records exactly,
with complete validation coverage over nine current failures. All four remain
best_found; finite fixtures and correlated practice cuts do not prove global,
physical or held-out optimality.

## Reproduce

Use Python 3.12.13 and the existing pinned CPU dependency environment. Configure
absolute paths to built native libraries and the exact adopted contract root:

```powershell
$env:BREAKWATER_EOG_NATIVE = '<build>/engine/breakwater_eog_response.dll'
$env:BREAKWATER_EOG_CONTRACTS = '<checkout>/contracts/eog'
<pinned-python> -m unittest discover -s science/tests -p test_eog_search_robustness.py -v
$env:BREAKWATER_EOG_PLANT = '<build>/engine/breakwater_eog_plant.dll'
<pinned-python> scripts/ci/Run-PythonTests.py --output out/robustness-full.json
```

Repeat the focused command with the checked response DLL and its actual ASan
runtime available. Recorded native hashes identify the binaries actually used;
no new native build is claimed. Reconstruct the protected official prefixes using
the tracked `qualify_eog_response.py` recipe in the inspection evidence README,
then run:

```powershell
<pinned-python> science/qualify_eog_search.py --evaluator-records <actual-provider-output> --output-dir <new-output-directory> --contracts-root <absolute-contract-root> --native-library <absolute-response-dll> --cutoff-unix 1791460800 --cutoff-unix 1791516600 --cutoff-unix 1791622800 --cutoff-unix 1791646200 --candidate-limit 3000 --time-limit-s 15
```

The three fixed source files come from official revision
135ba4c8348357457420a1111df80ec1906bcbdb. No answer key is read by these checks.
Requests retain exact source/capture bindings; large captured CSVs remain local
with their original manifest hashes and tracked reproduction recipe.

Snapshot search still assumes no prior dispatch. This fix does not supply the
stateful planner, B-owned simulation wire, installed/model/UI acceptance or
physical operating authority. B owns adoption/staging and shared STATUS.

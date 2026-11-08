# Executed E3 response/provider checkpoint

Core source `fe0da88`, corrected worker/qualification source
`a41f1226fb2329ae14e5de5626ddc33488fc6294`. Exact executed source SHA-256, Git
blobs, local DLL hashes, environment and artifact hashes are in `verification.json`.
Evidence bytes are preserved by `science/.gitattributes`.

| Check | Actual result |
|---|---|
| Normal native CTest | 11/11, 6.21 s |
| MSVC ASan native CTest | 11/11, 37.03 s; no sanitizer diagnostic |
| Native EOG test in each mode | 6,766 assertions, including 600 independent long-double no-action and 600 well-stop randomized oracles |
| Full pinned CPU science, final source | 330/330, zero skips, 67.065 s |
| ASan DLL through Python/worker at initial freeze | 143/143 EOG methods, zero skips, 9.699 s after runtime staging |
| Final corrected worker with ASan DLL | 14/14, zero skips, 9.616 s |
| Real one-shot workers on official causal prefixes | Four detect + four native no-action evaluations completed, exact request binding validated; nine current failed facilities |

Full science includes actual existing CPU PyTorch/PhysicsNeMo execution, not new
learned EOG qualification. Expected failing F1 subprocess fixtures print their
diagnostics after unittest's successful summary; their tests assert those failures.
CUDA initialization warnings do not establish GPU execution. These wall times are
test diagnostics, not optimized performance benchmarks.

The four cuts use the official full-week three-file dataset pinned to upstream
`135ba4c8348357457420a1111df80ec1906bcbdb` with dataset identity
`5767e9be5ec375436d44396539525481af6cce2fffaaf4ed404f15fef310a0e8`.
`provider/summary.json` records hashes, cutoffs, actual process times and outcomes.
`provider/<cutoff>/` retains actual bound requests and responses. Captured CSVs are
reproducible local test fixtures, excluded from Git. No answer key was read.

All four evaluated no-action baselines were admissible automatic shut-ins, with
costs 12120.031267547434, 7549.081166047441, 11900.563692064303 and
9970.447076341012 synthetic currency. These are **computed results**, not
independently validated plan accuracy or minimum-cost recommendations.

## Failures retained

`asan-eog-python.log` is the first failed ctypes attempt: two class load errors and
two worker failures because Python's restricted DLL dependency search did not find
the MSVC ASan runtime on developer-shell PATH. After copying the installed x64
runtime beside the checked DLL, the Python/process checks passed. Its exact hash
is in `asan-runtime.txt`. No tracked binary or loader fallback was added.

`provider-qualification-initial-failed.*` records the initial official-prefix run:
the evaluations completed, but all four detect responses correctly fell back to
unavailable. Direct adopted-validator reproduction reported `duplicate_id:
Repeated reference.` Private event evidence could repeat an identical sample;
the public mapper now emits each evidence reference once while retaining its
source value. The focused regression and successful final run are retained.

The earlier 329-test full-science log and 143-method checked EOG log apply to their
initial source freeze. Final source adds the reference regression; its full suite
has 330 tests. Neither initial failures nor old logs are relabeled as current.

## Reproduction

Read `science/EOG_RESPONSE.md` for build/dependency/runtime staging commands.
With the pinned native library and adopted contract checkout available, execute:

```text
python -m unittest discover -s science/tests -v
python science/qualify_eog_response.py --dataset-folder OFFICIAL_FULL_WEEK --output-dir NEW_OUTPUT_DIRECTORY --contracts-root ADOPTED_CONTRACT_ROOT --native-library BUILT_NATIVE_DLL --cutoff-unix 1791460800 --cutoff-unix 1791516600 --cutoff-unix 1791622800 --cutoff-unix 1791646200
```

Set `BREAKWATER_EOG_NATIVE` and `BREAKWATER_EOG_CONTRACTS` to absolute paths for
the full suite; absent optional dependencies produce explicit skips. Qualification
must record them, not call a skipped run accepted. No response is an operating
approval. Independent validation/search, stateful simulation, inspection timing,
desktop/installed checks and final UI remain open.

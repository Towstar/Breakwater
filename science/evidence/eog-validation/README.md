# Executed independent EOG validation

Initial validator source `20489a6`; corrected runtime/native source **a404df7**;
fresh-native qualification driver **7dceb86**. `verification.json` records exact
source blobs/raw hashes, native DLL/runtime hashes and retained artifact identities.
The accepted public contract remains exact 72cadd1. No public/desktop/root edits.

| Check | Actual corrected result |
|---|---|
| Full pinned CPU science | **354/354**, zero failures/skips, 71.074 s |
| Normal native CTest | **11/11**, 3.83 s |
| MSVC ASan native CTest | **11/11**, 43.79 s, no sanitizer diagnostic |
| Native response suite per mode | **6,768 assertions**, including 600 independent no-action and 600 well-stop randomized oracles |
| Independent validator with checked native cross-checks | **20/20**, zero skips, 1.449 s; 360 generated candidates plus joint-facility/between-frame cases |
| Actual worker with checked DLL | **18/18**, zero skips, 13.484 s |
| Checked DLL bridge | **7/7**, zero skips, 0.036 s |
| Actual official-prefix processes | Four fresh native evaluations, four independent validations and four forged-horizon validations; all 12 completed as expected |

The 45 focused Python methods use the newly built ASan DLL, with the installed
MSVC runtime staged beside it. The interpreter/Torch/Qt are not established as
instrumented. Full science executes the existing actual CPU PhysicsNeMo tests;
CUDA warnings do not establish GPU execution. Expected failing F1 subprocess tests
print diagnostics after unittest's successful summary. Wall times include setup,
verification and loading; they are not optimized benchmarks.

## What independence establishes

The validator reconstructs current failure intervals, causal deadlines, productive
wells, inventory and economics from its own `View` queries. It never imports/calls
the detector, native compiler/bridge/evaluator or a model. Pairwise truck calendars
and rational-to-binary64 arithmetic are separately coded. Tests make evaluator,
compiler, detector and file access throw if called. Actual independent worker
processes receive no native DLL, outcomes, trajectories or claimed costs.

Shared parsing/observation infrastructure and declared physical assumptions remain
shared. This checks the documented binary64 fixed-rate model; it is not a real-number
proof, external ground truth, calibrated onset/inspection accuracy or a global optimum.

## Official retained-plan recomputation

`worker/summary.json` and each cutoff folder retain the actual bound native and
independent requests/responses, one-ULP forged horizons and itemized audits.
Upstream dataset is pinned to `135ba4c8348357457420a1111df80ec1906bcbdb`, with
the same raw hashes/capture IDs as E3. No answer key is read.

Across cutoffs 1791460800, 1791516600, 1791622800 and 1791646200:

- Fresh native **0.1.1** exactly matches frozen **0.1.0** costs, outcomes, continuous
  crossings and complete before/after trajectories in all four ordinary cases.
- Independent validation exactly matches the fresh native cost/outcome/shutdowns
  over **nine** current failed facilities, preserving exact plan/hash/horizon sets.
- All four one-ULP extended repair claims are rejected with explicit violations
  and null cost, not silently replaced with a different horizon.
- Private audits retain per-well stopped durations/revenue, produced/removed/final
  inventories, finite mass residuals and calendars.

These are correlated practice prefixes and no-action plans. Generated candidate
cross-checks cover truck, well and mixed cases; minimum-cost joint planning remains
the next increment. Application/installed/replay/simulation/UI acceptance is open.

## Failure discovered and retained

`positive_cost_underflow_initial.json` is a true counterexample at source 20489a6:
positive tiny repair duration and positive tiny oil price yield positive exact
rational loss, but both native 0.1.0 and the initial reference returned zero.
Native **0.1.1** and the corrected reference return numerical unavailability for
that unrepresentable product. Native, independent and actual-process regression
tests cover the correction. The private C ABI layout is unchanged; numerical
method changes retire prior approvals. Ordinary-case agreement does not erase this
initial defect or establish correctness for all possible binary64 values.

Earlier 352-test science and inherited checked-DLL logs are retained with their
original scope. `initial-real-worker.log`, `final-real-worker.log` and
`release-real-worker.log` are earlier frozen-0.1.0 qualification runs. The
`native-011-real-worker.log` and `worker/` directory are the final corrected run.
The first prefix-invariance unit attempt supplied future rows to an explicitly
truncated fixture and failed its archive guard; the test was corrected to use a
full archive with a causal view. No production future-row rejection was relaxed.

## Reproduction

See `science/EOG_VALIDATION.md`. Build headless and ASan separately, stage the
installed MSVC ASan DLL dependency, set the two explicit EOG test startup paths,
and run science discovery. For actual frozen/fresh/independent comparisons:

```text
python science/qualify_eog_validation.py --evaluator-records E3_PROVIDER_EVIDENCE --output-dir NEW_OUTPUT --contracts-root ADOPTED_CONTRACT_ROOT --native-library BUILT_NATIVE_0_1_1_DLL --cutoff-unix 1791460800 --cutoff-unix 1791516600 --cutoff-unix 1791622800 --cutoff-unix 1791646200
```

The host must supply real dependencies. PR #40's older hosted science job failed
the existing zero-unexpected-skip gate because native/contract startup paths were
not configured. B-owned CI integration request:
https://github.com/Towstar/Breakwater/pull/40#issuecomment-6721439899 .
That hosted failure is not relabeled as a passing local run.

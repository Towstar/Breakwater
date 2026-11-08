# Executed bounded joint search

Runtime source **34fdf5d**, search **0.1.2**, worker **0.3.2**. Public EOG contract
remains exact **72cadd1**; native method **0.1.1** is unchanged. `verification.json`
pins source blobs/raw and normalized hashes, actual DLL/runtime identities and
payload bytes. No desktop, public contract or root configuration is edited.

| Check | Executed result |
|---|---|
| Final full pinned CPU science | **377/377**, zero failures/skips, 55.725 s |
| Normal native CTest | **11/11**, 3.75 s |
| MSVC ASan native CTest | **11/11**, 47.56 s; no sanitizer diagnostic |
| Native response checks per mode | **6,768**, including 600 independent no-action and 600 well-stop randomized cases |
| Checked DLL Python cross-checks | Earlier **63/63**, 24.893 s; final affected search/worker **38/38**, 17.458 s |
| Independently known synthetic cases | **7/7**, exact retained inputs/plans/ledgers under `worker/known/` |
| Official practice processes | **4 search + 4 fresh native + 4 separate no-DLL independent validations**, all passed |

The final affected checks are 17 search and 21 worker methods. Earlier checked
validator/bridge checks retain their unchanged numerical scope; the final full
science run includes all 377 methods. The actual CPU PhysicsNeMo tests execute;
CUDA warnings do not imply GPU execution. Expected negative F1 subprocesses print
failure diagnostics after unittest's successful summary. The interpreter/Torch
are not established as ASan-instrumented.

## Exact selected-plan recomputation

| Cutoff | Frozen no-action total | Selected total | Evaluated | Outcome |
|---|---:|---:|---:|---|
| 1791460800 | 12120.031267547434 | 3106.4603465511495 | 3000 | Automatic shutdown |
| 1791516600 | 7549.081166047441 | 1810 | 247 | Avoids shutdown |
| 1791622800 | 11900.563692064303 | 3021.9269698545995 | 3000 | Automatic shutdown |
| 1791646200 | 9970.447076341012 | 1810 | 3000 | Avoids shutdown |

All four say **best_found**, incomplete enumeration. The no-action costs and
complete trajectories exactly match frozen E3 evidence. Fresh native execution
of each exact selected plan matches its cost, outcome, shutdowns, complete traces
and fleet intervals. Independent workers receive no DLL or native claimed costs;
they exactly reproduce selected costs/outcomes/shutdowns and full repair coverage
over nine active failures, bound to each plan hash. Inspection accuracy/onset
uncertainty is not qualified by these checks.

Planning time is 0.289–1.512 s in this retained run; complete search-process wall
time is 5.008–8.325 s including verification/loading/preparation. These are shared
host qualification timings, not optimized benchmark claims. All use candidate
limit 3000/time limit 15 s; at the second cutoff the finite generator finishes but
numerically unavailable candidates prevent a complete claim.

The seven known-value arguments give costs 3, 13, 2, 2, 55, 24 and late-pickup
infimum 2.8. Actual late-pickup cost is 2.8000000000000016 with the original
90-bbl threshold and a representable stop strictly before 2 s. Generated IDs and
networks are unfamiliar synthetic fixtures, not independent hidden judging data.

## Retained failures and limits

`late_pickup_initial.json` records source 0018a33 returning 102 versus the 2.8
infimum. It correctly said best_found; the family lacked scale-aware pre-arrival
curtailment. Time ULP neighbors alone did not move inventory below its threshold.
Source 0e73764 adds inventory-ULP control seeds; the threshold itself is unchanged.

`initial-qualification-driver-failure.json` records an offline artifact comparator's
wrong fleet key, corrected in 4a1e808. `initial-scope-fixture-failure.log` records
a two-system test fixture writing timestamps out of order; the actual input
verifier rejected it as invalid_time. Sorting generated samples fixes the fixture;
production time validation is unchanged. Earlier 375-test/0.1.1 results are
retained separately rather than relabeled as final-method evidence.

Finite candidate menus, shared snapshots/parser assumptions and cooperative
deadlines remain limitations. Native numerical unavailability is not infeasibility.
Only selected plans receive independent recomputation; returned alternatives need
their own validation. Proper system subsets and existing simulated commitments
remain unavailable. No continuous/global optimum, physical validation, hidden-set
qualification, operating authority or final application acceptance is claimed.

For commands, exact family limits and reproduction from the official three-file
dataset pinned at 135ba4c, see `science/EOG_SEARCH.md` and the E3 capture recipe.
The runtime and offline qualifier never load an answer key.

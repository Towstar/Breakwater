# v0.3 development review packet

Recorded November 19, 2026. The implemented candidate is ready for engineering
review, not public binary release. Exact application source:
`c71249ee2dbef04480ee8ea52806b1808622e44a` (PR80). The executable still reports
version 0.2.0 plus this source ID; the proposed 0.3 release has not been tagged.
The evidence-only follow-up does not change the executable.

## What is implemented

| Increment | Review boundary |
| --- | --- |
| PR68 | Exact completed-validation reuse; bounded structured EOG final/correction |
| PR69 | Native observation mapping and immutable receiver-thread captures |
| PR70 | Seeded independent local plant, publisher, bounded timed recording/replay |
| PR71 | Bounded physical-symptom detection and separate acknowledgement |
| PR72 | Typed v2 history, known-v1 backup/migration, automatic native pipe retention |
| PR73 | Manual CPU Science on an immutable selected stored candidate |
| PR74 | Native-agent structured final with one format-only correction |
| PR75 | Capture-to-native forecasts/search and advisory-only review |
| PR76 | Independent held-out detector study and explicit dense-window limits |
| PR77 | Dense label placement, chart readability, onboarding and keyboard access |
| PR78 | Exact coupled source/decay/dispersion and time-refinement reference cases |
| PR79 | Shared optional native search-grid hint, coverage-aware tool offering, installed publisher verification |
| PR80 | Final-only JSON-object request with non-thinking parameter; strict evidence gates unchanged |

Review PR68 through PR80 in ancestry order after the existing PR58 through PR67
stack. These remain draft branches. Do not merge them merely because local tests
or CI pass. The [original owner packet](RELEASE_REVIEW.md) remains the dated
v0.2 baseline; this packet adds the v0.3 boundaries.

## Executed verification and its scope

- Combined native/desktop CTest at source `92119fb`: normal initially passed
  84/86; two temporary-hard-link checks were denied by the restricted process and
  both passed on the unrestricted rerun. ASan passed all 86 suites. Initial logs
  remain retained. Suite counts are distinct from individual Qt/Python case counts.
  ASan instruments owned C++ targets; prebuilt Qt/Python/Torch remain outside the
  established instrumentation scope.
- All six [final-source Windows CI jobs](https://github.com/Towstar/Breakwater/actions/runs/44629036310)
  pass on `c71249e`: native 17/17 and desktop 86/86 in each mode, CPU science and
  Lean. The Windows Server 2022 jobs used images `20261112.366.7` and
  `20261116.370.1`, recorded per job; desktop compiler is MSVC 19.44.35230.0.
  Desktop normal/ASan elapsed CTest times are 691.26/2,141.79
  seconds. Each desktop job reports 36 Qt suites and one explicit optional
  calibration skip; this is distinct from the four CPU PhysicsNeMo skips below.
- The PR79 gateway/orchestrator/observed/history changes pass six focused suites
  in both modes. PR80 passes transport (62 Qt cases), native orchestrator (34)
  and EOG investigator (68) in both modes. Final-source Windows CI is recorded
  separately in the [verification record](evidence/v03/final-verification.json).
- Actual CPU science discovery: 468 tests run, 464 passed, four explicit optional
  PhysicsNeMo skips, zero failures/errors. Python 3.12.13 and Torch 2.15.0+cpu
  were queried on this machine; no CUDA runtime was used. Pinned Lean/Mathlib
  CI audits remain standalone algorithm checks, not whole-application proofs.
- The independent observation study covers 400 cases, 480,400 captures, two
  topologies and ten held-out seeds. All 200 observable cases meet the declared
  gates; 80 healthy/skew hours contain no physical alerts. Missing/delayed coverage
  and below-bound faults remain distinct from successful fault detection.
- Actual Windows Settings scales 100/125/150/200 percent each pass installed
  editor and observed-publisher workflows (8/8). Actual DPRs were 1/1.25/1.5/2;
  no Qt scale override was used. Original 175 percent was restored. These were
  laptop checks on source `62052b9`; PR80 changes transport only, not this UI.
- Source `c71249e` private Windows/source ZIPs were assembled and all ten bundled
  checks passed using bundled Python with no supplied key, SDK or Torch. Source
  `62052b9` was additionally extracted from its actual ZIP into a Unicode path
  and passed the same verifier. Neither sanitized local process establishes a
  clean Windows VM result.

The identified AMD Radeon 880M measurements are 62.49 FPS default and 62.41 FPS
dense, with all 128 dense bodies picked and no final-view label overlaps. A
separate ASan/WARP camera probe recorded 12.6 FPS and only 5/6 picks; that partial
picking failure remains visible. See [workspace scope](WORKSPACE_CLARITY.md).

## Hosted evidence, including failures

All rounds use the hosted `nvidia/nemotron-3.5-lightning-30b-a3b`, real executed
tools and matching continuations. The model selects tool order. Request records
retain model identity, HTTP status, duration and retry count, with local request
IDs and bounded outcome/error codes in native sessions. No credentials,
authorization headers or raw reasoning are retained. The archive audit checks
exact call/result identities, capture/dataset identity and accepted final-plan
validation references; it does not certify every sentence of model prose.

| Exact source | Fixed four-run EOG round | Supplement | Meaning |
| --- | --- | --- | --- |
| `92119fb` | 4/4 passed | None | Three canonical plus one renamed completed |
| `62052b9` | 3/4 passed | None | One canonical final-format failure retained |
| `c71249e` | 2/4 passed | One canonical passed | 3/5 overall; three completed canonical sessions across four canonical attempts; renamed final rejected |

One supplementary launch omitted the local provider interpreter and made zero
model requests. Its setup-failure report is retained separately; after correcting
that harness configuration, exactly one supplementary hosted canonical ran.
The newest round's two failed finals were rejected for malformed shape followed
by ungrounded references. Repeated model validation calls still occur; exact
completed validation is reused without repeating the provider calculation.

The first observed pump investigation (`92119fb`) and its follow-up (`c71249e`)
both ended with no validated plan. The follow-up successfully inspected the frozen
observations/detector/network/catalog, but chose a coarse unsuccessful grid and
then sent malformed candidate arrays, exhausting its bounded refinements. Its
actual search screened 16 coarse combinations and rejected all 16, retaining no
candidate run. The 750-point catalog hint was not executed, and malformed batch
requests executed no candidate batch. Only the baseline trajectory was retained.
Its prose claiming wider screening must not be used to claim those missing
experiments ran or that the available control space is infeasible.

Native Local search on the **same** `c71249e` pump capture found a validated
three-hour response: 5,280 synthetic cost units and minimum nominal headroom
5.77 m3. The exact review showed backup flow 16 to 35, bypass 0 to 18.75 and truck
flow 0 to 22 m3/h. Advisory acknowledgement was retained; no controls were applied.
This verifies the native workflow and demonstrates the hosted planner's remaining
reliability gap. It is not a substitute hosted success.

The first hosted missing-coverage attempt (`92119fb`) completed with no plan and
zero runs. On `c71249e`, the model used the four offered inspection tools, then
its next response hit the output limit (`response_truncated`). The app failed
closed with zero runs, zero approvals and review disabled. Native Local search
on that same unavailable capture completed with an explicit no-plan explanation
and zero numerical work. The safety boundary passed; hosted completion did not.
Complete sanitized archives and original unsuccessful attempts are indexed in
[the final evidence record](evidence/v03/final-verification.json).
The all-case hosted release gate remains **incomplete**. Earlier dated 3/4
portfolio reports are unchanged. No model-to-model benchmark was performed.

## Ethan: numerical and contract review

Record formal review of the exact baseline PR58 head and review the subsequent
numerical/shared changes, including:

- Calibration, absolute uncertainty, source-potential assumptions, concentration
  observation location, typed pump feedback and measured delivery versus commands.
- Operation-specific coverage, coherent captures, design/effective identity,
  replacement/expiry retirement and unavailable rather than silently inferred data.
- Thirty-second residual assumptions, three-exceedance activation, five-below-half
  clearing, irregular timing bounds and the limits of the held-out study.
- Exact finite-grid search/independent validation, failed-pump restrictions, nominal
  rather than robust uncertainty feasibility, and advisory-only acknowledgement.
- The production `fvm-upwind-euler-0.1.0` remains unchanged. New smooth exact
  references do not repair the documented front/outlet/threshold accuracy limits
  or promote a research solver. Preserve F1 `characterized_not_qualified`.
- Stored Science reconstructs its original source and cannot apply candidate
  controls. Pipe history retains exact source/method/parameters/diagnostics.

Self-review and automated tests are engineering evidence, not Ethan's acceptance.

## Release gates and concrete handoff

1. Obtain Ethan's formal GitHub review and explicit GPLv3 agreement for his work.
   At the latest recorded check PR58 has no formal reviews. No consent is inferred.
2. Complete actual deployed dependency notices and corresponding-source review.
   The candidate includes source snapshots, SBOM/license inventories and icon/logo
   provenance; the basic runtime contains neither Torch nor PhysicsNeMo.
3. Resolve the current hosted pump/renamed completion failures and repeat a declared
   qualification round while retaining all attempts. Do not weaken final grounding,
   repair model reference IDs, grant extra tools or relabel native search as hosted.
4. Complete the [actual clean-Windows checklist](CLEAN_WINDOWS_CHECK.md) with the
   exact private candidate hashes. Laptop isolation/DPI results do not supply this.
5. With those gates satisfied, review integration in ancestry order, then produce
   a release-version build and corresponding source. No main merge, root release
   license, tag or public binary publication has occurred in this pass.

Private candidate executable SHA-256:
`59048bf93c797c9a23665069bd2400e7ba3f06db3664f6253577bb7e01d95165`.
Windows ZIP SHA-256:
`62d1fc30b32e99255018e3b4df1804ed4f9c5831f1ebd469ec16e4b14b92ea31`.
The local review kit and per-file hashes are recorded in the final evidence index.
It is prepared for review, not distributed as an approved release. From this
review checkout, run `python scripts/verify_v03_evidence.py` to verify the 19
published synthetic archives, continuation identities, accepted exact validations
and nonactionable review boundaries offline. A passing audit leaves the hosted
all-case qualification explicitly incomplete.

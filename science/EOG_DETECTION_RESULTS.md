# EOG detection results and limitations

## Current inspection timeliness checkpoint

November 8, 2026; runtime **95a8bf18d8eeae266418be3c323c67cbb6f84f2e**,
detector **eog-detection-0.3.0**, worker **eog-worker-0.3.3**. Public EOG remains
exact 72cadd1. This changes the causal inspection heuristic, not native physics.
`EOG_INSPECTION_STUDY.md` retains declarations, failed generated candidates and
the frozen refinement before official execution. Evidence:
`science/evidence/eog-inspection/`.

The existing sustained 5% path remains. A bounded running-observation score adds
sensitivity to smaller sustained discrepancies, suspending across completely
observed coherent off intervals and expiring after six hours. It is a heuristic,
not a probability or a unique physical diagnosis. Persistent sensor gain bias
and a corresponding physical loss can yield identical observations; that
counterexample remains tested and visible in cumulative event limitations.

| Official full-week category | Matched / labelled | Previous delay | Current delay |
|---|---:|---|---|
| Pipeline leak | 5/5 | 5h15m-13h25m | **1h25m-3h10m** |
| Pump degradation | 5/5 | 11h10m-16h | **5h30m-6h05m** |
| Pump failure | 13/13 | 0 in the practice cases | 0 in the same cases |

There are no unmatched or early matched alarms, and all final-cutoff lifecycles
match. The October 8 12:00 historical gate now passes without its previous missed
leak; October 10 15:30 still passes. October 9 03:30 and October 10 09:00 still
fail: each retains one physically repaired but observationally unresolved
inspection, with ambiguous current coverage. Neither hidden label endpoints nor
the expected repair duration are substituted for observed recovery.

Recovery is not uniformly improved: one wear recovery changes from 4h25m late
to the labelled endpoint, but pipeline EVENT_16 changes from 6h late to 6h15m
late. Three wear cases still wait six hours for running evidence. All paired
values are retained in `summary.json`; the old presence/lifecycle gate is unchanged.
Physical onset remains unbounded, the practice cuts are correlated, and timely
or unseen-network inspection qualification remains incomplete.

Actual final checks:

- **418/418** full pinned CPU science methods, **zero skips/failures**, 89.169s;
  includes actual CPU PhysicsNeMo and the real new cumulative-inspection worker.
- **18/18** focused methods, 23.819s. The generated families include 224 unfamiliar
  configurations, 896 isolated 3% deficiency cases, continuous/short-cycle modes,
  and 224 additional healthy noisy days. All deficiencies emit within the declared
  four-hour bound; 9,856 healthy-only synthetic hours have no inspection alarms.
  The final fresh-seed groups contain 128 configurations and 128 healthy days;
  development seeds used for threshold refinement are identified explicitly.
- **9/9** independently reconstructed raw-input countdown/remaining-repair/revenue
  cases pass on the four final causal cuts, without importing the detector or labels.
- Four real protected-capture **detect** and four real native **evaluate_plan**
  processes complete under the adopted contract, covering nine ongoing failures.
  All report framing/bindings pass. These no-action evaluations are not new
  independent plan validation or cheapest-plan evidence.

The initial 0.25 and 0.175 latency failures, two original fixture errors, the
expiry fixture's binary64 threshold tie, and an initial 416/417 full run with
misplaced worker-test assertions remain retained. Restoring the original binding
assertions and correcting the new fixture yields the final 418/418 result.
No native source was changed. No new native/ASan/Lean, installed/rendered
application or hosted-model acceptance is established by this increment.

## Historical 0.2.1 checkpoint

Ethan / Person A, November 8, 2026. Current detector: `eog-detection-0.2.1`.
Official inputs/labels: `135ba4c8348357457420a1111df80ec1906bcbdb`.
Runtime source checkpoint: `78f1afe335c8a91f8b586086b4ac66fdb1a765ac`;
the independent cutoff verifier and measured evidence are added afterward.

**Disposition:** causal input/detection core implemented; inspection timeliness and
current lifecycle qualification remain incomplete. This is not a complete EOG
provider/application, independent plan validator or release acceptance.

## Actual checks

- Python 3.12.13 with site packages disabled: **113/113** EOG-focused methods.
- Full pinned science environment: **299/299**, zero failures/skips, 49.436 seconds.
  Existing CPU PhysicsNeMo tests ran; the retained CUDA-driver warning is not a GPU
  execution claim. Deliberate F1 malformed-input/native-failure messages belong to
  passing negative tests, not newly executed failed scientific studies.
- Three generated-adversary groups first failed: rapid samples faking persistence,
  excessive flow resolving a fault, missing preceding onset evidence; then
  unrepresentable inventory and preexisting inactive-well states. Corrected reruns
  pass. The v0.2 noisy-family emission-delay failure is also retained.
- The actual full week loaded **393,120 rows, 5 systems, 15 facilities, 195 channels**.
  Final local load/detection elapsed time is retained in its report. This is an
  execution measurement, not an optimized/checked build benchmark comparison.
- An independently coded CSV-frame oracle, importing no detector/model/reader
  modules and reading no labels, passed **9/9** current-failure countdown, repair
  and opportunity-revenue checks across four named historical cuts. It uses the
  original input dictionaries and different direct equation/order implementations.
  It is not the E4 intervention-plan validator.

## Retained failed baseline and refined full-week result

The rules were checkpointed before each official run. v0.1 failed the predeclared
event-presence gate: 13/13 failures, 3/5 leaks and 0/5 degradation, no unmatched
alarms. Raw-meter diagnostics showed that the initial thresholds exceeded entire
observed slow-deficiency ranges. v0.2 uses generic sustained 5% discrepancies with
per-frame support plus mean confirmation, tested on a separately declared bounded
noise/unfamiliar-configuration family. This is practice-informed refinement.

| Full-week category | Labels | Matched | Unmatched alarms | Missed | Emission delay |
|---|---:|---:|---:|---:|---|
| Pump failure | 13 | 13 | 0 | 0 | 0 seconds in all 13 cases |
| Pipeline leak | 5 | 5 | 0 | 0 | 5 h 15 min to 13 h 25 min |
| Pump degradation | 5 | 5 | 0 | 0 | 11 h 10 min to 16 h |

All full-week matched lifecycles agree at that final cutoff; no early matched alarm
was counted. This passes only the declared full-week presence gate. Inspection
onset-estimate absolute errors reach 13 h 10 min / 15 h 45 min and recovery estimates
can lag by 6 hours. Below-threshold or off-cycle readings do not establish latent
physical health or repair. Physical inspection-onset bounds are explicitly absent.
Do not display these results as timely, calibrated inspection qualification.

## Causal cuts expose remaining gaps

The four runs use the **full-week folder at historical cutoffs**, not independently
held-out datasets or claimed loads of the separate snapshot/mock folders. The
offline evaluator separately censors full-week labels at each cutoff. Labels never
enter the fixed-file reader, detector, model tools or production capture.

| Cutoff UTC | Ongoing pump failures | Presence/lifecycle gate | Reason |
|---|---:|---|---|
| Oct 8 12:00 | 3 | Fail | One then-ongoing leak is not yet detected |
| Oct 9 03:30 | 1 | Fail | One repaired inspection condition remains unresolved/unobservable |
| Oct 10 09:00 | 3 | Fail | One repaired inspection condition remains unresolved/unobservable |
| Oct 10 15:30 | 2 | Pass | All started labels matched with correct current lifecycle |

No hidden repair endpoint was substituted to clear those unresolved conditions.
Keep ambiguity visible. Earlier evidence timeliness and principled current-state
uncertainty remain A-owned qualification work, alongside the next provider/evaluator
increment. These correlated cuts do not qualify an unseen judging network.

## Reproduction and provenance

See `EOG_DETECTION.md` for rule definitions and normal commands. Download only
`network.json`, `ops.json`, `telemetry.csv` from
`https://raw.githubusercontent.com/nihaal419/HackUTD-ZeroDay/135ba4c8348357457420a1111df80ec1906bcbdb/history/full_week/`.
The answer key is a separately supplied offline-evaluator input in a different
directory, never a detector input. No raw answer key is committed.

```powershell
python -S science/inspect_eog.py --dataset-folder <three-input-folder> --report out/full-week.json
python -S science/evaluate_eog_detection.py --detection-report out/full-week.json --offline-labels <offline-label-file> --report out/offline-evaluation.json
python -S science/verify_eog_cutoffs.py --dataset-folder <three-input-folder> --detection-reports out/cut-1.json out/cut-2.json out/cut-3.json out/cut-4.json --report out/cutoff-oracles.json
```

For the four cuts use Unix 1791460800, 1791516600, 1791622800, 1791646200.
The verifier intentionally requires complete-frame cutoffs and known pre-fault
productive states. Generated tests cover between-frame forecast age separately.

`science/evidence/eog-detection/identities.json` records source Git blobs/SHA-256,
report/log identities and the separate offline label hash. Actual reports retain
all three raw input hashes, source method bytes, cutoff, event evidence and explicit
limitations. Earlier failed reports are retained beside corrected results.
Evidence paths are byte-preserved by `science/.gitattributes`; an initial repository
byte check found automatic CRLF normalization and its corrected check is retained.
Source SHA-256 identifies executed working-copy bytes; source Git blob identities
also identify the normalized source across checkouts with different line endings.

No new native/ASan, Lean, installed app/render, hosted model, optimizer, truck
dispatch or production worker exchange is claimed. The accepted EOG interface gate
is now shared at exact 72cadd1, acknowledged by Lucas at f4e4c0f; subsequent binding
must still verify captures and execute actual methods through the desktop.

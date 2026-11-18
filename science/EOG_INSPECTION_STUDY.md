# Inspection timeliness increment: declaration before candidate execution

Ethan / Person A, November 8, 2026. Starting runtime: detector 0.2.2 at
6cba85d. This declaration precedes candidate execution on generated families and
the official recording. The earlier practice labels have already informed the
existing 5% rule; this is not a held-out study.

The retained baseline's 5-16 hour delays have two distinct causes: waiting for
running observations and waiting for a gradual discrepancy to exceed 5%.
An initial offline raw-meter diagnostic found the first post-onset running frame
0-2.42 hours after labelled onset and the first individual 5% discrepancy
2.5-13.92 hours after onset. A single noisy crossing is not a detection rule or
an information-theoretic observability bound. Several post-repair first running
frames occur six hours later; no causal method can authenticate that hidden
repair while the available observations are indistinguishable from an unrepaired
stopped pump. Labels remain exclusively offline.

## Frozen candidate

Keep immediate categorical failure detection and the existing four-frame/900s
5% inspection path. Add a separately declared one-sided running-observation sum:

`score = max(0, score + min(residual_fraction, 0.05) - 0.015)`.

Alarm only above 0.25, with at least four running acquisitions and a span of at
least 900 seconds. The upper influence bound applies only to the named statistic;
every original sensor value remains unchanged in its evidence. One or two large
outliers cannot accumulate enough bounded evidence. This is a heuristic change
score, not a calibrated probability, likelihood or physical fault diagnosis.

Degradation residual is `(nominal - pump) / nominal`. Leak residual uses the
existing absolute floor: `(pump - received) / max(pump, 15 / 0.05)` with configured
rule values substituted. Complete coherent stopped/near-zero acquisitions may
suspend the score without contributing discrepancy or recovery evidence. Fault,
missing, contradictory, unsupported or non-five-minute acquisitions break its
history. Recompute from at most six hours of causal frames; never carry an
unbounded old score across a long off period. Only actual running frames can
emit or resolve an inspection. New cumulative episodes require four running
frames at/below the 1.5% allowance for observed recovery. Off/failure does not
resolve an episode or supply a physical onset/repair bound.

The score cannot distinguish a persistent meter gain bias from an equally sized
physical loss. An explicit indistinguishable-observation counterexample must
remain in the tests/report. No guaranteed sub-noise or real-world sensitivity is
claimed. Default-on adoption is conditional on the following executed checks.

## Generated acceptance, fixed before execution

- Retain every existing detector/worker/countdown/response check, including the
  original 30-configuration noisy 7.5-20% deficiency family and exact boundaries.
- Seed 2026110804: 32 unfamiliar configurations, independent uniform +/-1% meters,
  healthy / 3% leak / 3% degradation, continuous operation and two-running/four-off
  cycles. Require no healthy alarms, correct isolated category, and detection
  within four hours of the synthetic onset. This bound is conditional on these
  declared acquisition/regime/noise cases.
- Seed 2026110805: 32 unfamiliar healthy configurations, one day each, independent
  +/-2.5% meters and cycling. Require no false inspection alerts in this finite
  family. This is not a statistical false-alarm guarantee.
- Independently calculate a noiseless 3% step's score/emission. Exercise one/two
  outliers, gaps, invalid stopped flow, rapid timestamps, fault masking, causal
  future/prefix equivalence, reset after observed recovery, six-hour expiry and
  bounded original evidence. Preserve meaningful failures and corrected reruns.
- Run the official week and all four historical cuts through the real candidate
  reader/detector, then invoke the separate offline label evaluator. Retain
  previous results; report all misses, unmatched/early alarms, delays and current
  lifecycle confusion. Unobservable post-repair cuts may remain unresolved.
- Run full pinned CPU science with the real native evaluator and plant configured;
  actual worker checks must still enforce the adopted 72cadd1 boundary. No native
  algorithm changes or new sanitizer execution is implied by Python-only checks.

Publish exact source/evidence identities and send Lucas the worker/method change
and remaining limitations. Search/simulation installed acceptance remains separate.

## Generated-family refinement, before official or fresh-seed execution

The initial 0.25 candidate passes all 41 existing detector methods and the
32-configuration noisy healthy-day family. It fails 29 of the 64 short-cycle
deficiency cases' four-hour latency bounds. The full 13-method candidate run
otherwise passes after fixing two test-fixture mistakes (a running final sample
mislabelled stopped and passing the policy to Archive rather than its View).
Both original failures and the fixture-corrected failure log are retained.

Freeze a **0.175** score threshold; retain the 1.5% allowance, 5% influence cap,
six-hour window and all original acceptance bounds. At a noiseless 3% deficit
this needs 12 running frames, versus 17 at 0.25, allowing the declared two-on/
four-off family to accumulate evidence in about three hours. This is generated-
family-informed refinement, not independent qualification. Before viewing the
new result, also require the same outcomes on 64 fresh configurations at seed
2026110806 and 64 fresh healthy noisy days at seed 2026110807. Only after those
checks will the frozen candidate run on official data; its result remains a
practice evaluation because prior labels were already available to this project.

The 0.175 refinement passes the original families and all healthy fresh days,
but two fresh-seed short-cycle leak cases emit at 4h05m, missing the unchanged
four-hour bound. Retain that failure and mark seed 2026110806 as development
data from this point. Freeze 0.15 for the next candidate, leaving all other
parameters/outcomes unchanged. Validate additionally on **128** new configurations
at seed 2026110808 and 128 healthy noisy days at 2026110809 before official
execution. The independent score oracle will round each exact Fraction sum to
binary64, so an exact mathematical tie is not silently substituted for the
runtime's declared floating-point recurrence.

The 0.15 candidate passes all original/refinement/fresh generated cases. The
expiry fixture needed eight individually insufficient running samples rather than
ten: binary64 accumulation of ten 1.5% increments is slightly above 0.15 and
legitimately emits. The corrected fixture still fails if the two eight-frame
groups are improperly joined across six hours. Retain its original failure.
The first full discovery executes 417 methods: 416 pass, one fails because four
assertions from the old worker fixture were accidentally left in the new test.
Restore those assertions to the original test; do not remove its binding checks.

First official 0.15 execution preserves all 23 matches/no unmatched alarms.
Leak delays are 1h25m-3h10m; degradation delays are 5h30m-6h05m. Some recovery
still requires a later long run because its four good frames reset on off cycles.
Before the next official run, extend **only cumulative episodes' recovery** to
four at/below-allowance running observations across completely observed coherent
off intervals within the same six-hour window. Retain gap/contradiction/fault
resets, original samples and non-running ambiguity. After recovery, discard old
positive evidence while retaining its healthy suffix; a new episode cannot reuse
old discrepancy. Add a generated short-cycle recovery case and rerun the full
unchanged families and real worker. Physical repair is still not authenticated.

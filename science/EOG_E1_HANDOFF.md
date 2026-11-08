# EOG E0/E1 numerical handoff — Ethan / Person A

This increment supplies independent analytical models and known-value fixtures.
It does not deliver the production EOG detector, optimizer, independent plan
validator, capture reader, worker protocol, desktop integration or a full EOG solution.

## Baseline and ownership

- Branch: `codex/person-a-eog-analysis`, isolated in its own worktree.
- Base: `2bd7bb9e13f1f6788a57c4c49e6d7570293ad0eb` (published main checked November 8, 2026).
- G1 head already accepted: `f12a82701641f1b4fd3af9ebba579b79f1db3db4`.
  G1 is not the EOG analysis contract.
- **Adopted EOG contract commit: none.** Published main has no `contracts/eog/`;
  no EOG contract PR was open at the initial remote check.
- PR #32 remains draft at `2beb0cba463918184d74544f1516a278ff14e9ad`.
  Its project-v3 allocation changes overlap G2 in project serialization,
  controller, agent/history/science mapping and desktop CMake/tests. It needs
  Lucas's separate integration review; an allocated v3 project cannot be put
  into the G1 v2 wrapper that requires an unchanged v1 project.
- PR #30 remains a separate open experiment at
  `d53565fcce93dcc9689e7312f2f15c680825574d`; no new qualification is claimed.
- Changes here are exclusively `engine/` and `science/`. No contracts, desktop,
  root CMake/presets/solution, shared status, source-feed work or transport study changed.

The supplied November 8 handoff, section 5, says: “Lucas drafts and publishes;
Ethan reviews the numerical meaning; both adopt the exact committed revision.”
It explicitly allows pure models and analytical fixtures before that gate.
These engine-owned C++ types are a numerical review surface, **not a substitute
wire contract**. Lucas should publish a small versioned capture/analysis contract
and examples before either side binds a provider or desktop process client.

## What Lucas can consume now

`engine/include/breakwater/eog_analytical.hpp` and the separate
`breakwater_eog_analytical` CMake library provide:

- bbl percentage conversion, bpd-to-bbl/hour conversion, oil/gas hourly revenue;
- pure normal-pump hysteresis with explicit fault/restart behavior;
- analytic threshold countdown and repair duration remaining from an assumed onset;
- a small observation fixture that distinguishes available countdown, already
  reached, no future crossing and unavailable evidence;
- deterministic, event-driven evaluation of explicit failed-pump analytical
  fixtures with delayed individual well shut-ins and discrete full truckloads;
- per-system truck identity and resource occupancy, including the final return;
- independent-facility trajectories, explicit automatic trip times, and itemized
  truck/lost-production costs;
- action violations, with absent total costs and explicitly incomplete traces
  when a response cannot be evaluated as supplied.

There is no file access in this library. Tests/example construct inputs directly.
They load no practice answer keys, future telemetry, credentials or model data.
The inputs explicitly assume already-known failures and prospective well rates;
the library does not infer those assumptions from telemetry.

## Units and analytical assumptions

All volumes are bbl; water/oil rates are bpd; gas rates are mcf/day; internal elapsed
times are hours. Response/cycle inputs are minutes. Revenue uses prices per bbl and
per mcf. Nothing uses sandbox continuous trucking costs or m3/h controls.

`Well::productive` means productive before the incident, not the latest observed
status after automatic shut-in. A shut facility can therefore retain its full
remaining revenue exposure. Mapping that state from causal evidence is future E2
work. A finite supported initial volume above the shutdown threshold is preserved
and marked already reached; physically out-of-range initial states are rejected.
Raw unit conversion itself preserves finite negative/above-capacity observations.

Each facility's pump is unavailable until its declared remaining repair deadline.
Its prospective per-well water rates are constant until individual or automatic
shut-in. Trajectories stop at repair. Post-repair pumping/resumption, healthy
network trajectories, transport and leak/degradation physics are outside this
fixture model. The isolated hysteresis function is not a simulator or detector.

The observation fixture takes the caller's historical cutoff, maximum age and
maximum acquisition skew explicitly. It abstains on missing, stale, future,
incoherent, nonfinite or unsupported readings. No one-second G1 freshness policy,
wall clock, calibrated confidence or inferred healthy default is introduced.
Its constant measured net-inflow forecast is a named assumption, not a diagnosis.

Inputs are bounded to 128 facilities, 128 wells per facility, 1,024 truck jobs,
16,384 total trips and a 168-hour remaining horizon. IDs are opaque, bounded local
strings, not a claim of compliance with a future wire ID convention. Invalid
fixture configuration/unrepresentable arithmetic throws `invalid_argument`.

## Proposed exact event ordering for contract review

1. Start from the unmodified initial inventory. An explicitly shut-in facility or
   an inventory already at/above its shutdown threshold is shut from time zero.
2. Integrate the prospective active-well rate to the next event, checking the
   continuous threshold crossing first. Reaching the threshold triggers automatic
   shut-in, including equality at an intervention or repair time.
3. At an action time, check the combined inventory requirement of all simultaneous
   truck arrivals. Loads are fixed and full; no partial pickup or clipping occurs.
4. Record both sides of the discrete removal. Well shut-ins change rates for the
   interval after their action time. Input ordering does not prioritize one load.
5. Repair is the final endpoint. Every arrival must be strictly before repair.
   A trip exactly at repair is invalid; a threshold reached exactly at repair is
   recorded as a trip with zero remaining automatic revenue loss.
6. A truck occupies `[call, last_arrival + cycle)`. A subsequent job can start
   exactly on that return boundary and incurs its own facility's response time.

This is a conservative, testable proposal requiring joint numerical review before
adoption in the EOG contract. Binary64 comparisons are strict for deadlines,
occupancy and crossing decisions; no permissive feasibility epsilon is added.
Assertion tolerances are for checking numerical results only.

Arrivals at/after automatic shutdown are explicitly unsupported/invalid in this
increment. The model does not simulate a wasted post-trip pickup or restart wells
early. It returns a violation and no total response cost rather than silently
dropping or moving that action. Such actions can be assigned richer semantics in
a separately reviewed production evaluator.

## Runnable fixtures and expected results

From this worktree root in PowerShell:

```powershell
.\scripts\build.ps1 -Preset headless -EmbeddedDebugInfo
.\out\headless\engine\breakwater_eog_analytical_tests.exe
.\out\headless\engine\breakwater_eog_analytical_example.exe
.\scripts\build.ps1 -Preset asan -EmbeddedDebugInfo
```

The script initializes the MSVC environment; retain that environment when running
ASan binaries directly. `-EmbeddedDebugInfo` uses `/Z7` and avoids shared PDB-server
requirements. No changes to the B-owned build script/presets are required.

The example uses the **public opening illustration**, not an answer-key record.
The supplied constants are 800 bbl, 50% initially, shutdown at 92%, 1,800 bpd water,
six hours until repair, 150 bbl/load, 75-minute first response, 230-minute cycle,
$905/load, and the three public wells' water/oil/gas values.

| Case | Analytic result |
|---|---|
| No response | `(736 - 400) / 75 = 4.48 h` to trip; `$3,300/h * 1.52 h = $5,016` loss |
| One load called now | Arrival `1.25 h`, truck free `5.083333... h`; final `700 bbl`, `$905`, no trip |
| Public low-revenue well shut at `2 h` | Final `733.333333... bbl`; `$500/h * 4 h = $2,000`, no trip |

The third case deliberately uses an exact two-hour start. It does not claim to
reproduce the README's minute-rounded 02:25 wall-clock alternative or an optimum.

Additional small cases cover simultaneous loads, insufficient inventory, strictly
late/deadline arrivals, delayed/independent well starts, automatic loss without
double counting, already shut-in exposure, two/three competing failures, separate
fleets and sequential yard-return jobs. Deterministic randomized cases compare
against closed-form no-action results and separately summed water/revenue
integrals. These test oracles do not constitute the E4 independent plan validator.

See `science/evidence/eog-e1/verification.json` and retained logs for the actual
commands, toolchain, tested source identities, case counts, results and limits.
No hosted model, full-week dataset qualification, Python/Lean suite, desktop,
installed application or new performance benchmark is claimed by this increment.

## Next dependency and work

Lucas should publish the exact EOG capture/operation contract commit. Numerical
review needs explicit bbl/bpd units, causal cutoff/capture identity, freshness,
onset/repair assumptions, the event ordering above, unavailable results, exact
per-truck schedules, component costs and search/independent-validation semantics.
The current fixture API must not be exposed as an already accepted protocol.

Then proceed to E2 causal event detection and E3 production evaluation against
that adopted contract, followed by E4 bounded joint search and an independently
coded validator. E5 official-data evaluation remains separate from production
inputs and must describe the correlated full-week/snapshot/mock samples honestly.

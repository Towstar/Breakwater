# Executed private stateful synthetic plant

Corrected native source **af73a42**, method **eog-synthetic-events-0.1.0**. Latest
main c1622b9 is merged as 7323968 with identical tested engine source. This is a
native subsystem increment, not complete application simulation acceptance.

| Check | Actual final result |
|---|---|
| Normal native CTest | **12/12**, 3.56 s |
| MSVC ASan native CTest | **12/12**, 26.15 s; no sanitizer diagnostic |
| New synthetic suite, each mode | **57,498 checks** |
| Analytical cases | Hysteresis, failure/automatic shut-in/repair, full/repeat pickups, strict deadlines/ties, well loss union and rate changes |
| Persistent resources | Shared busy truck survives repair; sequential yard job repays response; repeated/concurrent incidents |
| Observation partitions | **100** runs, single advance versus many observation checkpoints, exact final inventory/ledger/cost equality |
| Actual E3 cross-checks | **100** native evaluator comparisons, exact cost and final/full-load inventory agreement |
| Month-scale cycle | **31 days**, independent periodic oracle and exact reset repeatability |
| Lifecycle and bounds | Real cancellation/current-clock cancel, reset, invalid input/action rejection and explicit bounded trace |

Final month-scale inventory error: **4.9828259562900712e-7 bbl** against the
independent continuous-cycle calculation; raw mass residual **-8.5472594690827464e-7
bbl**, with **6.9979188310753671e-7 bbl** signed event-boundary rounding separately
reported. This rounding audit is never described as a physical source/sink or
used to zero the raw residual. Existing tolerance is unchanged at 1e-5 bbl for
cycle error and 1e-6 bbl for mass residual in this specified case. These are
qualification bounds, not a universal accuracy proof.

Initial source **68686c1** failed the month-scale oracle at **2.5875685963683281e-5
bbl** error. Logs retain that failure, the rejected predecessor-time attempt that
broke exact short-cycle behavior, the algebraic-root correction, and final checks.
The fixed method uses algebraic event boundaries as E3 does, preserves physical
anchors across samples/zero-effect forcing, and keeps per-well loss anchors
independent of pump/pickup breakpoints. No invalid source volume is projected into
range, no partial pickup is substituted and no acceptance tolerance is relaxed.

The actual E3 evaluator is separately coded but shares declared physical rules,
compiler/platform and indexed fixture inputs. It is not external physical truth
or an independent long-run simulator. Qualification timings are not optimized
benchmarks; normal and checked results are retained separately. No Python/model/
proof/desktop/installation execution is newly claimed by these native checks.

`verification.json` pins tested source blobs/raw and normalized hashes, normal and
checked binaries/runtime and retained payload bytes. Source records distinguish
the historical failing implementation. `search-ci-provider-failure.txt` and its
metadata retain a separate genuine PR #46 hosted Qt failure: the test still expects
search to be unavailable. Lucas's requested B-owned follow-up is linked in the
coordination note; that hosted failure is not relabeled as a native simulation pass.

To reproduce, use the supported Windows MSVC environment:

```powershell
.\scripts\build.ps1 -Preset headless -EmbeddedDebugInfo
.\scripts\build.ps1 -Preset asan -EmbeddedDebugInfo
```

Next requirements remain open: real imported model/ID mapping, seeded forcing
provenance and causal sensor projection, stateful planner commitments, adopted
application contracts, actual recorded/counterfactual/extended controls and final
rendered/installed checks. See `engine/EOG_SIMULATION.md` for supported semantics,
limits and the required B-owned mapping. The active goal is not complete.

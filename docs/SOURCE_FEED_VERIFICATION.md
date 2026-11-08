# Explicit multi-tank source feeds: verification

Implementation revision: **675e926b7baf4db13b829d7d250114f44f339198**. The isolated
`codex/source-feed-connection` worktree started at `7f2bb7f` and integrated main
`ce35c81` in `eac3c5b`, retaining both documentation histories. The original root
checkout, other worktrees, local handoff copies and original running application
were preserved. The ignored local continuation plan is excluded.

## Change and compatibility

The original application rejected adding a second Production B feed with “Each
source feeds exactly one tank. Delete its existing route first.” Removing that
guard alone would have duplicated production on every branch. The correction adds
explicit integer shares in hundredths of one percent, totaling exactly 10000.
Every branch receives its configured fraction of production after curtailment;
branch capacity, cost and derivatives use that flow. Shares are configuration,
not new search controls. No pressure/valve solver or PDE stencil was introduced.

Contracts were committed separately in `efd1c48`; the initial implementation is
`8408825`. Compatibility/regressions are `2795b25`; the final keyboard-editor
correction is `675e926`. Existing single-feed projects retain version 1 and their
serialization/hash behavior. Allocated projects use **version 3**, preserving G1's
reserved project-v2 telemetry envelope. Storage method/result version 2 is
independent of project serialization. Old worker versions explicitly reject split
execution; old project readers cannot silently ignore shares. Known legacy native
`constant-flow-0.1.1` science evidence remains readable and non-actionable.

At the owner's request, [Person A accepted G1's exact `f12a827` contract](https://github.com/Towstar/Breakwater/pull/31#issuecomment-6719191863).
The PR's A-side acceptance checkbox is checked; B can adopt that same revision
without waiting for this feature or PR #30. [Fresh adoption checks and scope](../science/G1_CONTRACT_ADOPTION.md)
are separate from source-feed verification. G2 and telemetry support for allocated
projects require their own contract extension; no G1 implementation file was changed.

## Executed checks

[The manifest](evidence/source-feed-verification/manifest.json) records exact source,
executable, configuration, raw-attempt and log hashes, registered test names and
compile/link flags. Committed log copies remove trailing whitespace; raw originals
remain in `out/connection-repro/` with their original hashes.

| Check | Observed result |
|---|---|
| Full pinned Python suite | **210 passed, zero skipped**, 102.052 s |
| CI workflow/result policy regressions | **15 passed**, 5.021 s after creating the missing ignored local `.tools` prerequisite |
| Native correctness | All **9 native CTests** passed in each build, including **12 new source-feed**, 22 engine, 22 search, 15 influence and 25 transport cases |
| Initial full desktop/native attempts | **27/30 passed** in each build; a new legacy-archive fixture failed and blocked two dependent History renders; 541.51/624.67 s normal/ASan |
| Final affected tests | **8/8 passed** normal/ASan, 82.74/169.38 s, including the new actual-QML keyboard regression, corrected science-evidence suite and History/Science renders |
| Passing registered coverage | **31 distinct CTests per build** across the complete attempts and affected reruns; a fresh unrestricted 31-target run was not repeated |
| Individual logged Qt suites | **416 passed, zero failed/skipped** in each build's 13 final logged suites; the separate project suite also passed in the complete attempts |
| Legacy cross-language check | All **18 fixtures**, three native solvers, 44 interior controls and 132 central differences passed; actual worker check executed **302 requests** |
| New source-feed check | **4 cases**, three native solvers, independent affine expectations, standalone Torch and **24 real-worker requests** passed in each build |
| Installed worker | **24 requests passed** from installed modules outside the source worktree; source/module hashes match, both test-history SQLite checks returned `ok` |
| QML lint | Completed successfully; existing unqualified-access/Shortcut warnings remain recorded |
| ASan | No sanitizer diagnostic found in the retained build/test records; prebuilt Qt/Python/Torch are outside established instrumentation coverage |

Toolchain: MSVC **19.51.36260**, Qt **6.12.0**, CMake/Ninja, C++20, RelWithDebInfo,
`/fp:precise`, `/Z7`, `/O2 /MD`, `/debug /INCREMENTAL:NO`; the separate ASan build
adds `/fsanitize=address`. Python **3.12.13**, Torch **2.15.0+cpu**, NumPy **2.5.4**,
PhysicsNeMo **2.2.2**, jsonschema **4.25.1** were used without pin changes. Elapsed
times are observations during concurrent work, not benchmarks. No new Lean audit,
hosted model rehearsal or PDE accuracy qualification is claimed.

## Numerical evidence

The private executed fixture starts with independently specified physical inputs.
It does not construct its oracle from native result arrays.

| Case | Shares | Curtailment m³/h | Expected final inventories m³ | Expected cost |
|---|---|---:|---|---:|
| Balanced | 50/50 | 0 | 104 / 54 | 72 |
| Asymmetric | 75/25 | 2 | 111.5 / 40.5 | 70.5 |
| Four-way | 10/20/30/40 | 4 | 3.6 / 7.2 / 10.8 / 14.4 | 168 |
| Inactive branch | 0/100 | 2 | 80 / 72 | 102 |

Every requested timestamp/index, tank identity/order and method identity is exact.
The unchanged forward tolerance is `1e-9 + 1e-10*max(abs(a),abs(b))`. New worker
maximum volume errors against the analytical fields are at most **3.41061e-13 m³**;
cost errors are zero. Analytical-gradient error is at most **2.22045e-16**, and the
largest error over the three central-difference sizes is **5.68071e-9** under the
existing `1e-6 + 1e-4*max(...)` gradient tolerance. Legacy comparison maximum volume
error is **7.67387e-13 m³**. Installed worker maximum error is **4.54748e-13 m³**.
Signed conservation and independent forged-result checks remain enabled; ledger
residuals, full metrics and provenance are in the two numerical evidence JSON files.

Tests reject malformed/incorrect integer totals, duplicate destinations, source
pumps/unavailability, capacities exceeded by any branch, unsupported tank-route
shares and fabricated source-route controls. They cover deterministic ranking,
four-target influence dispatch, zero-share/zero-capacity behavior, exact version
negotiation and provenance drift, stale edits, immutable captures, layout survival,
undo/redo, project save/load, and actual source-split Science/pipe/history validation.

## Preserved failures and corrections

- Attempts 07/08 failed verification-harness assumptions: the installed distribution
  is `nvidia-physicsnemo`, and worker provenance retains its `+worker-v1` suffix.
  They remain separate from successful attempts 09/14/20.
- Complete attempts 13/19 exposed a bad new historical fixture: constructing a new
  capture with deliberately old provenance correctly threw. A subsequent fixture
  edit changed the outer details rather than `runs[0].run`; attempts 25/26 retain
  that failure. The final fixture represents a consistent historical record and
  rejects unknown provenance; final science-evidence runs report **44 passes**.
- Early QML tests looked for Repeater controls in the QObject tree instead of the
  visual tree. After correcting that lookup, real keyboard input exposed live
  SpinBox formatting that converted typed `75` into `7`, while the interactive
  paste path displayed an edit without synchronizing the applied shares. Both
  unsuccessful attempts are retained. The final TextField keeps typing separate
  from formatting, updates shares/flows/totals together, disables Apply for empty
  or invalid input, and passes keyboard, Cancel and 75/25 Apply regressions.
- The initial CI-policy run lacked the worktree-local `.tools` directory. Creating
  that ignored prerequisite resolved it without changing dependencies or policy.
- Ninja log-recovery and optional Qt translation/Vulkan warnings are retained.
  The installed English interface and actual worker execution succeeded.

## Operator observations and limits

The dedicated installed instance used temporary projects/history. A saved v3
50/50 project opened through the startup argument and displayed two source pipes
and **8+8 m³/h**. The split dialog opened; Cancel preserved the original allocation.
An actual native forecast displayed **104/90/24 m³ at 3 h**, cost zero and minimum
headroom 16 m³. This canonical UI case has different initial conditions from the
private numerical fixture above. No simulated approval was clicked.

[Observed forecast](evidence/source-feed-verification/observed-three-hour-forecast.png)
and [UI observations](evidence/source-feed-verification/ui-observations.json) are
separate from numerical reports. New-connection creation via scene picking and
Network selection was attempted without an observed successful completion; the
actual common QML connection/Apply path is covered by the private automated test.
Native Open-dialog selection was blocked in the automation helper and cancelled;
actual Save/Open-dialog roundtrip remains **unverified**. A final interactive retry
stopped at user input. The final editor's keyboard acceptance is automated evidence,
not an interactive-workflow claim. These limitations remain visible.

The updated disposable installation is
`C:/Projects/Breakwater/out/source-feed-preview`. `Launch-Breakwater.cmd` selects
the existing pinned interpreter and a separate preview history database. The
original running executable and user installation were not replaced. This is a
locally tested feature for review, not a clean-machine or application-wide quality
guarantee. Existing F1/F1b failed accuracy targets and original proof scope remain.

## Reproduction

Run from the isolated feature worktree. Use a fresh output directory for each
verification attempt and retain failures.

```powershell
& 'C:/Program Files/Microsoft Visual Studio/18/Community/Common7/Tools/Launch-VsDevShell.ps1' -Arch amd64 -HostArch amd64 -SkipAutomaticLocation
cmake -S . -B out/connection-repro/desktop-normal -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_MSVC_DEBUG_INFORMATION_FORMAT=Embedded -DBUILD_TESTING=ON -DBREAKWATER_BUILD_DESKTOP=ON -DBREAKWATER_ENABLE_ASAN=OFF -DCMAKE_PREFIX_PATH=C:/Projects/Breakwater/.tools/Qt/6.12.0/msvc2022_64 -DPython3_EXECUTABLE=C:/Projects/Breakwater/.worktrees/person-a-engine/science/.venv/Scripts/python.exe '-DCMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO=/debug /INCREMENTAL:NO'
cmake --build out/connection-repro/desktop-normal --parallel 4
ctest --test-dir out/connection-repro/desktop-normal --output-on-failure
```

For ASan, use a separate `desktop-asan` directory and `BREAKWATER_ENABLE_ASAN=ON`.
Prepend the Qt `bin` directory to PATH. Numerical runs set OpenBLAS/OMP/MKL thread
counts to 1 and use the pinned interpreter. The exact final affected command is:

```powershell
ctest --test-dir out/connection-repro/desktop-normal --output-on-failure -R '^desktop.(source_feed_qml|science_evidence|scientific_history_normal|scientific_history_compact|qml_smoke|storage_science_normal|storage_science_compact|storage_science_derivatives)$'
& 'C:/Projects/Breakwater/.worktrees/person-a-engine/science/.venv/Scripts/python.exe' -m unittest discover -s science/tests -v
& 'C:/Projects/Breakwater/.worktrees/person-a-engine/science/.venv/Scripts/python.exe' science/verify_rollout.py --native out/connection-repro/desktop-normal/engine/breakwater_rollout_reference.exe --report out/new-attempt/rollout.json
& 'C:/Projects/Breakwater/.worktrees/person-a-engine/science/.venv/Scripts/python.exe' science/verify_worker.py --native out/connection-repro/desktop-normal/engine/breakwater_rollout_reference.exe --report out/new-attempt/worker.json
& 'C:/Projects/Breakwater/.worktrees/person-a-engine/science/.venv/Scripts/python.exe' science/verify_source_feeds.py --native out/connection-repro/desktop-normal/engine/breakwater_source_feed_reference.exe --output out/new-source-feed-attempt
cmake --build out/connection-repro/desktop-normal --target breakwater_qmllint
cmake --install out/connection-repro/desktop-normal --prefix C:/Projects/Breakwater/out/source-feed-preview
```

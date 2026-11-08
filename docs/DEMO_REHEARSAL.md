# Live Nemotron rehearsal

Use the production desktop investigator and its existing operator review dialog.
The launch options below prepare one investigation; they never approve a plan or
choose a tool sequence, candidate, cost or final run for the model.

## Prepare and launch

Build the current desktop, configure the NVIDIA key through **Set API key**, and
set up the local CPU worker as described in [DESKTOP_SCIENCE.md](DESKTOP_SCIENCE.md).
Keep credentials out of project files, commands, screenshots and exported evidence.
Choose a fresh database and a new JSON destination for each attempt. From the
repository root in PowerShell 7:

```powershell
$attempt = Join-Path $PWD 'out/live-rehearsals/canonical-1'
$objective = 'Investigate this incident over the full horizon. Use stored numerical evidence and scientific sensitivity analysis where useful to explain the controls. Find and independently validate a feasible response, compare alternatives, and stage the selected run for operator review. State the scope and limitations of the evidence.'
.\scripts\Start-Desktop.ps1 -AppArguments @(
  '--history-db', "$attempt.sqlite",
  '--agent-panel', '--investigate', '--objective', $objective,
  '--rehearsal-archive', "$attempt.json"
)
```

`--investigate` requires one explicit, nonempty objective (at most 4,000 characters).
Without `--project`, it loads the canonical **Pump failure** incident before starting;
the healthy default or an earlier applied plan cannot silently become the next
canonical attempt. Investigation starts only after the history database is ready.

For the edited-network attempt, add `--project 'C:\path\edited-project.json'`.
The file must be a valid project-v1 file no larger than 2 MiB. Make a separate
project with renamed asset and route IDs and a real topology change, such as an
added buffer tank and a redirected transfer. Save it through the project editor.
Simple relabeling does not establish topology adaptation. All runs retain the
three-hour planning horizon.

Invalid/missing/duplicate startup values, an invalid project, an existing archive
destination, or mixing investigation with smoke/import/science-check modes stops
before investigation with exit 8. History startup failure uses exit 9. Normal
interactive launches and project-v1 contents are unchanged.

## Complete each attempt

1. Observe actual model-issued calls and numerical outcomes. Tool errors can be
   returned to the model for correction. A no-plan, failed, cancelled or unstaged
   advisory result remains evidence, but does not complete a rehearsal.
2. When the investigation finishes with a current staged recommendation, select
   the recommended run and choose **Review staged plan**. Inspect the exact run
   ID, independent validation reference, current/proposed controls, units and
   margins. Choose **Apply simulated controls** in that dialog only when the
   evidence supports it. No startup option replaces this action.
3. Wait for the updated native forecast. The title **Approved rehearsal archived**
   means the matching approval checkpoint was read back from SQLite and exported
   to the requested new JSON file. Export requires a fresh feasible native capture
   matching the approved effective network, revision and recommendation identities.
   Changing physical state while this check is pending prevents success. A storage
   error or 30-second post-approval timeout shows **Rehearsal archive failed**
   with a bounded reason code;
   keep the database and investigate the failure.
4. Close the app, reopen the same database without `--investigate`, and inspect
   History, native trajectories, approval evidence and the separately retained
   scientific result. Reopened records remain historical and non-actionable.

Repeat from a fresh launch/database for `canonical-2` and `canonical-3`, then run
the saved edited project as `edited-1`. Preserve failed attempts and their databases
alongside successful attempts. Do not overwrite a failure with a retry or count the
verification-only live-check executable as an operator-approved demonstration.

## Audit retained evidence

At least one successful model-issued `analyze_sensitivities` call must be present
across the four complete rehearsals. The local worker's separate immutable science
session must also be retained in one of the supplied databases. The model chooses
which eligible captured controls to analyze; the objective does not fix call order.

```powershell
$evidence = Join-Path $PWD 'out/live-rehearsals'
.\.tools\science-cpu\Scripts\python.exe -B scripts/Verify-LiveRehearsals.py `
  --canonical "$evidence/canonical-1.json" `
  --canonical "$evidence/canonical-2.json" `
  --canonical "$evidence/canonical-3.json" `
  --edited "$evidence/edited-1.json" `
  --science-db "$evidence/canonical-1.sqlite" `
  --science-db "$evidence/canonical-2.sqlite" `
  --science-db "$evidence/canonical-3.sqlite" `
  --science-db "$evidence/edited-1.sqlite" `
  --output "$evidence/audit.json"
```

The script is read-only except for the optional new summary file. It checks distinct
completed sessions, exact canonical fixture state, a renamed changed topology,
complete native trajectories, matching selected validation/staging/approval,
successful hosted model identity/HTTP/timing/retry metadata, exact tool-call/result
identities and later model continuation. Successful scientific outputs must agree
with the separately retained source archive, origin and full native capture. It
rejects credentials/raw-reasoning fields and archives over the existing 512 MiB
bound. Default model: `nvidia/nemotron-3.5-lightning-30b-a3b`; `--model` can specify
another deliberately configured model identity.

Exit 0 means these evidence gates passed; exit 1 means a gate failed; exit 2 means
invalid CLI usage or an unwritable/existing output destination. The audit establishes
internal consistency of retained application evidence, not cryptographic execution
authentication. Observe the live UI action and preserve the source databases. Missing
credentials or incomplete traces leave the live demonstration explicitly pending.

Run the auditor's regression checks without a model, worker or credentials:

```powershell
.\.tools\science-cpu\Scripts\python.exe -B scripts/tests/test_live_rehearsals.py
```

These ten checks use clearly synthetic, temporary fixtures only. They cover the
consistent format and rejection of missing approvals, wrong call IDs, missing/wrong
hosted metadata, missing continuation, malformed/duplicate/nonfinite/credential JSON,
missing retained science, duplicate topology IDs and validation/staging mismatches.
They are software regression checks and never count as a live rehearsal.

## Executed live evidence

Local source checkpoints `ca84c0c` and its latest-main merge `3b3127f` produced the
actual successful set below. The merge incorporated PR #26 without changing this feature's
runtime files. Each session used the production investigator, real hosted tools,
native independent validation, the existing operator review/apply action and a
fresh feasible post-approval forecast before automatic export. These are observed
runs; their different search grids and outcomes are not a reliability benchmark or
a claim of global optimality.

| Actual local archive | Finite search inputs | Native runs | Selected cost (synthetic) | Margin m³ | Tools / HTTP requests | Investigation elapsed |
|---|---:|---:|---:|---:|---:|---:|
| `out/agent-rehearsals/canonical-7.json` | 108 | 6 | 9,030 | 13 | 9 / 10 | 53.534 s |
| `out/agent-rehearsals/canonical-9.json` | 81 | 4 | 12,780 | 16 | 10 / 11 | 38.788 s |
| `out/agent-rehearsals/canonical-11.json` | 720 | 6 | 4,800 | 4 | 7 / 8 | 39.313 s |
| `out/agent-rehearsals/edited-2.json` | 64 | 6 | 4,800 | 9 | 8 / 9 | 54.912 s |

The [retained audit summary](evidence/live-nemotron-rehearsals.json) passes all four
sessions with no errors. All 38 completed requests report
`nvidia/nemotron-3.5-lightning-30b-a3b`, HTTP 200 and zero retries. Exact model-issued
call IDs, executed results, subsequent model continuation, native validation and
the one matching operator application per session were checked. Each native run
retains all 121 original samples over three hours. All four SQLite databases returned
`ok` from `quick_check`. The JSON/SQLite source files remain local evidence; the
committed summary is not a replacement for those source records.

The [edited project-v1 input](examples/rehearsal-edited-project.json) is reproducible
with `--project`. Every asset/route ID is renamed; Tank A starts at 75 m³, a new
180 m³ buffer starts at 20 m³, and bypass now transfers from A to the buffer.
The model selected source curtailment/bypass/trucking of 0/20/20 m³/h while leaving
backup at 16 m³/h. All six edited runs retain the full three-tank trajectories.
Its actual validation reference is `21ee68fb-70c1-4d35-aad0-be0351915052`.

Canonical 7 chose network → baseline → controls → search → validate → stage →
sensitivity catalog → sensitivities → stage → final. Its separate scientific result
`d4c4f795-e3f7-4942-860e-d90f0f69b7cb` matches the exact model call/session/incident/
effective run origin and full retained native capture. At captured curtailment 5 and
bypass 16 m³/h, its terminal-volume Jacobian is `[[-3,-3],[0,3]]` h and cost gradient
`[750,0]` synthetic units/(m³/h). CPU float64 Torch returned native/worker cost 9,030
with zero sampled difference. Analysis followed initial staging; this trace does
not show a changed numerical experiment caused by the derivatives. Canonical 9
requested no scientific tool; canonical 11 and edited 2 inspected the catalog
without requesting analysis. Those sessions correctly have no scientific archive.

After restarting with canonical 7's actual database, History reopened its six native
runs and 121-frame charts. The baseline exceeds Tank A's 120 m³ capacity (terminal
approximately 236 m³); its selected candidate ends near 107 m³. Historical approval
was unavailable. The separate scientific record reopened with native/worker cost
9,030, zero difference, captured values 5/16 and the same derivatives. Viewing these
historical records applied no controls. Explicit Reset clicks were observed after
canonical 9, canonical 11 and edited 2, returning to the healthy 44/16 state with
old review unavailable. Canonical 7 was followed by a fresh canonical launch, rather
than a recorded Reset-button click.

Recheck the exact retained set against a new output destination:

```powershell
.\.tools\science-cpu\Scripts\python.exe -B scripts/Verify-LiveRehearsals.py `
  --canonical out/agent-rehearsals/canonical-7.json `
  --canonical out/agent-rehearsals/canonical-9.json `
  --canonical out/agent-rehearsals/canonical-11.json `
  --edited out/agent-rehearsals/edited-2.json `
  --science-db out/agent-rehearsals/canonical-7.sqlite `
  --science-db out/agent-rehearsals/canonical-9.sqlite `
  --science-db out/agent-rehearsals/canonical-11.sqlite `
  --science-db out/agent-rehearsals/edited-2.sqlite `
  --output out/agent-rehearsals/audit-recheck.json
```

The original invocation returned exit 0 and wrote `out/agent-rehearsals/audit.json`.
A separate delivery bundle, `Breakwater-live-Nemotron-evidence-verified.zip`, retains
the four successful archives/databases, nine unsuccessful-attempt archives/databases,
edited input, audit and SHA-256 manifest. Bundle SHA-256:
`a878fa3cecbc222f629d69d01312d8f64de835c6f86ce4fa84186a7e6787f0ed`.
It is a local delivery artifact rather than a dependency of this checkout.

Unsuccessful/no-plan/timeout/budget/unknown-finish attempts remain separate and are
not counted as clean rehearsals. Edited 1 failed the tool limit with zero approvals,
while its completed scientific result remained durable and passed science-only
origin/capture checks. Canonical 4 retained an approved backend record but failed
automatic export; the original failure was not reproduced by the current-source
full-signal test. Neither result is relabeled successful. Broader G4 native-dialog,
clean-Windows and relocated-release checks, telemetry work, and the remaining manual
candidate-forward Science workflow remain separately scoped.

## Combined local checks

The final integrated checks passed 29/29 CTests in each normal/ASan build, with
408 Qt cases per build. Full Python discovery ran 127 tests: 123 passed and four
optional PhysicsNeMo tests skipped in this CPU environment. See
[the check manifest](evidence/agent-tools-verification.json) for toolchain versions,
timings and local log/binary hashes. Prebuilt Qt/Python/Torch are outside established
ASan instrumentation coverage.

From the repository root in a VS 2022 x64 Developer PowerShell, with Qt's `bin`
on PATH, the executed setup/build/test commands are reproducible as follows:

```powershell
$breakwaterQt = (Resolve-Path '.tools/Qt/6.12.0/msvc2022_64').Path
$breakwaterPython = (Resolve-Path '.tools/science-cpu/Scripts/python.exe').Path
.\scripts\Generate-VisualStudio.ps1 -Generator 'Visual Studio 17 2022' -EmbeddedDebugInfo -QtPrefix $breakwaterQt -PythonExecutable $breakwaterPython
MSBuild.exe Breakwater.slnx /p:Configuration=RelWithDebInfo /p:Platform=x64 /m:1
ctest --test-dir out/visual-studio -C RelWithDebInfo --output-on-failure
& $breakwaterPython -B -m unittest discover -s science/tests -v
cmake -S . -B out/agent-asan -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBREAKWATER_BUILD_DESKTOP=ON -DBUILD_TESTING=ON -DBREAKWATER_ENABLE_ASAN=ON -DCMAKE_MSVC_DEBUG_INFORMATION_FORMAT=Embedded "-DCMAKE_PREFIX_PATH=$breakwaterQt" "-DPython3_EXECUTABLE=$breakwaterPython"
cmake --build out/agent-asan --parallel 2
ctest --test-dir out/agent-asan --output-on-failure
```

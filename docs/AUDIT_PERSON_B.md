# Person B foundation audit and B5 gate

Baseline: `10cb8f7c3df5a83d100bd9d7cb5e1ffc116dbe6b`, November 7, 2026.
The published merge equivalent is `ac22a1cf06ac9549682be12ca9875fe5bc90f477`,
with the identical source tree `9b078c75479c1b1d6949b184ee73a523f4f31921`.
This includes the open B7 workstream and PR #14, plus the new audit-first handoffs.
This is a source/boundary audit with fresh automated checks. Manual dialog/restart
closure and installed scientific execution are tracked separately below.

| Requirement | Inspected implementation and evidence | Outcome |
|---|---|---|
| Editor/project | ProjectDocument strict IDs/types/budgets; ProjectController drafts, undo/redo, atomic save/load, numerical versus layout/profile hashes; project tests | No P0/P1 found in scope |
| State/approval | AgentController/gateway stored validation, selected/staged run/hash/revision, historical rejection, explicit application; gateway/controller regressions | No P0/P1 found in scope |
| Worker | A8 framing/mapping/contracts; health client identity/byte/deadline/crash handling and environment redaction; actual worker tests | B5 deployment/client gaps below |
| Pipe viewer | Captured compile/availability/source flow, parameter identity, asynchronous publication, signed native ledger, historical import isolation; pipe suites | No P0/P1 found in scope |
| Persistence | Dedicated SQLite connection thread, transactional immutable checkpoints, queued teardown, bounded historical import/collision checks; repository/history suites | No P0/P1 found in scope |
| Nemotron | Native tool dispatch, immutable snapshot/horizon, stored validation/staging, 12 calls/3 rounds/120 seconds, fabricated IDs/errors, HTTPS redirects/metadata/redaction | No P0/P1 found in scope; fresh hosted rehearsals not run |
| Delivery | Generated solution graph, Qt/QML/Sql, worker source fallback and install rules, local runtime discovery | Installed science gap; B5 gate below |

Fresh commands/results on this checkout:

- VS2022 x64 developer shell, MSVC 19.44, Qt 6.12.0. Direct `MSBuild.exe
  Breakwater.slnx /p:Configuration=RelWithDebInfo /p:Platform=x64 /m:1`: passed,
  zero warnings/errors. `ctest --test-dir out/visual-studio -C RelWithDebInfo
  --output-on-failure`: **20/20 passed** (21.59 s).
- Full desktop MSVC AddressSanitizer existing build: `cmake --build out/agent-asan
  --parallel 2`, then `ctest --test-dir out/agent-asan --output-on-failure`:
  **20/20 passed** (59.48 s). Python health tests used an actual bundled 3.12.14
  interpreter, not a WindowsApps alias. These checks do not establish Torch execution.
- Logs: `out/b5-audit-vs-build.log`, `out/b5-audit-vs-tests.log`,
  `out/b5-audit-asan-build.log`, `out/b5-audit-asan-tests.log`.
- Independent same-family source reviews covered worker/Nemotron authority and
  editor/state/persistence/pipe boundaries; root reviewed editor and delivery callers.
  Tests are execution evidence, not a substitute for those scoped reviews.

## Findings and requirement gaps

| ID / priority | Owner / evidence / reproduction | Impact and resolution / acceptance |
|---|---|---|
| B-01 / P2, existing | B; PythonWorkerClient retire/fail; timeout then immediately request again | Previous child may still terminate when replacement starts. B5 retains retirement ownership and serializes replacements. Test live-child overlap, cancellation, queued retry and destruction. |
| B-02 / B5 requirement gap | B; desktop install copies worker.py only; installed capabilities reports mapper absent | Install worker_storage plus package and configure pinned runtime. Copied installed app must execute real storage with source checkout unavailable. |
| B-03 / B5 requirement gap | B; health-only client/UI; capability implementation is not execution | Add same-process capabilities/storage/capabilities, strict results and clean-exit publication; verify false-to-true and reset on a new process. |
| B-04 / P2, existing metadata | B with A endpoint convention; capturedRun lacks original config/native reference, uses last sample as horizon | Retain original config and immutable native arrays; compare indexed grid without guessing step or rewriting horizon. Include shortened-step and known one-ULP endpoint regression. |
| B-05 / P2 delivery | B; development source fallback can mask incomplete install | Restrict fallback to canonical development binary directory. Test installed bundle with source paths unavailable, including missing worker/module cases. |

No P0/P1 source blocker was identified in this bounded audit. B-01/B-04/B-05 are
included in B5 implementation and validation. Runtime preparation is authorized and
uses ignored project-local tools/environments; scientific locks and A-owned source
are preserved.

## Remaining evidence closure

B5 closure: STATUS records 24/24 normal/full-desktop ASan suites, 32 real Python tests,
290 worker/native requests, actual installed execution with original science sources
unavailable, and explicit missing-runtime/module cases. B-01/B-04/B-05 fixes passed
regressions. Actual GUI comparison/state/layout/history checks passed at both sizes.
GUI cancellation raced completion; filename entry blocked dialog save/reopen completion.
Those limits and the missing comprehensive A audit remain explicit.

- B5 GUI checks passed at both sizes; renamed/custom execution and cancellation
  passed automated checks. GUI in-flight cancellation still raced completion.
- Actual dialog filename entry/save/reopen remains unverified. Backend restart,
  immutable restoration, project save/load and exports passed their suites.
- Local pinned Torch/native and source-independent installed execution passed.
- A's comprehensive `science/AUDIT_PERSON_A.md` is not present at this baseline;
  upstream numerical/scientific/proof reports are not labeled as this audit's execution.
- Full release gates, scientific history and live Nemotron rehearsals remain separate.

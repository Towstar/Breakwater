# A-004: accurate scientific-worker status

Person A documentation correction on `codex/person-a-engine`, after `0c2efe9`.
A-004 is resolved. The four reported Person A audit findings now have linked
resolutions in [AUDIT_PERSON_A.md](AUDIT_PERSON_A.md). This does not complete Person
B's audit, installed-app verification, or broader scientific qualification.

## Corrected distinction

At A4 delivery the worker supported health only. That historical milestone remains
recorded in [PROGRESS.md](PROGRESS.md). A8 subsequently added repository
`capabilities`, `storage_rollout`, and selected-control terminal Jacobians/cost
gradients under the published contract. [PHYSICSNEMO.md](PHYSICSNEMO.md) now states
that progression and links [WORKER.md](WORKER.md), instead of calling the current
repository worker health-only or waiting for an unpublished contract.

The legacy `health` result intentionally retains `methods: ["health"]` and
`scientific_execution: "not_implemented"` for compatibility. Those fields are not
the repository's scientific capability inventory. New callers use `capabilities`.

Storage `implemented: true` currently detects the mapper file beside the worker.
It does not verify a complete deployment, importable Torch runtime, successful
numerical execution, native-run identity or approval. Storage's process-local
`execution_verified` becomes true only after a real result passes publication
checks and the output budget; it resets on restart.

Python `pipe_transport` is explicitly `not_implemented`, artifact support is false,
and there is no generic PhysicsNeMo inference endpoint. Native FVM remains the
selected-pipe authority. The separately executed A4/A-003 model remains fixed-case
experimental quality evidence, separate from volume feasibility and approval.

The desktop source inherited by this branch from `6fb8c29` still requests health
only and installs only `worker.py`. Documentation now identifies that snapshot
rather than making a claim about Person B's advancing parallel branches or an
installed application. B owns integration and runtime packaging.

## Documentation changed

- `science/PHYSICSNEMO.md`: replaces the stale current-tense statement with A4/A8
  chronology, current repository methods, unavailable quality methods, and the
  scoped desktop/package boundary.
- `science/WORKER.md`: clarifies mapper presence versus execution, labels the
  inherited desktop snapshot, and distinguishes historical A8 counts from later
  audit-fix evidence.
- `science/PROGRESS.md`: labels dated entries as milestone-time snapshots; original
  A3/A4 health-only statements and measured evidence remain intact.
- `science/AUDIT_PERSON_A.md`: lists current resolutions for all four findings and
  clearly labels the original findings/gate as frozen `6fb8c29` evidence.
- This resolution note records the source check and small actual-worker smoke.

## Verification

Read-only source checks at `0c2efe9` covered:

| Claim | Source |
|---|---|
| Registered methods, legacy health, implementation flag and publication transition | `science/worker.py` |
| Storage calculation, selected derivatives and echoed provenance | `science/worker_storage.py` |
| Standalone learned-model training/reference comparison | `science/physicsnemo_smoke.py` |
| Desktop health-only client in this branch | `desktop/science/PythonWorkerClient.cpp` |
| One-file worker installation in this branch | `desktop/CMakeLists.txt` |
| Authoritative native pipe execution | `desktop/quality/PipeTransportController.cpp` |

A small actual NDJSON smoke executed **16 requests in four profiles**, using the
committed contract examples and existing Python 3.12.13 environment:

| Profile | Requests | Observed behavior |
|---|---:|---|
| Repository worker | 6 | Legacy health unchanged before/after storage; capabilities starts unverified; storage plus selected derivatives completes; verification becomes true; Python pipe request fails explicitly |
| Fresh process | 1 | Storage execution verification resets to false |
| Repository with site packages disabled (`-S`) | 4 | Health/capabilities work; mapper implementation is present; storage returns `runtime_unavailable`; verification stays false |
| Copy of `worker.py` alone, with `-S` | 5 | Health/capabilities work; storage implementation is absent; storage returns `runtime_unavailable`; pipe request returns `not_implemented` |

The repository example returned times [0,.5,1] h, inventories [50,49.5,49] m3,
selected terminal-volume Jacobian [[-1]] h and cost gradient [0]. It echoed the
supplied identity and returned no feasibility verdict. All replies preserved request
IDs, used newline framing, stayed within the 1 MiB limit, and emitted no stderr.
Artifact support remained false.

Local evidence is ignored `out/a004/worker-status.json`, including the requests,
responses, reviewed source commit and runtime-source hashes. Its SHA-256 is
`2011edb33291eba2603c7dc083d69f0424ce152724101880dfd4e8c8466dd9f5`.
This was a targeted status/documentation check, not a new full schema/parity or
installed-desktop acceptance run. Existing worker commands and contract examples
in [WORKER.md](WORKER.md) provide the normal reproduction entry point.

All updated local Markdown links, code fences, whitespace and changed paths were
checked. No tests or runtime code were added or changed for this documentation-only
correction. The preceding 94-test suite and 12-gate PhysicsNeMo smoke remain
A-003 evidence; they were not rerun here. Person B's files, the root solution,
contracts and shared documentation remain untouched.

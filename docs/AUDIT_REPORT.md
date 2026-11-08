# Integrated audit status

Person B's frozen source/boundary matrix and original evidence remain in
[AUDIT_PERSON_B.md](AUDIT_PERSON_B.md). Person A's independently frozen numerical,
scientific and proof audit is now available in
[AUDIT_PERSON_A.md](../science/AUDIT_PERSON_A.md), with its original counterexamples
and linked resolutions. The reports have different scopes and baseline commits.
The earlier B review did not identify A-001; its bounded no-P0/P1 conclusion must
not be read as a numerical-engine clearance.

PR #17 at `367dcf1` supplies the A-owned fixes. It is merged with B5a/B5b ancestry
preserved; B adds the required desktop grid/deployment compatibility changes.

| Finding | Original severity | Current implementation and acceptance |
|---|---|---|
| A-001: shortened forecast horizon | P1 | Native, Torch and worker preserve rounded interior multiples and append the exact requested horizon. B independently reconstructs this bounded grid and packages storage_grid.py. A one-ULP-short result cannot validate or stage. |
| A-002: nonfinite independent costs/catalog prices | P2 | Native validation rejects nonfinite independent cost; catalogs reject unrepresentable combined unit prices. Worker and desktop error paths retain explicit rejection. |
| A-003: constant/linear residual derivatives | P2 | Coordinate-zero derivatives preserve parameter gradients; analytical residual checks execute locally. Genuine PhysicsNeMo tests remain upstream evidence on this CPU-only environment. |
| A-004: worker documentation drift | P3 | A's documents distinguish inherited desktop snapshots, current repo capabilities and actual scientific execution. |
| B-01/B-04/B-05: process retirement, captured config, source fallback | P2 | B5 regression checks cover serialized retirement, immutable original configuration/reference arrays and source-independent installation. The A-001 integration supersedes the former short-endpoint compatibility policy. |
| B-06: cross-source endpoint/key mixing in new native key setup | P1 | A project endpoint-only .env could redirect a process/saved-user key. Endpoint configuration now follows the selected credential source unless explicitly overridden by the process; synthetic request/header regressions cover the boundary. |

Native solver provenance advances to `constant-flow-0.1.1`; Torch reports
`torch-constant-flow-0.1.2` (`+worker-v1` on the wire). Wire/method/project versions remain 1. Old stored runs
are historical evidence, not newly validated results. No prior approval is restored.

Fresh combined integration checks and concrete remaining limitations are recorded
in [STATUS.md](STATUS.md). A's reported Lean, performance and actual PhysicsNeMo
executions remain attributed to A; this integration does not claim those runs were
repeated on B's laptop. The original local B5 evidence remains intact.

The numerical fixes close the identified implementation defects; full release
qualification remains open. Manual dialog/cancellation closure, scientific result
retention and clean hosted-model rehearsals remain separate acceptance gates.

Later main `6966707` also supplies native key settings and A9's bounded transport
qualification. B resolves the Science/credentials QML bindings together and fixes B-06
before accepting the new configuration path. A9 changes no native transport algorithm;
its refined mean-L1 qualification does not establish local peak/threshold accuracy,
arbitrary-regime quality or volume approval. STATUS distinguishes fresh local checks
from the upstream report and retains the original coarse-study failure.

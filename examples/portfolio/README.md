# Offline portfolio examples

These are self-authored synthetic inputs, not EOG field measurements or challenge
answer keys. They can be redistributed with the project once the project owners
approve the release license. No API key, Torch or PhysicsNeMo is needed to import
them, search responses, validate an exact plan or reopen its exported evidence.

- `sandbox/healthy.json`: the editable default, with a conservation check.
- `sandbox/pump-failure.json`: a failed primary transfer and overflowing baseline.
- `sandbox/bad-reroute.json`: a locally plausible reroute that violates constraints.
- `sandbox/mixed-response.json`: a feasible combination of bounded controls.
- `operations/`: two systems and three facilities, with one explicit pump fault.
- `renamed-topology/`: a different one-system/two-facility network, with Unicode
  asset IDs. It exercises identity and topology independence.

Operations imports the three fixed files `network.json`, `ops.json` and
`telemetry.csv`. It preserves original units and finite out-of-range values.
The default cutoff is 2026-11-16 12:00 UTC. Pump status 2 is a fault, distinct
from the normal stopped state 0. The scene is schematic; its road, pad and pipe
positions do not determine travel times, rates or pressure.

`manifest.json` records source-file SHA-256 hashes and generation parameters.
`sandbox/native-results.json` is produced by the actual native engine. Its two
stock closed-form conservation checks are independent of the simulator's output.
Regeneration is explicit and refuses existing output files:

```powershell
python scripts/Create-PortfolioExamples.py NEW-OUTPUT-FOLDER
out/portfolio-desktop/desktop/breakwater_export_examples.exe ABSOLUTE-NEW-FOLDER
```

Any `recordings/` archive is a genuinely retained hosted investigation on these
exact inputs, labeled historical. Opening it makes no hosted request and never
restores approval. Live model failures remain in the separate qualification
report; a recording is an example, not a guarantee of model success or optimality.

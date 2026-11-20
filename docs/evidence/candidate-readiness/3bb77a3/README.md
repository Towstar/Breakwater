# Retained candidate evidence

Engineering source: `3bb77a353fb04267260764060bc5f58d61d5e7f5`.
See [the results and limits](../../../CANDIDATE_RESULTS_3BB77A3.md).
The [copy inventory](copy-inventory.json) maps original local paths to these
byte-identical copies. The [supplemental inventory](supplemental-copy-inventory.json)
adds the completed round's final independent audits. Some reports retain original
developer paths; the inventories provide their portable repository locations.

Both hosted rounds are preserved, including all seven declared slots and the
first round's failed pump attempt. No successful case was copied between rounds.
The original package wrapper and dependency-inventory failures remain beside
later successful checks. CI artifacts remain on the linked exact run; this folder
contains their digest/source/toolchain audit and selected raw reports, rather than
republishing large CI ZIPs.

From the repository root, use an existing Python 3.12 interpreter:

```powershell
python -B scripts/audit_candidate_evidence.py docs/evidence/candidate-readiness/3bb77a3/hosted/round2/manifest-0006.json
python -B scripts/audit_candidate_evidence.py docs/evidence/candidate-readiness/3bb77a3/hosted/round1/manifest-0006.json
```

Round 2 returns exit 0, seven passed. Round 1 returns exit 2, a valid retained
incomplete round with six passed and one failed. These commands audit retained
integrity and grounding offline; they make no model request or numerical recomputation.

Private Windows/source ZIPs, databases, symbols, credentials, environments and
the user's saved project are excluded. The private review kit's nineteen manual
checks remain `not_run`. Human review, GPLv3 agreement, distribution review and
actual clean-Windows acceptance remain separate pending gates.

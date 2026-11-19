# Typed retained evidence and database version 2

New archive version 2 keeps the existing native session fields and adds exactly
one bounded `evidence_envelopes` item. An item has `envelope_version: 1`, a known
`kind`, and a typed `payload`. It cannot authorize controls. Supported envelopes
cover observed origins, detector witnesses/acknowledgements, native pipe results
and a selected-candidate scientific origin. Native pipe retention and manual
selected-candidate Science are connected to the desktop. Observed forecast/review
integration remains a separate increment.

G1 snapshots/events, project-v1/v2, the scientific worker protocol, EOG archives,
native pipe-result-v1 and scientific-result-v1 remain unchanged. Legacy session-v1
documents still load and export in their original version. A v1 document cannot
carry a v2 envelope. Unknown versions/kinds/fields, malformed UTF-8, duplicate JSON
keys, nonfinite values and inconsistent identities are rejected. Historical reads
and imports explicitly remove operating authority.

## Database migration

Every connection, migration, backup, serialization and transaction stays on the
existing history worker. A known v1 schema receives a consistent SQLite
`VACUUM INTO` sibling backup before migration. The backup includes committed WAL
data and is retained. Two metadata columns and `user_version = 2` are installed
within one transaction. A failed transaction rolls back; an unknown schema is
rejected before alteration. Insufficient backup space or backup failure leaves
the old schema intact and reports the error. Native calculations can continue.

Old applications that only understand database version 1 reject version 2 rather
than silently rewriting its records. Keep the sibling v1 backup for an explicit
rollback to an old application; do not replace a newer database while it is open.
An abrupt power-loss/clean-VM exercise remains a separate release acceptance gate.

## Incident evidence

Detection envelopes retain strict G1 events, immutable samples and commands,
registered calibration/configuration, mapping diagnostics/effective state, the
declared detector method and bounded pre-assimilation numerical windows. Import
validation reconstructs volume residuals and absolute thresholds from the window.
This checks consistency, not authenticated execution or mechanical diagnosis.

Onset, clear and acknowledgement are separate records. Ongoing active updates
are coalesced to at most one retention request per ten event seconds. Completed
evidence remains historical when a source or numerical context retires; retirement
does not discard a queued history record. The Local monitoring panel reports
pending/saved/storage failure. No database/model task runs per telemetry packet.

## Native pipe history

Completed noncancelled native results are retained automatically with their exact
source run, source snapshot/network/controls, method, parameters, preview sampling
and diagnostics. Changing selection after computation begins cannot substitute
another source. Results that finish after an edit remain historical. A cancelled
calculation cannot publish new automatic history. Imported external pipe files do
not become new execution records.

Automatic pipe retention stops visibly at 256 records or 256 MiB, including
versioned archive bytes. It never removes earlier records. Manual result export
remains available after this limit or a storage failure. History can reopen a
retained result in the existing Pipe transport viewer without a current volume
forecast and without restoring approval.

The original pipe result still reports `native_scalar_reference` and
`fvm-upwind-euler-0.1.0`. Durable retention does not improve its documented transport
accuracy or establish network chemistry, pressure hydraulics or treatment quality.

## Manual Science on a stored candidate

In Science, choose **Selected stored candidate** after selecting a run in Compare
or History. The source is reconstructed on a background task from that run's
original snapshot, complete overrides, numerical method and time grid. Its full
trajectory and modeled cost must agree with an independent native reproduction
before the scientific worker can run. The original stored samples are retained
for comparison. This selection never applies the candidate's controls.

Both current and historical candidates support a new CPU PyTorch forward
comparison and eligible continuous-control sensitivities. The existing
scientific-result-v1 document is unchanged. A manual-science-origin envelope
retains the source session/snapshot/run, design and effective hashes, grid and
source mode. Revision zero identifies this immutable derived source; it is not
the editor's current numerical revision. No model tool-call ID is invented.

Selecting another source or run retires pending work. Reconstruction is limited
to one background task with the latest pending selection coalesced. Cancelled
or superseded tasks cannot publish an earlier source. Historical evidence still
cannot restore operating approval, and a scientific comparison is not a plan
validation or feasibility verdict.

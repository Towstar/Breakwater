# Private EOG causal observation core

`eog_observations.py`, method `eog-observations-0.1.0`, indexes already parsed
observations without file access, Qt, Torch, model APIs or a public wire mapping.
It is the next small Person A increment after E1. It does not supply a production
capture reader, event detector, planner, independent plan validator or simulation UI.
The exact-contract review is separate; this module consumes none of its DTOs.

## Supported behavior

An explicit catalog contains opaque full asset IDs, metric names, units and optional
discrete status domains. The caller owns the input-to-catalog mapping. The index
rejects unknown or duplicate channels, unordered rows, duplicate timestamp/channel
identities, nonfinite numbers, coerced string/bool values, malformed identities and
work-limit violations. Acquisition times are exact nonnegative integer Unix seconds.
Imported Unicode and punctuation are retained, with a 256 UTF-16-unit identity bound.

Every finite binary64 measurement remains unchanged, including negative values,
negative zero and values outside nominal ranges. A finite unknown status is retained
but its interpretation is `unsupported_status`. This does not validate a physical
initial state or convert a measurement into a commanded setting.

The actual first/last supplied row timestamps determine the archive's window. Views
select a cutoff within that window. `latest`, `series`, `cohort`, `window` and private
evidence hashing use only acquisitions at/before cutoff. A view's cutoff can fall
between recorded frames. Missing channels never produce zero/healthy defaults.
The archive, samples, catalog and returned records are immutable.

## Explicit policy

Default policy: maximum reading age **300 s**, maximum supported gap **300 s**,
maximum cross-channel acquisition skew **0 s**. Freshness and gap equality are
allowed. One second beyond the limit is unavailable; there is no tolerance epsilon.
The 300-second defaults follow the official EOG cadence. Exact cross-channel
alignment is our conservative choice, not a claimed organizer mandate. A caller may
choose and record a different bounded policy. G1's one-second defaults are not used.

- A reading is available, missing, stale or unsupported, with its raw sample and age.
- A cohort retains each reading/reason, becoming unavailable if any component is
  unavailable, or incoherent if acquisition spread exceeds the chosen skew.
- A window requires support through the entire requested interval. It retains one
  predecessor at/before the start so a boundary gap cannot disappear. Missing,
  insufficient history, stale, unsupported and gap states remain distinct.
- Query limits raise an explicit error. There is no silent decimation or removal
  of extrema/gaps. Chart downsampling is a separate B-owned presentation concern.

Budgets: at most 2,000,000 samples, 8,192 catalog channels, and 8,192 returned samples
per series/window query. Construction stops after the first row beyond its budget.
Internal indexes store immutable rows/timestamps; a view does not duplicate the
archive. Fingerprinting streams the causal prefix without making another full copy.
Subprocess cancellation/lifetime enforcement belongs to the future adapter.

## Evidence and use

```python
from eog_observations import Archive, Channel, Sample

channel = Channel("arbitrary-pump", "flow_bpd", "bpd")
archive = Archive([channel], [Sample(0, *channel.key, 0),
                              Sample(300, *channel.key, 0),
                              Sample(600, *channel.key, 1000)])
view = archive.at(450)
assert view.latest(*channel.key).sample.unix == 300
assert all(s.unix <= 450 for s in view.series(*channel.key))
```

Run with the documented scientific Python 3.12 environment:

```powershell
python -m unittest discover -s science/tests -p test_eog_observations.py -v
```

Generated adversaries cover fresh/stale boundaries, missing/unsupported states,
cross-channel skew, internal/boundary/tail gaps, future-only channels, immutable
evidence, Unicode, exact IDs, bounds and budget exhaustion. Seeded queries compare
the index with a separate straightforward prefix filter. No answer keys or official
telemetry are loaded by these tests. Actual logs/source identities are recorded in
`science/evidence/eog-observations/` when the increment is published.

## Identity and remaining work

The private fingerprint hashes the causal values, cutoff, declared channel meanings
and observation policy; future values cannot change it. This is deliberately
different from the public full-source dataset/capture identities. It is not a file
integrity check, authenticated execution record or replacement for the accepted
capture verifier.

Archive construction validates every supplied row. A production historical analysis
must receive a verified truncated capture, rather than using this pure index to
authenticate or lazily parse the original full recording. Network/ops parsing,
allowed metric/unit mapping, atomic capture creation and corruption checks remain
separate acceptance items.

Next: adopt the reviewed exact contract; implement causal event/lifecycle methods
using these views, versioned thresholds and generated unfamiliar cases; then bind
the real capture/provider. A current coherent cohort does not establish healthy
operation. A continuous window alone does not establish a fault, calibrated
confidence, hydraulic fidelity or shutdown countdown.

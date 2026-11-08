# Explicit production-feed allocations

One production source may feed multiple distinct tanks. The prototype prescribes
the split; it does not infer pressure, valve resistance or pump hydraulics.

Each source route has an optional integer `source_share_bps` in `[0,10000]`, in
hundredths of one percent. Omission means 10000 (100%). Across the outgoing routes
of each source, the integer shares must total **exactly 10000**. There must be at
least one route. Duplicate source-to-tank pairs are invalid. Zero-share routes are
allowed, and represent inactive branches. Tank-origin routes retain the default
10000; an allocation on those routes is invalid.

Effective feed flow in m3/h is `(inflow_m3h - curtailment_m3h) * source_share_bps /
10000`. Shares are fixed model configuration; they are not new search controls.
Curtailment continues to act on total source production and affects every branch
proportionally. Branch capacities still apply to the resulting flows. Source
feeds retain the existing no-pump/available rule. Values and inventories are never
clipped; there is no implicit normalization of invalid totals.

For Production B at 16 m3/h, 5000/5000 delivers 8/8 m3/h. With curtailment 2 m3/h,
the same split delivers 7/7 m3/h. A 7500/2500 split delivers 12/4 before curtailment.
Source curtailment cost is counted once; route costs use each branch's actual flow.

## Project compatibility

The reader supports project versions 1 and 2. Existing single-feed projects with
default shares keep their original version-1 serialization and physical hashes.
Projects containing multiple source branches or non-default shares are written
as version 2. A version-1 document may not contain `source_share_bps`. Version 2
is required even for an invalid allocation draft, so older applications reject it
instead of ignoring a share and simulating the full source flow on each branch.
Stable IDs, layout, pipe parameters and undo/redo are preserved when shares change.

## Scientific worker compatibility

The outer NDJSON protocol remains version 1. `storage_rollout` method version 2
supports the optional allocation field and shared validation above. Method version
1 remains supported for legacy networks and rejects the new field. Responses echo
the requested method version, identity, tank/control order and original sample
indices. Capabilities advertise the highest supported storage method version (2),
with backward support for version 1. Other method versions are unchanged.

New desktop clients can still compare legacy networks with a version-1 worker;
split networks require version 2 and receive an explicit unsupported-worker state
otherwise. Older desktop clients reject the newer unsupported storage method version
instead of treating a split result as a checked legacy calculation. Existing
history keeps its recorded provenance and remains non-actionable.

## Acceptance boundary

Check independent 8/8 and 12/4 trajectories, conservation, branch capacities,
curtailment cost/derivatives and optimized search against native reference paths.
Check malformed/incorrect totals, typed integer limits, stale edits, atomic
application/cancellation, save/reopen, legacy compatibility and real worker parity.
Run native/desktop suites normally and under MSVC ASan and the full pinned Python
suite. Existing numerical tolerances, dependency pins and proofs are unchanged.
The existing proof library is not a new audit of the allocation-to-model mapping.

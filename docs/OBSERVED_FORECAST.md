# Frozen observations, native forecasts and advisory review

This workflow is local and synthetic. It shares the native numerical tools with
the Sandbox investigator, but has a separate source, controller, conversation and
History selection. The observed controller has no control-application method.
EOG recorded analysis and editable Sandbox simulation retain their own workflows.

## Try the complete local path

1. Open **Project > Local monitoring example**, then **Local monitoring**.
2. Choose a preset and seed, and start synthetic input. The bundled publisher
   requires the locally configured Python interpreter. It is a separate seeded
   plant, not a forecast used as its own measurement source.
3. Pause input, then **Capture state**. Pausing is explicit. The synthetic event
   clock freezes while heartbeats continue; it does not add freshness to a replay
   or keep a dead source eligible. Raw measurements, frozen commands, calibration,
   coverage and mapping bounds remain distinct.
4. Open **Forecast / advisory review**. **Local search** runs actual native finite
   search without a model or API key. **Investigate** explicitly starts hosted
   Nemotron using the same immutable source and bounded numerical tools.
5. Compare baseline/candidate trajectories. **Review staged plan** shows the run,
   independent validation reference, horizon, nominal headroom, design/effective
   hashes, capture/expiry times, frozen command changes and measured state/bounds.
6. **Acknowledge advisory review** records human review only. It applies no
   Sandbox controls, sends no publisher command and clears no incident. Historical
   sessions remain viewable through this dialog's History tab.

Capture an available coherent state before numerical work. An unavailable state
can instead be investigated to explain missing/conflicting coverage. All numerical
and scientific tools reject it with `coverage_unavailable`; a local investigation
retains a no-plan record with no numerical runs. Expired, replayed or retired
sources cannot begin a new current-source investigation or regain review authority.

## Numerical and identity boundaries

The registered design/network hash remains unchanged. The immutable observed
snapshot retains original samples and commands; the separately mapped effective
network initializes native forecasts. A gateway independently reconstructs mapping
diagnostics from the registered raw evidence and rejects substituted bounds or
state. A failed pump remains unavailable at zero delivery; candidate controls
cannot invent its repair. Finite negative/overrange readings stay in raw evidence
and remain unavailable where unsupported by this model.

The detector events supplied to an investigation are frozen on the acquisition
worker at capture time. Future events and different stream/epoch/configuration
identities are rejected. Later detector updates and acknowledgements cannot
rewrite an earlier investigation's event set. Symptom evidence does not establish
a mechanical root cause.

Forecasts use the existing nominal constant-delivery model. Declared observation
uncertainty is retained and displayed, but no robust-feasibility guarantee is
claimed. Native independent validation checks the actual selected numerical run.
Scientific sensitivities remain local derivatives, not plan validators.

## Model tools and authority

Observed sessions add `get_observations` and `get_detection_events` to the existing
native tool catalog. Nemotron chooses tool order. It must inspect observations,
the network and controls before concluding. Numerical calls execute real native
methods; final recommendations must reference independently validated feasible
stored runs. Missing coverage never falls back to a design-only numerical plan.
The model has no hidden plant truth, file access, publisher actuation or editor
mutation tool.

The existing `request_approval` tool stages **advisory** review in this workflow.
Every observed result/archive has `actionable: false`. A first acknowledgement
requires the exact current capture, available coverage, current design/revision,
matching selected/recommended/staged run and validation reference. Expiry preserves
completed calculations as read-only evidence. Replacement, restart and numerical
edits retire the old session. Layout-only edits preserve its numerical identity.

Source validation is nonblocking and limited to one background preparation task.
Cancellation/generation guards prevent superseded work from starting a session.
Starting and cancelling a replacement cannot revive an earlier acknowledgement.

## Retention and resource bounds

Version-2 `observed_origin` envelopes retain the exact raw snapshot, registration,
mapping diagnostics, effective network and frozen events alongside existing full
native trajectories, validation, tool events and review records. G1, project-v1/v2,
scientific-result-v1 and EOG archive formats are unchanged. Restored/imported
history cannot restore current-source eligibility or operating authority.

The session recorder now waits for an exact checkpoint completion before queuing
another intermediate checkpoint. A single latest update coalesces while storage
is busy. Its final checkpoint still contains all original events and run evidence;
explicit flush retains the latest source before shutdown/session replacement.
Storage failures remain visible and do not disable native calculations.

Native tool/refinement/deadline budgets remain 12 calls, 3 refinements and 120
seconds, plus the already bounded format-only final correction. Envelopes remain
limited to 8 MiB, sessions to 128 runs, and acquisition keeps its existing G1
frame/reorder/trace limits. No automatic model request runs per measurement.

Final hosted qualification, actual clean-Windows acceptance, owner review and
licensing consent remain release gates; offline or fake-model tests cannot replace
them. Consult the versioned evidence report for actual executed checks and failures.

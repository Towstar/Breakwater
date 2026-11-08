# EOG all-system search tool correction

The user observed `system_scope_unavailable` when Nemotron requested search over
one system. The numerical provider deliberately requires the complete captured
network; manual Search plans already requested it correctly. The model-facing
schema incorrectly advertised a general system filter.

Runtime source: 4654de484b79821a01ca2e7d0ee36d506c066abc.

The search model tool now declares one scope: all captured systems. Its parameters
contain candidate_limit and time_limit_s, with no system_ids field. The adapter
explicitly binds the provider's required system_ids=[] only when that field is
omitted. Explicit null, malformed, duplicate, unknown and narrower scopes are
rejected before provider execution; they are not coerced to an empty list. Valid
explicit [] and complete system lists retain compatibility and original values.
Filtered topology/observation/detection tools remain available. Prompt/context
state the capability and require reporting unavailable if a strictly narrower
search is required. Returned results still disclose complete search scope.

Actual checks: controller50/50 and investigator66/66, zero failures/skips.
These use a fake hosted transport with actual controller/provider framing and
history persistence; they are not a hosted-model performance claim. New cases
include omission-to-bound-request history, a real two-system subset rejection
followed by a corrected call, unchanged original model-call scope in history,
and explicit null versus omission. Provider contract and numerical source are
unchanged. A separate real hosted verification is recorded only after completion.

Launch the fixed installed bundle using its Start-Breakwater.cmd. Start a fresh
investigation; an already running executable retains its older tool definitions.
No old application process is terminated or overwritten by this installation.
No controls are dispatched and no Devpost content is edited.

Installed verification: two consecutive new-build startup/render runs passed,
and the previous325a168 control build passed. Their real images/reports were
inspected. An initial new-build attempt crashed0xC0000005; its cause was not
isolated, and the failed and successful attempts are retained separately.

The genuine hosted attempt completed real detect and evaluate_plan operations,
then its third model request failed transport_failed. It never requested search;
therefore it does not establish a hosted end-to-end pass for this scope fix.
The full local archive remains in ignored out/eog-scope-verification; only the
sanitized summary is retained here. No operating controls were applied.

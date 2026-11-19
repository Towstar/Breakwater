# Bounded JSON final mode

The second hosted round on `62052b9` passed three of four rehearsals. Its first
Operations run completed detection, then returned an empty final. The one allowed
format-only correction returned the wrong field set. Both were rejected. The
archived incomplete trace remains evidence; it is not a grounded recommendation.
The earlier `92119fb` round passed four of four. Keep both rounds in reports.

JSON final/correction requests now also send
`chat_template_kwargs: {"enable_thinking": false}`. NVIDIA recommends disabling
thinking for concise/structured output in its
[Nemotron 3.5 Lightning guide](https://docs.nvidia.com/nim/large-language-models/2.0.10/get-started/advanced/get-started-nemotron-3.5-lightning.html).
These turns format retained evidence and do not perform new investigation.

Tool-calling requests keep their existing sampling/reasoning settings and omit
`response_format`. Final mode still has no tools, temperature zero, JSON-object
format and the same 4,096-token cap. Transport retries, request limits, one
format-only correction, tool/refinement budgets, deadlines and exact independent
validation remain unchanged. JSON-object mode does not guarantee the required
field set or grounded references; application checks remain authoritative. This
change cannot turn a premature final with no plan evidence into a valid plan.

The existing transport regression checks the complete ordinary request before and
after a final request and the separate final payload. The full transport,
native-orchestrator and EOG-investigator suites pass in normal and ASan builds.
New hosted results must be retained and reported separately. This is not a
comparative-model benchmark or a claim of perfect completion reliability.

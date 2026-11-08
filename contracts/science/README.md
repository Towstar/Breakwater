# Science boundary v1

These are additive method contracts. A8's repo worker implements `health`,
`capabilities` and CPU float64 `storage_rollout`; Python `pipe_transport` remains
explicitly `not_implemented`. The deployed desktop adapter executes real storage
comparison and selected-control sensitivities, including stored-run analysis for
Nemotron, and saves completed results in immutable History. See
[worker implementation and evidence](../../science/WORKER.md).

- [Pipe parameters](pipe-parameters-v1.schema.json) describe the strict serialized
  selected-pipe settings consumed by the native desktop helper.
- [Requests](request-v1.schema.json) and [responses](response-v1.schema.json) describe
  protocol-v1 method extensions, including the reserved native-FVM pipe envelope.
  They preserve the existing health exchange.
- [Examples](examples/) are **illustrative shapes**, with placeholder identities/hashes
  and example-only method versions. They are not executed numerical or ML evidence.
- [Integration rules](../../docs/SCIENCE_CONTRACTS.md) define the mandatory semantic
  checks that JSON Schema alone cannot express, provenance, bounds and authority.

Load referenced schemas from this directory when validating offline. Schema identifiers
do not grant permission to fetch remote references or read arbitrary local files.

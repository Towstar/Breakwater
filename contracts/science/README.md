# Science boundary v1

These are additive **proposed method contracts**, not a claim that the Python worker
executes them. The current worker implements `health` only; a `capabilities`,
`storage_rollout` or `pipe_transport` request still receives `not_implemented`.
Person A must review/adopt the units and array semantics before worker implementation.

- [Pipe parameters](pipe-parameters-v1.schema.json) describe the strict serialized
  selected-pipe settings consumed by the native desktop helper.
- [Requests](request-v1.schema.json) and [responses](response-v1.schema.json) describe
  reserved protocol-v1 method extensions. They preserve the existing health exchange.
- [Examples](examples/) are **illustrative shapes**, with placeholder identities/hashes
  and example-only method versions. They are not executed numerical or ML evidence.
- [Integration rules](../../docs/SCIENCE_CONTRACTS.md) define the mandatory semantic
  checks that JSON Schema alone cannot express, provenance, bounds and authority.

Load referenced schemas from this directory when validating offline. Schema identifiers
do not grant permission to fetch remote references or read arbitrary local files.

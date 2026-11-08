# EOG search provenance boundary

Person B integration of A's `a420deb` keeps the adopted `72cadd1` public wire and
existing history format unchanged. Search responses and their complete retained
evaluations, family declaration, limitations and capture/request bindings already
fit that format. Old archives remain readable and historical, without actionable
review authority.

The version-1 response has no explicit worker implementation identity or health
operation. `method_version: 1` names the operation contract. The client's sanitized
`method` metadata names the requested operation, such as `search_plans`; it does
not assert a numerical or worker implementation. The worker's internal method
constant participates in an opaque analysis hash, which cannot be reversed into
an attestation. No version is inferred from a filename, environment variable or
an injected fixture. Installation/source-hash evidence records which packaged
worker was exercised separately.

The complete response retains the provider's free-form search family declaration
and explicit evaluator/validator fields where supplied. A bounded tool-result
summary also copies actual search scope, optimality, completeness and candidate
counts. This is a projection of received claims, not a new implementation identity
or independent verification. It does not make `best_found` global optimality and
does not turn an unavailable selection into advice. Current review still requires
the separate complete validation response for the exact selected plan.

Direct client tests use a conspicuous protocol-only process and require absent
implementation labels. Investigator tests cover bounded, complete-family,
unavailable and malformed summary fields, preservation of the original tool
response and no inferred final selection. These additions require the integrated
normal/ASan gates; source changes alone do not establish executed search or live
model behavior.

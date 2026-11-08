# EOG hosted Nemotron rehearsals

Four actual hosted investigations reached a grounded, independently validated advisory recommendation: canonical-06, canonical-13, canonical-15 and renamed-02. These are **zero-cost recommendations with no truck jobs or manual well shut-ins**, not demonstrations that an intervention was necessary. Each selected forecast avoids shutdown over its checked repair horizon. Inspection asset references remain in the recorded plans.

The model was `nvidia/nemotron-3.5-lightning-30b-a3b`. It issued the tool calls; the production desktop controller dispatched the numerical worker and supplied matching tool continuations. The evaluator was `eog-native-seconds-0.1.1`; the independent validator was `eog-rational-validation-0.1.0`. The retained records contain 23 model-request metadata events, 19 model-issued calls in their actual order, 16 numerical exchanges and 65 agent events.

| Rehearsal | Model requests | Tool calls | Numerical exchanges | Model-selected tool order |
| --- | ---: | ---: | ---: | --- |
| canonical-06 | 5 | 4 | 3 | detect → read_topology → evaluate_plan → validate_plan |
| canonical-13 | 4 | 3 | 3 | detect → evaluate_plan → validate_plan |
| canonical-15 | 7 | 6 | 4 | detect → read_topology → read_topology → evaluate_plan → validate_plan → validate_plan |
| renamed-02 | 7 | 6 | 6 | detect → evaluate_plan → validate_plan → search_plans → validate_plan → validate_plan |

The renamed input has a different dataset identity, renamed references including Unicode identifiers, and a **structurally changed network**: the entire `S1_FACILITY_C` branch was removed, including its three wells, tank, pump, pipeline, corresponding operations and 21,151 readings. The official source triple is `history/mock_test_2026-10-10T1530`; all three source hashes and all three derived hashes were checked against the derivation provenance and the successful canonical/renamed archive manifests.

| Full source/derived dataset | Systems | Facilities | Wells | Assets | Telemetry series | Readings |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Official source | 5 | 15 | 45 | 110 | 195 | 317,265 |
| Renamed, branch removed | 5 | 14 | 42 | 103 | 182 | 296,114 |

These counts were recomputed from the actual network and CSV files. The derivation provenance SHA256 is `fae744dc85cbb380b11ae36219a4a1f372880dcb25b7e1268708b33b6ee79aa1`. This demonstrates tool use on a structurally smaller derived network with renamed identifiers; it is **not an independent held-out accuracy or physical qualification benchmark**. All four selected plans use a checked 18,300-second remaining repair horizon at cutoff 1791342300. Their plan IDs, semantic hashes, full trajectories, costs, source-evidence references and exact independent-validation bindings remain in the individual JSON files. Renamed-02 has six numerical exchanges: five completed and one explicitly unavailable search response.

`search_plans` was unavailable in this worker-0.2.1 rehearsal freeze. Renamed-02 actually requested it and retained the explicit unavailable response before completing a separately evaluated and independently validated recommendation. Ethan's later search work is a separate integration increment. These traces do not claim a working global optimizer, universal model reliability, superiority over another LLM, or independent hydraulic/real-world qualification. The model's final narrative is retained as a claim; numerical records and application reference checks determine eligibility.

No control application or dispatch occurred. There are zero operator review records in these four archives. The live runner recorded review eligibility; exported sessions are historical, `actionable: false`, and cannot restore approval.

## Files and provenance

The four `*.sanitized.json` files are **non-importable publication projections**. They retain the capture manifest and hashes, numerical requests/results, model-issued arguments/order, matching result references, final claims and sanitized request metadata. Captured file payloads, base64 input blobs, runtime capture paths, credentials, authorization/header data and private reasoning are excluded. Model metadata is limited to model identity, HTTP status, duration and retry count; numerical process metadata retains duration and retry count.

[manifest.json](manifest.json) records exact hashes, sizes, selected plan/validation IDs and offline verification counts. [attempts.json](attempts.json) preserves every retained runner summary plus original archive/log hashes. The preparation checkout in the manifest identifies this projection step; the underlying rehearsals span development changes and configuration corrections and must not all be attributed to that one checkout.

The untouched, full, importable originals remain local under `out/eog-rehearsals/`. Their SHA256 provenance is:

| Original archive | SHA256 |
| --- | --- |
| canonical-06.eog-session.json | `14f08c2491fb1715d2b2470386db136d18f1d5fbd8fb4c9a3528db098982a8b9` |
| canonical-13.eog-session.json | `c8c97bf7f3eeb3074efee67bae1b5ae1dfa302a73f7c9581d2c845c189fbc8d5` |
| canonical-15.eog-session.json | `669b48f75918aa8203b414bfcccc6bc4bcd6868c548bae22b43b62e711ae670b` |
| renamed-02.eog-session.json | `99035c465cdf77d347f6ff2861a729debc32cd1e544325b4c701f665160b6e98` |

A local-only `out/eog-live-successful-archives.zip` contains exactly these four unchanged full archives and a README. It is not part of the committed publication projection.

## Incomplete attempts are retained

The ledger contains 15 retained attempts: four passes, eight hosted incomplete investigations and three setup-only incomplete attempts. This is an accounting of development runs, **not a statistical success-rate benchmark**.

| Attempt | Recorded outcome | Model requests | Completed numerical exchanges |
| --- | --- | ---: | ---: |
| canonical-01 | Incomplete: provider operation unavailable; search and validation were unavailable | 11 | 4 |
| canonical-02 | Incomplete: no grounded review-eligible final | 12 | 1 |
| canonical-03 | Incomplete: no grounded review-eligible final | 13 | 0 |
| canonical-04 | Incomplete: no grounded review-eligible final | 7 | 1 |
| canonical-05 | Incomplete: no grounded review-eligible final | 7 | 5 |
| canonical-07 | Incomplete: no grounded review-eligible final | 5 | 3 |
| canonical-08 | Incomplete: provider operation unavailable; search unavailable | 9 | 6 |
| canonical-12 | Incomplete: provider operation unavailable; search unavailable | 7 | 5 |
| canonical-09 | Setup-only incomplete; zero model requests and zero model-issued calls | 0 | 0 |
| canonical-10 | Setup-only incomplete; zero model requests and zero model-issued calls | 0 | 0 |
| renamed-01 | Setup-only incomplete; zero model requests and zero model-issued calls | 0 | 0 |

The generic runner code for the three setup cases is `investigation_incomplete`; their zero-request records distinguish them from hosted model failures. No unrecorded failure cause is inferred. The unsuccessful investigations are not silently removed when presenting the four successful examples.

## Offline publication checks

All 16 retained numerical exchanges passed the existing local EOG JSON-schema and semantic request/response binding checks. Archived requests deliberately omit ephemeral capture directories; validation supplied a fixed in-memory path placeholder without opening capture files. Each selected plan matched its final plan ID, semantic hash, available/feasible independent validation, exact repair horizons and zero cost. Model-call order and call/result IDs matched all four runner summaries.

The projections were recursively checked for excluded private fields, credential-like strings and runtime absolute paths, including JSON encoded inside diagnostic summaries. These are publication/content checks on existing evidence, not new hosted calls, numerical executions or operating authorization.

# Frozen presentation review

Reviewed at 2026-11-21T00:11:59.5699743Z. Baseline 7df8c379e1178600bcb869cd62ab3d6751ea8a38; head 3d91452d81af7052effb35f98afe48c6f3661b20. Read-only source review; no tests executed by this review.

No consequential correctness finding in the inspected presentation/controller changes.

- EOG cost formatting is reached only for an available evaluation and matching independently feasible validation. EogAnalysisClient validates schemas before publishing; EogContracts::validate_document runs shape validation, finite-value checks and semantic cost consistency. The common-v1 cost schema requires numeric truck_cost, lost_revenue and total. Null/unavailable evaluations cannot reach the dollar-formatting branch.
- AgentOrchestrator emits application-owned explanations for selected, unselected and coverage-unavailable outcomes. Raw hosted prose remains separately retained. Missing coverage does not become a claim that the native engine is absent. Observed review remains acknowledgement only; sandbox application still requires separate operator review.
- AgentPanel uses the retained_evidence source marker for the primary explanation. Legacy narrative receives a generic evidence-review headline and a separately labeled unverified disclosure. Headers/body use plain text; the narrative is read-only and selectable.
- EOG regenerates headlines from retained evidence on both live completion and historical reopening. It keeps the original five-key final event unchanged, so the strict v1 archive validator requires no extension. Exact plan/validation references, hashes, available coverage, independent feasibility and cheaper-compatible checks remain in force. No history eligibility is restored.
- EOG headline elision is limited to presentation: an always-visible advisory/optimality caption remains, and the disclosure includes the full evidence summary and original model narrative. Checkboxes remain ordinary keyboard-focusable controls. Actual keyboard, display-scaling and rendered layouts were not exercised by this review.

## Inspected validation evidence

focused-normal.log records five passing CTest entries, zero failures, 58.91 s. focused-normal-v3.log records later reruns of agent_orchestrator and observed_forecast, two passing entries, zero failures, 21.13 s. These are focused checks, not a new full 88-entry suite.

- agent_gateway: Totals: 29 passed, 0 failed, 0 skipped, 0 blacklisted, 19ms Recorded 2026-11-21T00:02:07.6536443Z; SHA-256 fec17c551c413a2cb66bbc79712d0d7c08feb6c921cb4d9590be97a33d0a2a06. File: C:\Users\lucas\Documents\Codex\2026-11-07\for-x20\work\Breakwater-next\out\portfolio-desktop\desktop\agent_gateway-results.txt
- agent_orchestrator: Totals: 44 passed, 0 failed, 0 skipped, 0 blacklisted, 8807ms Recorded 2026-11-21T00:07:53.3197317Z; SHA-256 837363657c50615962e88ff616fc9c50dfa63f631eaff80790d78866097b419d. File: C:\Users\lucas\Documents\Codex\2026-11-07\for-x20\work\Breakwater-next\out\portfolio-desktop\desktop\agent_orchestrator-results.txt
- observed_forecast: Totals: 19 passed, 0 failed, 0 skipped, 0 blacklisted, 12124ms Recorded 2026-11-21T00:08:05.5363040Z; SHA-256 6873274bf6a85feffffde29cabbc8bc3f7653c1bfd0ee3c048b9a6372d1f1c92. File: C:\Users\lucas\Documents\Codex\2026-11-07\for-x20\work\Breakwater-next\out\portfolio-desktop\desktop\observed_forecast-results.txt
- eog_investigator: Totals: 86 passed, 0 failed, 0 skipped, 0 blacklisted, 5196ms Recorded 2026-11-21T00:02:36.4364381Z; SHA-256 c6ff10c9e2be656ecb149839af705f3c9af1251297a408e93dfe3934e96affcb. File: C:\Users\lucas\Documents\Codex\2026-11-07\for-x20\work\Breakwater-next\out\portfolio-desktop\desktop\eog_investigator-results.txt
- eog_analysis_controller: Totals: 60 passed, 0 failed, 0 skipped, 0 blacklisted, 29870ms Recorded 2026-11-21T00:03:06.4061960Z; SHA-256 93daa241470dd663e03240aadc9e8375ef47c56d7af2b7dcc12a5b8cd2533d56. File: C:\Users\lucas\Documents\Codex\2026-11-07\for-x20\work\Breakwater-next\out\portfolio-desktop\desktop\eog_analysis_controller-results.txt

The latest five Qt result files total 238 passes, zero failures and zero skips, including per-executable init/cleanup cases. They have mixed run timestamps as listed, and are not presented as one full final-candidate invocation. The two new contradictory-narrative controller rows are explicitly PASS in eog_analysis_controller-results.txt.

## Attention

Reviewer provenance: delegated same-family agent. This reviewer previously authored the EOG presentation/controller patch, so that portion is a post-implementation self-review; the AgentOrchestrator and gateway portion was read independently. No independent human or cross-family review is claimed. Scope: listed controller, gateway, QML, contract validation, archive validation and focused result files. Transcript-completeness checks were not performed; only supported visible artifacts were inspected. Fresh hosted behavior, ASan execution, complete CTest, native UI screenshots and keyboard/scaling acceptance remain outside this review. Retaining unverified raw prose intentionally does not establish that the model narrative itself is accurate.

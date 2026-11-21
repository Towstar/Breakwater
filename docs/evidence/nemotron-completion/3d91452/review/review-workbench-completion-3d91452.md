# Workbench completion review

Read-only review of `AgentOrchestrator.cpp/.hpp`, `SimulationGateway.cpp`, their focused tests and controller/QML callers. Baseline: `7df8c379e1178600bcb869cd62ab3d6751ea8a38`; frozen head: `3d91452d81af7052effb35f98afe48c6f3661b20`.

Two implementation-review findings were raised before the freeze and addressed in the inspected frozen source:

- Early completion originally could close tools before the observed session's required `get_observations` inspection. The current `inspected` gate checks current-session network/catalog inspection and observed inspection before narrowing tools.
- Repeated validation originally could close every tool before any matching plan was staged, making the response unavailable to `canApprove` / `canAcknowledge`. The current staging-only phase preserves a genuine model-issued `request_approval` opportunity. It does not automatically stage a plan, select references or apply controls.

The frozen guard rechecks current immutable evidence after the complete ordered model batch. Later successful numerical work in that batch clears the pending completion run. The staging-only receive path rejects non-staging tools before dispatch, the final-only path rejects all tools, and cancellation/reentrancy checks remain. New sessions clear the phase state. A model remains free to finish with null or decline staging; these choices do not grant review eligibility.

The gateway reuses only an explicitly validated immutable run whose validation source is `validate_run`. The first model-issued check of a search-screened run still executes the independent validator and obtains its own validation identity. Reuse validates full arguments and snapshot identity before accessing the immutable report. Feasible and infeasible reports remain distinct; repeated validation cannot turn an infeasible run into an eligible response.

Application-owned explanations distinguish absent observed coverage from absence of the native simulator. Raw model prose remains separately retained and labeled unverified. Advisory acknowledgement does not apply controls or clear an incident. Native sandbox application still requires a current matching staged plan and explicit operator action.

No additional consequential correctness issue was found in this bounded frozen-source review. This reviewer did not execute the coordinated build, CTest, ASan or native UI checks. Their results must be read from the root task's actual artifacts.

## Attention

Reviewer provenance: delegated same-family agent, independent of the workbench implementation. This reviewer authored the separate EOG investigator repeat guard, which is outside this independent workbench review. No human or cross-family review is claimed. Coverage includes the changed workbench orchestration/gateway code, focused test additions, approval/acknowledgement callers and current recommendation presentation. Complete transcript audit, clean-Windows acceptance and licensing review were not performed.

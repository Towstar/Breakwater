# BREAKWATER development

Read `For Dev Agents.md` Part A and `docs/IMPLEMENTATION_PLAN.md` before expanding scope.
The accepted desktop plan supersedes the older Python/web architecture. Preserve the handoff.

## Ownership

- Person A: `engine/`, `science/`, `proofs/`, `scenarios/`, numerical tests and benchmarks.
- Person B: `desktop/`, `contracts/`, root CMake/configuration, scripts, integration tests and delivery docs.
- Shared public contracts belong to B; coordinate changes before concurrent implementation.
- Future concurrent sessions need separate branches/worktrees after a shared baseline commit.
  Do not change another session's branch or overwrite its uncommitted work.

## Engineering rules

- Keep the numerical engine independent of Qt, Python and model APIs.
- All data and costs are synthetic. Approval applies only to simulated controls.
- Layout edits do not change physics. Numerical/topology edits invalidate old results and approval.
- Do not clamp simulated volumes or silently clip invalid controls.
- Never claim a model, proof, sanitizer or test ran without actual evidence.
- Keep credentials out of project files, QML and logs.
- Use precise floating-point semantics. Benchmark optimized and checked builds separately.
- Record implemented, planned and unavailable features in `docs/STATUS.md`.

The current request is the **minimum starting base for both people**. The full plan remains
the roadmap, not a claim that every milestone is implemented.

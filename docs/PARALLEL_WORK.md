# Parallel work: Person A and Person B

Person A is the project owner/user and owns PDEs, numerical simulation, scientific ML
and proofs. Person B owns the desktop application, Nemotron orchestration and integration.

Repository: https://github.com/Towstar/Breakwater

The initial implementation is commit `59e5e4a`. Start from the latest `origin/main`,
which also includes these kickoff documents. Read STATUS.md to distinguish working
components from planned integrations.

## Separate working directories and branches

Use separate clones on the two computers. Two sessions on one computer must also use
separate clones or worktrees. Never share a directory whose active branch can change.

Person A's branch: `codex/person-a-engine`.
Person B's branch: `codex/person-b-desktop`.

For a fresh start, clone into `Breakwater-A` or `Breakwater-B`, respectively, then create
the assigned branch from current `main`. If a checkout already exists, inspect its
status first; preserve uncommitted changes and do not reset it to follow these instructions.

## Exclusive write ownership

| Area | Writer |
|---|---|
| `engine/`, including its headers, tests and CMakeLists.txt | A |
| `science/`, including Python dependency manifests/locks and scripts inside science | A |
| `proofs/`, including Lean dependency manifests/locks | A |
| `scenarios/` | A |
| `desktop/`, including its CMakeLists.txt and integration tests | B |
| `contracts/`, including shared C++ types and wire-schema documentation | B |
| Root CMake/presets, `.gitignore`, scripts and `Breakwater.slnx` | B |
| README, AGENTS and shared `docs/` | B |

Do not fix another person's files opportunistically. Report a problem with a small
reproduction and request a change from its owner. A records progress/evidence inside
the owned subtree, for example `engine/PROGRESS.md`; B incorporates it into shared STATUS
and SCIENCE_VALIDATION documents.

## Freeze existing boundaries initially

Preserve the project-v1 format, stable IDs, existing native types/functions, control-ID
conventions and worker NDJSON envelope. A may add native capabilities in an additive
engine-owned header. B owns their eventual JSON/tool mapping.

When a shared contract must change, B publishes a separate additive contract increment
with examples. A and B adopt that same committed increment before using it. Avoid
incompatible replacements of functions already used by the desktop.

## Generated Visual Studio files

`scripts/Generate-VisualStudio.ps1` rewrites the tracked root `Breakwater.slnx`, even
with `-Headless`. B owns committing regenerated solutions. A normally builds using
`scripts/build.ps1 -Preset headless` and `-Preset asan`, which keep build output ignored.
If A needs a local IDE solution, generate it locally but keep solution-only changes
out of A's commits. Never discard other uncommitted changes to clean up a generated file.

`.tools/`, `out/`, SDKs, generated `.vcxproj` files, credentials and generated checkpoints
are not delivered by Git. Each machine needs its own supported environment.

## Integration

1. Both people push small commits to their own feature branches and open PRs. B is the
   sole integrator for `main`; do not push feature work directly to `main`.
2. B's first Nemotron/tool increment uses today's simulator and validator. It does not
   wait for A's search, transport or ML additions.
3. A delivers bounded candidate evaluation/search first, then the transport/science
   increments. B adds the search tool after its implementation is integrated.
4. Merge complete increments through PRs, then update each person's own branch from
   `origin/main`. Rebase only unshared local work; merge main into an already shared
   branch so no force push is needed. Do not both cherry-pick and later merge the same work.
5. Run numerical tests after engine integration and desktop tests after application
   integration. Record unavailable live credentials/runtime checks honestly.

Every handoff includes commit/branch, touched files, public-interface additions, actual
test commands/results, limitations, and an executable example.

If B lacks write permission, fork this public repository, work on the same feature-branch
name in the fork, and open a PR to `Towstar/Breakwater:main`. Repository access does not
need to block local work.

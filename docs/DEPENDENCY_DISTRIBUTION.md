# Candidate dependency and corresponding-source inventory

This inventory records the basic Windows candidate's inputs and notice locations.
It is not both owners' license agreement or a completed distribution approval.
The exact deployed files, hashes and retained metadata are provided by the sealed
candidate's `files-sha256.json`, `verification/package.json` and Qt SBOM directory.

The [actual ad45d45 inventory](evidence/portfolio/release-readiness/dependency-inventory-ad45d45.json)
records 42 Qt runtime DLLs, seven installed Python package records, 94 notice files,
runtime hashes and retained upstream source/license metadata. Its distribution
review and owner agreement fields remain pending. Hash equality and notice
presence alone do not complete the corresponding-source review.

| Component | Pinned input / retained material | Distribution checkpoint |
|---|---|---|
| Breakwater application and native providers | Matching source ZIP, per-source-file SHA-256 map, CMake/lockfiles and build instructions | Ethan's numerical/shared review and GPLv3 consent remain required; add the approved root license after consent. |
| Qt 6.12.0 | Installer revision `6.12.0-0-202609280346`; base, declarative, Quick3D, SVG and shader-tools SDK inputs | `share/breakwater/licenses/Qt-SBOM` retains deployed-module and source SPDX/CycloneDX metadata. Review applicable license texts, third-party notices and corresponding-source delivery for the actual deployed modules before publishing. |
| CPython 3.12.13, Windows x64 | Managed interpreter input retained by setup; `CPython-LICENSE.txt` in the package | Match the runtime inventory and notice to the actual candidate; keep optional development environments outside it. |
| Python schema dependencies | Hash-locked worker requirements; installed `.dist-info` metadata/license files under `licenses/Python-packages` | Review each package's recorded license/source location against the deployed file manifest. The basic runtime also contains pip's installed metadata. |
| Microsoft compiler runtime / redistributable | Actual DLLs and `vc_redist.x64.exe` in the candidate | Review the matching Visual Studio runtime redistribution list/terms and retain applicable notices. Presence of an installer is not this review. |
| Lucide/Feather icons | Exact existing upstream pin, `LUCIDE-LICENSE.txt` and `ASSET_SOURCES.md` | Preserve attribution and notices with the binary. |
| Logo, industrial scene and portfolio examples | Lucas-supplied logo; original code-native scene; self-authored synthetic datasets | Keep source provenance and actual capture identities. Official EOG challenge datasets and third-party weights are not substituted into the distributable examples. |

Torch and PhysicsNeMo are separate optional CPU installations, not basic-bundle
dependencies. Their real-runtime qualification is reported separately. No model
weights or NVIDIA credentials belong in the basic ZIP, source ZIP or gallery.

The package/source ZIP hashes identify immutable archives. New documentation and
qualification records may be committed later, but must identify the binary's
original application-source revision. An archive is not relabeled as a newer
binary simply because a README or screenshot changed.

Use `LICENSING.md`, `RELEASE_REVIEW.md` and the actual clean-Windows acceptance
record together before public distribution. Pending owner/license/source-delivery
checks remain pending even when automated tests pass.

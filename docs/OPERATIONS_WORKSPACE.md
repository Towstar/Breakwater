# Modern Operations observation workspace

Lucas / Person B's increment is runtime source `1b16a881fa5d5ee87269188fc4f0ef7070c5de89`, stacked on PR [#92](https://github.com/Towstar/Breakwater/pull/92). It extends the graphite instrument design to recorded-data inspection while preserving the existing sandbox.

## Delivered behavior

- An empty workspace explains the three required input files and offers explicit dataset import or a synthetic example. Loading never starts an investigation.
- Grouped system, facility, equipment and measurement selectors retain their original handlers and identities. Normal dataset metadata uses quiet secondary text; warnings retain their status color.
- A labeled observation cutoff shows the recorded UTC clock. **Latest** means the dataset's latest retained timestamp, rather than the current wall clock.
- The selected observation has a prominent value, original unit, pump-state meaning when applicable, freshness and recorded timestamp/age. Missing channels remain **No reading**, numeric zero remains a reading, and finite negative/above-capacity values remain visible. Stale values remain evidence and are unavailable as fresh measurements.
- The chart uses the existing normalization, gap handling, discrete points and unit-matched guides, with clearer typography, grid and endpoint labels. It uses available height. Small/docked views retain a visible interactive scrollbar when content overflows.
- Map labels reserve the actual navigation/footer/axes space without moving equipment or changing picks. Hidden shared action buttons cannot leave their tooltips behind.

No numerical algorithm, observation mapping, schema, operating control or approval rule changed. The synthetic example path reuses the existing local example catalog. No new dependency, hosted request or automatic analysis was added by loading/navigation.

## Verification

Both full configured build graphs passed on the development Windows machine using MSVC 19.44.35219.0 and pinned Qt 6.12.0. The normal app reopened the byte-verified saved project. Private user-project contents and their screenshots are excluded from the published gallery.

| Configuration | New Operations probes | Existing focused regression | Failures / skips |
| --- | --- | --- | --- |
| Normal | 3/3, 9.55s | 33/33, 194.39s | 0 / 0 |
| AddressSanitizer | 3/3, 35.01s | 33/33, 501.56s | 0 / 0 |

This 36-entry matrix qualifies `df404bb`, including required fixtures. Artifact review then found that the true condensed dock did not include the entire taller observation column in its scroll extent. The final `1b16a88` correction accounts for that content height and extends the compact/minimum probes to open the real dock, drag its scrollbar and require the bottom chart edge and caption inside the actual scroll clip.

| Final-source follow-up | Extended Operations probes | Affected Operations/QML regression | Failures / skips |
| --- | --- | --- | --- |
| Normal | 3/3, 10.91s | 10/10, 52.18s | 0 / 0 |
| AddressSanitizer | 3/3, 36.39s | 10/10, 151.88s | 0 / 0 |

The final-source follow-up selects 13 affected CTest entries per configuration. These combined records are focused UI/integration evidence, rather than a fresh full 91-test numerical/hosted qualification. ASan instruments owned C++ targets; prebuilt Qt/Python/Torch libraries are outside that instrumentation.

The new probes render actual 1440x900, 1100x700 and 960x540 logical clients at DPR 1.75 through D3D11/WARP. They exercise example loading, real ComboBox keyboard activation, the cutoff slider, Latest, facility/overview navigation and minimum-size scrollbar thumb dragging. Final minimum probes expose the full 210-pixel chart with the dock closed. With the dock open, compact clients expose the full 210 pixels; minimum clients expose 159 pixels at once, including the lower axis and full caption, while the upper plot remains scrollable. Rendered truck-yard labels must clear the map legend.

Four invalid verification-option combinations per build also exited with code 8 in 87-219 ms without creating the forbidden history sentinel: missing dataset, explicit history path, legacy smoke mode and duplicate verification flag. These eight process checks are separate from the CTest counts.

Three verification-only rejecting transports prohibit model/configuration reloads; explicit temporary credential/preferences/history paths prevent fallback to user state. Every probe reports zero model requests, no analysis session and unchanged sandbox document/hash/run/revision/approval. Fixtures are copied before the stale-clock extension; original repository inputs are unchanged.

The broader scope covers UI invalidation/toggles, camera framing, label placement, WARP picking, editor/project settings, telemetry lifecycle/replay, sensor forms, observed advisory review, Science, retained pipe evidence and Operations/provider/CLI rendering. A manual open investigation dock was scrolled to its observation chart without running analysis.

The earlier `328f5f8` ASan attempt passed 3/3 but its screenshots exposed the tooltip/footer defects. That original attempt is retained separately; it does not qualify the corrected source.

PR #92's original workflow [44853744952](https://github.com/Towstar/Breakwater/actions/runs/44853744952) passes all six jobs at head `ce256dd`. Main and all Person A branch heads were unchanged at the fresh GitHub recheck. Those results qualify the earlier source, not this new runtime.

[Raw build/test records, complete probe output and reviews](evidence/modern-operations/README.md) retain the exact source/app identities and original attempts. The [gallery](images/portfolio/README.md) uses three unaltered final-source synthetic-fixture PNGs.

## Reproduce

Use the existing pinned Windows toolchain and normal/ASan build setup. From a configured checkout:

```powershell
. ./scripts/ci/Initialize-Compiler.ps1
$env:NVIDIA_API_KEY = ''
$env:QSG_RHI_BACKEND = 'd3d11'
$env:QSG_RHI_PREFER_SOFTWARE_RENDERER = '1'
ctest --test-dir out/portfolio-desktop --output-on-failure -R '^desktop\.eog_workspace_'
ctest --test-dir out/portfolio-desktop --output-on-failure -R '^desktop\.(eog_(analysis|overview|facility)_.*|eog_cli|qml_smoke)$'
```

Use `out/portfolio-desktop-asan` for ASan. New probe PNGs are retained under `desktop/test-artifacts/eog-workspace` in the chosen build tree. Keep desktop input quiet during GUI checks. Raw full probe output is in retained `LastTest.log`; CTest's JUnit output truncates long JSON lines.

## Attention

Independent same-family reviewers inspected source and actual artifacts; they did not supply formal human approval or cross-family review. The bounded local decision trail cites inspected artifacts and actual checks; transcript-dependent auditing is incomplete across compaction. Dense-network packing, additional real display scaling, clean-Windows VM acceptance, new hosted rehearsals and owner/licensing release gates remain separate. This increment neither integrates the PR stack into main nor publishes a release binary.

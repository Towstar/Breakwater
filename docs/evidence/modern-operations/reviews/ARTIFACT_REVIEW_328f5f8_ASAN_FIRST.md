# Initial ASan Operations probe artifact review

Reviewed the parent's executed source `328f5f8df5c9d809e906a59ece4bd330be4b0464` probe results. This reviewer performed no UI input, test execution or rerun. The normal build and broader regression matrices are not qualified by this report.

## Actual executed scope

The original `328f5f8-asan-probe-first.log`, JUnit and JSON report3/3 passing CTests, zero failures/skips and36.20seconds total. Source/app identity in the parent record is clean `328f5f8`, ASan executable SHA256 `17cc9a9b0d0ac931af677a3feebd3c493636b4a04a283a42632d14d84d7609fc`.

| Case | Requested and actual logical client | CTest duration | Probe checks |
| --- | --- | ---: | ---: |
| Normal | 1440×900 | 12.63s | 11 |
| Compact | 1100×700 | 11.70s | 11 |
| Minimum | 960×540 | 11.85s | 12 |

All report DPR1.75, `passed:true`, `code:ok`, model request attempts0, analysis busyfalse and an empty analysis session ID. The new test transport blocks hosted requests. The minimum real thumb-drag observation reports position0.2088724584, size0.7911275416 and177logical pixels of visible chart after scrolling. These establish the declared probe assertions, not a full91-test run or hardware/field qualification.

Raw JSON observations agree across all three sizes: retained104% tank reading has age1500seconds, `stale:true`, `available:false`, `has_sample:true`; historical negative reading is-2% with age0; pump status is2state with `fault feedback`. The probe also asserts that a legitimate0flow is not missing and a missing channel is not rendered as0. Sandbox document/hash/run/revision/preview approval and absence of analysis/review evidence are checked through navigation.

## Images actually inspected

Original images are under `out/portfolio-desktop-asan/desktop/test-artifacts/eog-workspace/`:

- `breakwater-eog-ui-QqLEKQ/02-stale-recorded-reading.png`: normal loaded layout, prominent104.00%, original unit, timestamp/age and stale disclosure; chart preserves above-capacity value and threshold guides.
- `breakwater-eog-ui-ANegkZ/01-unloaded-workspace.png`: minimum onboarding fits with visible global header, required filenames and both explicit actions.
- `breakwater-eog-ui-ANegkZ/08-minimum-scrollbar-chart.png`: scrollbar and lower chart are reachable; current readout, original unit and dates remain readable. The top filter strip is partly above the viewport because this capture is intentionally scrolled.
- `breakwater-eog-ui-CKRJFb/04-discrete-status.png`: status2 and `fault feedback` are readable; discrete samples remain dots.
- `breakwater-eog-ui-CKRJFb/05-missing-is-not-zero.png`: `No reading` and `Unavailable` are shown without a bogus numeric0 or stale status string.

## Concrete polish observations

1. The normal stale snapshot retains the hidden onboarding action's `Try a synthetic example` tooltip over the map. The attached tooltip binding in shared `ActionButton` does not include effective control visibility. A visibility guard would prevent this hidden-action popup and produce cleaner UI/captures.
2. In minimum08, the map's `CUSTOM-system truck yard` label is partly obscured by its bottom legend. Label clamping currently reserves8pixels but not the legend area. Reserving the legend's actual height/margin in both base placement and collision candidates would avoid the overlap without moving the imported equipment.

These observations do not invalidate the executed no-authority/no-model assertions, but a later source fix needs its own build/render verification. Original captures must remain unchanged.

## Attention

Independent same-family artifact inspection was limited to the listed source, reports and images. The reviewer authored chart styling, so its visual inspection is self-review; the parent-authored panel/verification observations are independent same-family review. No condensed open-analysis-dock image was present in this three-probe set, so that view remains uninspected here. No normal-build execution, clean-Windows VM, additional actual display scale, complete transcript audit, hosted rehearsal or formal human approval is claimed.

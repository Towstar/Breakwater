# Operations polish artifact review: df404bb

Reviewed parent-executed probe results for clean source `df404bb8d527a9e63de257dbb1fe3d6066552b9f`. No tests, reruns or UI input were performed by this reviewer. The parent-authored panel, label/tooltip fixes and verification path receive independent same-family review; the reviewer's own chart styling receives self-review.

## Executed probe scope inspected

Normal and ASan each passed all three new workspace probes, with zero failures/skips. Original logs, JSON, JUnit and preserved full `LastTest.log` records agree. Normal total CTest time was9.55seconds; ASan was35.01seconds. The app hashes in the executed records are:

- Normal: `756629062a9bc5486cf71b4e268cd079533184d813bac6b61e92dcd98eb43d4d`.
- ASan: `a0dfe787081a8c1c48ee33aef9b2d1d68908fa361153f8e419492f01606438db`.

All three logical clients match requested sizes1440×900,1100×700 and960×540 at DPR1.75. Normal and compact report12 checks each; minimum reports13. All report `passed:true`, `code:ok`, zero model-request attempts, analysis busyfalse and an empty analysis session ID. The new rendered truck-yard/footer clearance assertion passes. Minimum's actual scrollbar thumb interaction reports position0.2088724584,size0.7911275416 and177logical pixels of visible chart.

These results qualify the three new probes per build, not an unexecuted broader matrix or full91-test run.

## Artifact observations

- Fresh normal `breakwater-eog-ui-NpfYPG/02-stale-recorded-reading.png` no longer contains the hidden onboarding action's tooltip. Stale104.00%, original unit,1500-second age and the explicit unavailable-as-fresh disclosure remain readable.
- Fresh normal minimum `breakwater-eog-ui-HBjQdV/08-minimum-scrollbar-chart.png` shows the truck-yard and disposal labels above the map footer, with their leaders visible. The map footer no longer masks the truck-yard label. The chart and stale readout are reachable and readable through the actual scrollbar.
- Fresh ASan `breakwater-eog-ui-cFPaFZ/02-stale-recorded-reading.png` and `breakwater-eog-ui-bxklNq/08-minimum-scrollbar-chart.png` show the same corrections. The underlying negative-to-above-capacity observations and three source threshold guides remain visible.
- The parent's self-authored NORTH/SOUTH example overview shows78.44% at the recorded cutoff with original units, dataset selection strip, clear plot grid and readable threshold captions.
- The manual open-Plans-dock view clearly reports unavailable candidates and no retained trajectory; it does not suggest that analysis or validation was performed. The corresponding scrolled view reaches the observation chart without closing the dock.

The automated images above are under their respective normal/ASan build `desktop/test-artifacts/eog-workspace/` directories. Manual source artifact names were `manual-df404bb-overview.png`, `manual-df404bb-condensed.png` and `manual-df404bb-condensed-scrolled.png`. Their bytes have JPEG signatures, not PNG signatures; byte-preserving `.jpg` publication names are appropriate.

## Attention

No unresolved issue was observed in these corrected normal/ASan probe frames. The manual dock images are1442-pixel Sky window captures of the default1440logical client and still show the dataset selector header. They demonstrate docked normal layout, not proof of the QML `condensed:true` branch, which requires a client below1280pixels in `Main.qml`. The scrolled dock screenshot keeps the chart readable, while the selected-channel identity/readout can scroll above the viewport.

### Subsequent compact-dock artifact inspection

The fresh normal `desktop/eog-analysis-compact.png` and matching JSON were inspected afterward. Their hidden dataset-selector header and visible Plans dock exercise the actual compact condensed branch. The report records a ready scene and trend,3guides,2samples,3visible labels, unchanged workbench and no requested analysis or hosted model run.

The screenshot shows the lower observation-chart UTC axis/caption and part of the scene axes clipped by the upper workspace viewport, without an obvious scrollbar thumb. This snapshot alone does not establish whether a later settled viewport can scroll to all content. The parent was asked to verify the actual condensed scroll path before claiming full reachability. A minimal meaningful extension would open the real dock action in the compact probe, scroll the workspace to the chart and compare its bounds with the EOG panel's clipped viewport rather than the whole window, which includes the lower dock. The condensed evidence-grid preferred height may need to account for the observation column's implicit height if this new assertion fails.

This review does not qualify clean Windows, additional real display scales, dense arbitrary topology packing, a live hosted investigation or a complete transcript audit. Earlier `328f5f8` images/results remain historical and must not be relabeled as this fixed source.

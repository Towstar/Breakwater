# Condensed observation height correction: source review

Reviewed the parent-authored working change to `EogDatasetPanel.qml` after `df404bb`. No reviewer source edits, UI input or tests were performed.

No consequential source issue or new direct height-binding loop was found. The grid's preferred height now includes `observationColumn.implicitHeight`, which derives from the readout/text content and the chart's fixed210-pixel implicit height. It does not derive from that column's assigned fill height. This lets the outer layout/ScrollView account for the complete value/readout, chart and caption when the dock reduces available height. The reserved scroll gutter remains fixed, avoiding a scrollbar-visibility/width-wrap feedback path.

The existing viewport-fill target remains available for larger workspaces. New object names expose the ScrollView and observation caption for meaningful test geometry; no filter, cutoff, measurement, unit, model, approval or imported-equipment handler changed. The active Main window has a960-pixel minimum width and therefore uses the existing two-column evidence grid.

## Attention

This is independent same-family source inspection, not a runtime binding-loop or scrolling pass. The fresh compact-dock probe should verify the actual panel-clipped viewport and the complete chart/caption through real scrollbar interaction. The old `df404bb` smoke image showed clipped lower axis/caption and did not establish reachability. A later successful old-binary regression cannot qualify this working change. One-column embedding below the current Main minimum width and additional actual display scales are outside this review scope.

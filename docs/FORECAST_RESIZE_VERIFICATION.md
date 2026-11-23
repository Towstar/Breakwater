# Forecast panel resizing

November 23, 2026. Application source
`0257c1d8ff1d5f55ecf7b81f1c921b15e85f7f7b`, following PR #99's original
`72a568d` head.

The empty-state text and action were centered inside a rectangle that could
become shorter than the content. They could overlap the heading and extend
below the results panel. The Forecast pane now has a clipped scroll viewport
around its complete content, intrinsic minimum heights for the empty state and
chart, and reserved space for a visible scrollbar. Short panes keep the heading,
chart, action and timeline reachable without drawing over adjacent UI.

Existing rendered checks now resize empty and executed forecasts with a
160-unit results dock, including 1008x672 and 960x540 windows. They check content
bounds, scroll the native forecast action and timeline into view, run a real
native forecast, restore the original window/preferences and verify that project
inputs, numerical identity and approval eligibility were preserved. They do not
apply controls.

## Verification

Final-source normal checks pass 8/8 with zero skips, covering 1440x900,
1100x700, 1008x672 and 960x540 at DPR 1.75. The initial ASan matrix at DPR 1
passes 7/8. Its short-window Settings focus-return check failed at phase 265
around the user's restoration of the existing app; that timing is a correlation,
not an established source defect or root cause. After saving and closing the old
app, the unchanged isolated check passes 1/1 in 15.67 seconds with zero model
request attempts. The original 7/8 result remains retained; this is not reported
as a clean 8/8 ASan matrix. All four ASan Forecast resize checks passed.
The preceding inner-body-only implementation passed an isolated ASan minimum case
and eight normal checks. Visual inspection then motivated scrolling the entire
pane, so those runs do not qualify the final source.

Logs, JUnit reports and actual rendered captures are retained separately under
`out/forecast-resize/`. The normal app was built to a temporary output so the
existing open workspace could remain untouched during verification.

Final normal evidence is `normal-verified.log/xml` (8/8, 92.12 seconds).
ASan evidence is `asan-verified.log/xml` (7/8, 211.39 seconds) and
`asan-focus-recheck.log/xml` (1/1). The temporary normal runtime output was
removed from the build configuration after verification. A byte-identical copy
of the checked executable was placed at the usual local desktop path after the
old app closed.

The final app was reopened with the fresh saved project and normal user history.
Actual D3D11 logs confirm hardware rendering with software preference disabled.
Manual inspection confirmed that scrolling the short Forecast pane reveals its
native forecast action, with approval still disabled before forecasting. Only
the latest application instance remained open; no operating controls were applied.

The original PR #99 CI passed five jobs. Its ASan desktop job passed 94/98;
four Compare/Settings probes exhausted a two-second frame wait immediately
after changing visual quality. The test driver now permits six seconds only
for quality changes and restoring the preset, which rebuild 3D resources.
It still requires both rendered frames. Other frame waits and the global UI
watchdog are unchanged. No numerical, model, freshness, evidence or approval
check was removed or relaxed. Current-head hosted CI is separate from local
verification; the original failed run remains preserved.

This is a presentation fix and focused UI verification. It does not establish
new hosted Nemotron qualification, scientific accuracy, clean-Windows acceptance
or a public binary release.

# Workspace clarity increment

This keeps the industrial equipment, neutral yard and schematic layout model.
It improves information density and navigation without adding hydraulic meaning
to scene positions or changing operating controls.

## Labels and selection

Scene labels use projected screen rectangles with deterministic priority: primary
selection, hover, other selected equipment, tanks and other equipment. Labels
avoid each other, the navigation header, viewport edges and the orientation
indicator. Long labels elide with their full name available on hover. The Network
list retains every asset, independently of which labels fit the current view.
View → More equipment labels adds labels where space permits. Every primary
equipment body retains its original picking identity.

The placement function is a small view-only JavaScript module. Native QJSEngine
tests cover priority, deterministic ordering, unchanged input arrays, 128 dense
candidates, reserved regions and invalid/tiny viewports. It does not edit network,
layout, measurements or numerical evidence.

## Charts and first use

The empty Forecast panel now explains its native calculation and provides a
direct Run native forecast action. It calls the same existing controller method
as the toolbar. No hosted request or control application follows that action.
Once a run exists, the tank selector and timeline reappear.

Trajectory legends wrap, units appear once, and large/small tick values use
bounded scientific notation. Axis margins and capacity-label placement keep
labels inside the chart. Stored negative/overflow trajectories remain present;
the chart does not clip values to storage capacity.

F1 or Mouse recommended opens a scrollable controls dialog with a top-right X,
footer Close and Escape dismissal. Ctrl+0 fits the network, Ctrl+Home restores
the saved view and F focuses selected equipment. Text editing and visible dialogs
keep the existing shortcut guards. Native-event checks open the help, attempt
Ctrl+D while it is open, dismiss it and use view shortcuts while verifying that
project data and numerical approval eligibility are preserved.

## Executed checks

Normal and MSVC AddressSanitizer each pass five placement Qt cases and eight
actual native UI suites: four editor/keyboard layouts and four observed
capture/search/advisory layouts. Sizes are normal, compact, 1008×672 and 960×540.
The editor functional harness now allows sixty seconds for the additional
onboarding/keyboard actions, with a ninety-second CTest bound. The observed-only
functional allowance remains forty seconds. Production numerical/model deadlines
and expiry policies are unchanged.

The final scene sweeps identify the actual AMD Radeon 880M adapter on the render
thread through Qt's [renderer interface](https://doc.qt.io/qt-6/qsgrendererinterface.html).
The immutable description is queued back to the report thread. A renderer flag
alone is not treated as proof of hardware execution.

| Normal build, AMD hardware | Default | Dense fixture |
| --- | ---: | ---: |
| Assets / routes | 6 / 6 | 128 / 256 |
| Average FPS | 62.49 | 62.41 |
| 95th percentile frame interval | 18.34 ms | 28.88 ms |
| Final-view body probes | 6 / 6 | 128 / 128 |
| Final-view label overlaps | 0 | 0 |
| Visible labels with extra labels enabled | 6 | 19 |

These are sequential camera sweeps at 1440×900, real DPR 1.75, with 2.5 seconds
of warmup and ten seconds of measurement. The default uses detailed equipment;
the dense overview uses the existing reduced-detail policy. Both keep numerical
identity unchanged. The 60/30 FPS targets pass on this laptop. These measurements
are not a guarantee for other hardware, arbitrary camera views or loaded systems.

A separate ASan/WARP probe correctly identifies Microsoft's software renderer.
It records approximately 12.6 FPS and five of six body probes in that final rotated
view, and exits with the partial-picking failure. It is retained, not called a
hardware-target pass. The eight normal/ASan functional UI suites above run through
D3D11/WARP and pass their actual action/render checks. See the complete scope,
checksums, raw reports and retained attempts in
`evidence/v03/workspace-clarity.json`.

## Real Windows scaling and remaining acceptance

The first attempt to open Settings was interrupted; that dated checkpoint remains
in the component evidence. A later pass used actual Windows Display settings at
100%, 125%, 150% and 200% on the 2880x1800 laptop display. At every scale, new
installed `62052b9` app processes passed both editor and observed-publisher UI
checks, including the compact advisory path. Actual DPRs were 1, 1.25, 1.5 and 2;
all eight workflows passed with no Qt scale environment override. The original
175% scale was restored and visibly verified. PR80 changes transport only; its
private candidate also passed the complete bundle verifier at actual 200% scale.

The [final verification index](evidence/v03/final-verification.json) binds these
results to exact sources/binaries and retained logs. This remains laptop evidence.
Actual clean-Windows VM/account acceptance, owner/licensing review and hosted
completion remain release gates. No root license, main integration or public
binary release is supplied by this visual increment.

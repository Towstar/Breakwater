# ASan UI failure triage, source 3d91452

Read-only triage of tests-asan-3d91452/ctest.log and UI fixture artifacts. No tests launched and no source changed.

Observed failures:

- UI normal, 1440x900: ui_check_timeout at phase50, 60049ms. This is the global60s verification deadline before a local search transition; it is not an ASan memory-error report.
- UI compact, 1100x700: route_filter_clear_changed_selection_or_count at phase74, 14627ms. The combined condition requires empty networkFilter.text, six filtered routes and selection backup_pump. The result records observed={} and only screenshots before this action; it does not identify which term failed.
- UI minimum, 960x540: route_cue_changed_primary_picking at phase95, 20756ms. The recorded bypass cue is present/visible, exactly one active cue, two neutral nonpickable models with expected geometry; body_hit is empty. This is a missing route ray hit, not evidence that the cue intercepted picking.
- UI short passed. Root reports the same-source normal-build UI checks passed.

Input and timing boundary:

WorkbenchUiCheck.cpp87-106 dispatches synchronous QMouseEvent and QKeyEvent with QCoreApplication::sendEvent to its own QQuickWindow. It does not use globally routed SendInput or QTest::mouseClick. Therefore a concurrent Sky action cannot directly redirect these synthetic events to another window. Native activation/exposure, global user input arriving during phases, and CPU/GPU/render contention can still interfere. The check contains explicit active-window/focus stabilization for other shortcut phases, but phase73/74 has one fixed event-loop interval between click and assertion and no recorded before/after diagnostics. The timer interval is100ms. Route picking waits for two frameSwapped signals, with a separate2s bound; that gate passed before the no-hit assertion.

Main.qml selection/clear behavior remains straightforward: selectRoute calls controller.select(id); clearNetworkFilter.onClicked invokes networkFilter.clear() and forceActiveFocus(), without changing selection. No Main.qml, ActionButton.qml or WorkbenchUiCheck.cpp change exists between baseline7df8c379 and frozen3d91452. This does not prove the older behavior is fault-free, but no new navigation-code regression is demonstrated by this diff.

Recommendation: finish the current full ASan invocation, keep its raw failures, then rerun the same executable and four UI entries with Sky/user input paused, without another CTest invocation sharing that build. Preserve the existing assertions, frame/deadline limits and executable hash. A repeat of phase74 requires recording actual text/count/selection, visibility and button/window coordinates before considering a source fix. A repeated no-hit requires camera/viewport/projected-point and render state rather than weakening pick assertions. No source fix is justified solely by the present incomplete compact diagnostics.

Attention: same-family delegated read-only triage, not independent human acceptance. Cause remains INCONCLUSIVE; concurrent UI activity is a hypothesis, not an established explanation. Quiet reruns and live rendered observation are not performed by this review. No assertion has been weakened and no failed attempt is discarded.
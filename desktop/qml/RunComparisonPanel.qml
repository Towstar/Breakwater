pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: comparison
    objectName: "runComparisonPanel"
    required property var history
    signal investigationRequested()
    signal historyRequested()
    readonly property var storedRuns: visible ? history.runs : []
    readonly property var evidence: visible ? history.comparison || ({}) : ({})
    readonly property bool hasRuns: storedRuns.length > 0
    readonly property bool retired: storedRuns.some(run => run.current === false)
    readonly property bool pairSelected: !!history.baselineRunId && !!history.candidateRunId && history.baselineRunId !== history.candidateRunId
    readonly property bool compatible: pairSelected && evidence.compatible === true
    readonly property bool wide: width >= 900
    readonly property bool dialogOpen: changesDialog.visible
    readonly property var runChoices: wide ? [] : storedRuns.map(run => ({id: run.run_id,
        label: (run.candidate_id || "Run") + " · " + shortId(run.run_id) + " · " + number(run.cost) + " units"}))
    spacing: 10

    function number(value, digits) {
        return value !== undefined && value !== null && Number.isFinite(Number(value))
            ? Number(value).toFixed(digits === undefined ? 2 : digits) : "n/a"
    }
    function shortId(value) { return value ? String(value).substring(0, 8) : "none" }
    function indexOfRun(id) { return runChoices.findIndex(run => run.id === id) }
    function validationText(run) {
        if (!run || !run.run_id) return "No run selected"
        const verdict = (run.validation || {}).feasible
        if (typeof verdict !== "boolean") return "Validation unavailable"
        return (run.validation_status === "validated" ? "Validated " : "Reported ")
            + (verdict ? "feasible" : "infeasible")
            + (run.validation_status === "validated" ? "" : " · validation not requested")
    }
    function violationText(issue) {
        if (!issue || !issue.code) return "No violation reported"
        return (issue.asset_id || issue.code) + (issue.time_h !== undefined ? " at " + number(issue.time_h, 3) + " h" : " · " + issue.code)
    }
    component Copy: Label { textFormat: Text.PlainText; color: Theme.text; font.pixelSize: Theme.smallSize; wrapMode: Text.WordWrap }
    component RunSummary: Frame {
        id: summary
        property var run: ({})
        property var violation: ({})
        property string roleLabel
        Layout.fillWidth: true; Layout.alignment: Qt.AlignTop; padding: 12
        background: Rectangle { color: Theme.raised; radius: Theme.cornerRadius; border.color: Theme.divider }
        contentItem: ColumnLayout {
            spacing: 6
            Copy { Layout.fillWidth: true; text: summary.roleLabel + " · " + comparison.shortId(summary.run.run_id); font.weight: Font.DemiBold }
            Copy { Layout.fillWidth: true; text: comparison.validationText(summary.run); color: Theme.secondary; font.pixelSize: Theme.captionSize }
            Copy { Layout.fillWidth: true; text: "Cost  " + comparison.number(summary.run.cost) + " synthetic units\nHorizon  " + comparison.number(summary.run.horizon_h) + " h" }
            Copy { Layout.fillWidth: true; text: "Minimum headroom  " + comparison.number((summary.run.validation || {}).minimum_headroom_m3) + " m³" }
            Copy { Layout.fillWidth: true; text: comparison.violationText(summary.violation); color: Theme.secondary; font.pixelSize: Theme.captionSize; wrapMode: Text.WrapAnywhere }
        }
    }

    RowLayout {
        Layout.fillWidth: true; spacing: 12
        Copy { text: "Run comparison"; font.pixelSize: 16; font.weight: Font.DemiBold }
        Item { Layout.fillWidth: true }
        Copy { text: comparison.history.historical ? "HISTORICAL · READ ONLY" : comparison.retired ? "RETAINED · SOURCE CHANGED" : "CURRENT SESSION"; color: comparison.retired ? Theme.warning : Theme.secondary; font.pixelSize: Theme.captionSize }
        ActionButton { objectName: "comparisonReturnCurrent"; text: "Return to current"; iconName: "rotate-ccw"; visible: comparison.history.historical; onClicked: comparison.history.showLive() }
    }
    Copy { objectName: "comparisonSourceWarning"; Layout.fillWidth: true; visible: comparison.retired && !comparison.history.historical; text: "These runs describe their original captured snapshot. The current network has changed."; color: Theme.warning; font.pixelSize: Theme.captionSize }
    ScrollView {
        id: comparisonScroll
        objectName: "comparisonScroll"
        Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumHeight: 0
        contentWidth: availableWidth; clip: true
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AlwaysOn }
        ColumnLayout {
            width: comparisonScroll.availableWidth; spacing: 12
            Frame {
                objectName: "comparisonEmptyState"
                visible: !comparison.hasRuns; Layout.fillWidth: true
                Layout.preferredHeight: Math.max(164, comparisonScroll.availableHeight)
                padding: 16
                background: Rectangle { color: Theme.raised; radius: Theme.cornerRadius; border.color: Theme.divider }
                contentItem: Item {
                    ColumnLayout {
                        anchors.centerIn: parent; width: Math.min(500, parent.width); spacing: 12
                        Image { visible: comparisonScroll.availableHeight >= 240; source: Theme.icon("git-compare-arrows"); sourceSize.width: 24; sourceSize.height: 24; Layout.alignment: Qt.AlignHCenter; Accessible.ignored: true }
                        Copy { Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter; text: "No runs to compare yet"; font.pixelSize: 17; font.weight: Font.DemiBold }
                        Copy { Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter; color: Theme.secondary; text: "Use Local search or a Nemotron investigation to retain numerical runs, or reopen a saved session from History." }
                        Flow {
                            Layout.fillWidth: true; spacing: 8; flow: Flow.LeftToRight
                            ActionButton { objectName: "comparisonGoInvestigate"; text: "Go to Investigate"; iconName: "workflow"; onClicked: comparison.investigationRequested() }
                            ActionButton { objectName: "comparisonBrowseHistory"; text: "Browse History"; iconName: "history"; onClicked: comparison.historyRequested() }
                        }
                    }
                }
            }
            GridLayout {
                id: compactSelectors
                visible: comparison.hasRuns && !comparison.wide; Layout.fillWidth: true
                columns: comparison.width < 560 ? 1 : 2; columnSpacing: 12; rowSpacing: 10
                ColumnLayout {
                    Layout.fillWidth: true; spacing: 4
                    Copy { text: "Baseline"; color: Theme.secondary }
                    AppComboBox {
                        objectName: "comparisonBaselineSelector"; Layout.fillWidth: true
                        model: comparison.runChoices; textRole: "label"; valueRole: "id"
                        currentIndex: comparison.indexOfRun(comparison.history.baselineRunId)
                        displayText: currentIndex < 0 ? "Choose baseline" : currentText
                        Accessible.name: "Baseline run"
                        onActivated: comparison.history.selectRuns(currentValue, comparison.history.candidateRunId)
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true; spacing: 4
                    Copy { text: "Candidate"; color: Theme.secondary }
                    AppComboBox {
                        objectName: "comparisonCandidateSelector"; Layout.fillWidth: true
                        model: comparison.runChoices; textRole: "label"; valueRole: "id"
                        currentIndex: comparison.indexOfRun(comparison.history.candidateRunId)
                        displayText: currentIndex < 0 ? "Choose candidate" : currentText
                        Accessible.name: "Candidate run"
                        onActivated: comparison.history.selectRuns(comparison.history.baselineRunId, currentValue)
                    }
                }
            }
            RowLayout {
                visible: comparison.hasRuns; Layout.fillWidth: true
                Layout.preferredHeight: Math.max(405, comparisonScroll.availableHeight - (compactSelectors.visible ? compactSelectors.implicitHeight + 12 : 0))
                spacing: 14
                Frame {
                    visible: comparison.wide; Layout.preferredWidth: 245; Layout.maximumWidth: comparison.width * 0.3
                    Layout.fillHeight: true; padding: 10
                    background: Rectangle { color: Theme.raised; radius: Theme.cornerRadius; border.color: Theme.divider }
                    contentItem: ColumnLayout {
                        spacing: 9
                        Copy { text: "STORED RUNS · " + comparison.storedRuns.length; color: Theme.secondary; font.pixelSize: Theme.captionSize }
                        ListView {
                            id: runList; objectName: "comparisonRunList"
                            Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 8
                            model: comparison.wide ? comparison.storedRuns : []
                            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AlwaysOn }
                            delegate: Frame {
                                id: runCard; required property var modelData
                                width: Math.max(0, runList.width - 12); padding: 10
                                background: Rectangle { color: Theme.surface; radius: Theme.cornerRadius; border.color: Theme.divider }
                                contentItem: ColumnLayout {
                                    spacing: 7
                                    Copy { Layout.fillWidth: true; text: (runCard.modelData.candidate_id || "Run") + " · " + comparison.shortId(runCard.modelData.run_id); font.weight: Font.DemiBold; wrapMode: Text.WrapAnywhere }
                                    Copy { Layout.fillWidth: true; text: comparison.validationText(runCard.modelData); color: Theme.secondary; font.pixelSize: Theme.captionSize }
                                    Copy { Layout.fillWidth: true; text: comparison.number(runCard.modelData.cost) + " units · " + comparison.number(runCard.modelData.horizon_h) + " h"; color: Theme.secondary; font.pixelSize: Theme.captionSize }
                                    RowLayout {
                                        Layout.fillWidth: true; spacing: 6
                                        ActionButton { Layout.fillWidth: true; text: "Baseline"; checked: comparison.history.baselineRunId === runCard.modelData.run_id; onClicked: comparison.history.selectRuns(runCard.modelData.run_id, comparison.history.candidateRunId) }
                                        ActionButton { Layout.fillWidth: true; text: "Candidate"; checked: comparison.history.candidateRunId === runCard.modelData.run_id; onClicked: comparison.history.selectRuns(comparison.history.baselineRunId, runCard.modelData.run_id) }
                                    }
                                }
                            }
                        }
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true; Layout.fillHeight: true; spacing: 10
                    RowLayout {
                        Layout.fillWidth: true; spacing: 10
                        Copy { text: "Tank"; color: Theme.secondary }
                        AppComboBox {
                            objectName: "comparisonTankSelector"; Layout.preferredWidth: Math.min(210, comparison.width * 0.35); Layout.maximumWidth: 240
                            model: comparison.history.tanks; textRole: "label"; valueRole: "id"; enabled: count > 0
                            currentIndex: comparison.history.tanks.findIndex(tank => tank.id === comparison.history.selectedTankId)
                            displayText: currentIndex < 0 ? "No stored tanks" : currentText
                            Accessible.name: "Tank trajectory"
                            onActivated: comparison.history.selectTank(currentValue)
                        }
                        Copy {
                            objectName: "comparisonCompatibility"; Layout.fillWidth: true; font.pixelSize: Theme.captionSize
                            color: comparison.pairSelected && !comparison.compatible ? Theme.warning : Theme.secondary
                            text: comparison.compatible ? "Same captured snapshot and horizon"
                                : comparison.pairSelected ? "Different captured states or horizons. Showing the candidate only."
                                : "Single-run view. Choose a different run to compare."
                        }
                    }
                    TrajectoryChart {
                        Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumHeight: 160
                        chart: comparison.visible ? comparison.history.chart : ({})
                        emptyText: "No stored trajectory is available for this run and tank."
                    }
                    GridLayout {
                        Layout.fillWidth: true; columns: comparison.width < 560 ? 1 : 2; columnSpacing: 10; rowSpacing: 10
                        RunSummary { objectName: "comparisonBaselineSummary"; visible: comparison.pairSelected; roleLabel: "BASELINE"; run: comparison.evidence.baseline || ({}); violation: comparison.evidence.baseline_violation || ({}) }
                        RunSummary { objectName: "comparisonCandidateSummary"; roleLabel: comparison.pairSelected ? "CANDIDATE" : "SELECTED RUN"; run: comparison.evidence.candidate || ({}); violation: comparison.evidence.candidate_violation || ({}) }
                    }
                    RowLayout {
                        Layout.fillWidth: true; visible: comparison.pairSelected
                        Copy { objectName: "comparisonCostDelta"; Layout.fillWidth: true; visible: comparison.compatible; text: "Candidate − baseline: " + comparison.number(comparison.evidence.cost_delta) + " synthetic units" }
                        Item { Layout.fillWidth: true; visible: !comparison.compatible }
                        ActionButton { id: changesButton; objectName: "comparisonControlChanges"; text: "Control changes"; iconName: "settings-2"; enabled: comparison.compatible; onClicked: changesDialog.open() }
                    }
                }
            }
        }
    }
    Dialog {
        id: changesDialog; objectName: "comparisonChangesDialog"; parent: Overlay.overlay; anchors.centerIn: parent
        width: Math.min(720, parent.width - 32); height: Math.min(430, parent.height - 32)
        modal: true; closePolicy: Popup.CloseOnEscape; title: "Baseline to candidate control changes"
        header: DialogHeading { owner: changesDialog }
        footer: DialogFooter { owner: changesDialog }
        onClosed: { if (changesButton.visible) changesButton.forceActiveFocus(Qt.PopupFocusReason); else comparison.forceActiveFocus() }
        contentItem: ScrollView {
            id: changesScroll; contentWidth: availableWidth; clip: true
            ColumnLayout {
                width: changesScroll.availableWidth; spacing: 12
                Copy { Layout.fillWidth: true; text: comparison.evidence.reason || ""; color: Theme.secondary }
                Repeater {
                    model: comparison.compatible ? comparison.evidence.control_changes || [] : []
                    delegate: RowLayout {
                        required property var modelData; Layout.fillWidth: true; spacing: 14
                        Copy { Layout.fillWidth: true; text: modelData.control_id; wrapMode: Text.WrapAnywhere }
                        Copy { text: comparison.number(modelData.before) + " → " + comparison.number(modelData.after) + " " + (modelData.unit === "m3/h" ? "m³/h" : modelData.unit || "") }
                    }
                }
                Copy { Layout.fillWidth: true; visible: comparison.compatible && !(comparison.evidence.control_changes || []).length; text: "No control settings differ between these runs."; color: Theme.secondary }
            }
        }
    }
}

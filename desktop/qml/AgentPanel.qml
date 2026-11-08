pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

ColumnLayout {
    id: panel
    required property var investigator
    required property var credentials
    required property var history
    required property var scientificWorker
    required property var storageScience
    readonly property bool scienceVisible: tabs.currentIndex === 3
    readonly property var reviewData: investigator.review || ({})
    readonly property var comparisonData: history.comparison || ({})
    spacing: 6
    function showScience() { tabs.currentIndex = 3 }
    function number(value, digits) {
        return value !== undefined && value !== null && Number.isFinite(Number(value))
                ? Number(value).toFixed(digits === undefined ? 2 : digits) : "—"
    }
    function shortId(value) { return value ? String(value).substring(0, 8) : "none" }
    function unit(value) { return value === "m3/h" ? "m³/h" : (value || "") }
    function validationText(run) {
        if (!run || !run.run_id) return "No run selected"
        const feasible = run.validation && run.validation.feasible
        if (run.validation_status === "validated") return feasible ? "Validated feasible" : "Validated infeasible"
        return (feasible ? "Reported feasible" : "Reported infeasible") + " · validation not requested"
    }
    function violationText(issue) {
        if (!issue || !issue.code) return "No violation reported"
        return (issue.asset_id || issue.code) + (issue.time_h !== undefined ? " at " + number(issue.time_h, 3) + " h" : " · " + issue.code)
    }
    function metricText(run, issue) {
        if (!run || !run.run_id) return "No run selected"
        return "Cost " + number(run.cost) + " · headroom " + number((run.validation || {}).minimum_headroom_m3) + " m³\n" + violationText(issue)
    }
    component PlainLabel: Label { textFormat: Text.PlainText; color: "#d2e1e9"; font.pixelSize: 12 }
    component SectionLabel: PlainLabel { color: "#9eb5c2"; font.pixelSize: 10; font.letterSpacing: 1 }
    Connections {
        target: panel.history
        function onChanged() {
            if (panel.history.historical && tabs.currentIndex === 0) tabs.currentIndex = 1
        }
    }

    Dialog {
        id: keyDialog
        objectName: "nemotronKeyDialog"
        title: "NVIDIA API key"
        modal: true
        anchors.centerIn: Overlay.overlay
        width: Math.min(460, panel.width - 28)
        onOpened: apiKeyInput.forceActiveFocus()
        onClosed: apiKeyInput.clear()
        contentItem: ColumnLayout {
            spacing: 12
            PlainLabel {
                Layout.fillWidth: true; wrapMode: Text.WordWrap
                text: "Paste your NVIDIA API key. It is saved locally for this Windows account and used for future investigations."
            }
            TextField {
                id: apiKeyInput; objectName: "nemotronApiKeyInput"
                Layout.fillWidth: true; placeholderText: "NVIDIA API key"
                echoMode: TextInput.Password; maximumLength: 4096
                selectByMouse: true
                onAccepted: { if (saveKeyButton.enabled) saveKeyButton.clicked() }
            }
            PlainLabel {
                Layout.fillWidth: true; wrapMode: Text.WordWrap
                text: panel.credentials.status
            }
            RowLayout {
                Button {
                    id: saveKeyButton; objectName: "saveNemotronKey"; text: "Save key"; highlighted: true
                    enabled: apiKeyInput.text.trim().length > 0 && !panel.investigator.running
                    onClicked: { if (panel.credentials.saveApiKey(apiKeyInput.text)) { apiKeyInput.clear(); keyDialog.close() } }
                }
                Button {
                    text: "Remove saved key"; enabled: panel.credentials.saved && !panel.investigator.running
                    onClicked: panel.credentials.forgetApiKey()
                }
                Item { Layout.fillWidth: true }
                Button { text: "Close"; onClicked: keyDialog.close() }
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        PlainLabel {
            Layout.fillWidth: true; wrapMode: Text.WordWrap
            text: panel.credentials.status
            color: panel.credentials.configured ? "#9eb5c2" : "#ffcf70"
        }
        Button {
            objectName: "configureNemotronKey"
            text: panel.credentials.configured ? "API key settings" : "Set API key"
            enabled: !panel.investigator.running
            onClicked: keyDialog.open()
        }
    }

    RowLayout {
        Layout.fillWidth: true; spacing: 8
        TextField {
            id: objective; Layout.fillWidth: true
            text: "Our main transfer pump failed. Find a validated response for the next three hours at low modeled cost."
            placeholderText: "Operator objective"; maximumLength: 4000; enabled: !panel.investigator.running
            onAccepted: { if (investigateButton.enabled) investigateButton.clicked() }
        }
        Button {
            id: investigateButton; text: "Investigate"; highlighted: true
            enabled: !panel.investigator.running && objective.text.trim().length > 0
            onClicked: { panel.history.showLive(); tabs.currentIndex = 0; panel.investigator.investigate(objective.text) }
        }
        Button { text: "Cancel"; enabled: panel.investigator.running; onClicked: panel.investigator.cancel() }
        Button { text: "Review staged plan"; enabled: !!panel.reviewData.run_id || panel.investigator.canApprove; onClicked: reviewDialog.open() }
    }
    PlainLabel {
        Layout.fillWidth: true; elide: Text.ElideRight; color: "#60d2d1"
        text: tabs.currentIndex === 2 ? panel.history.status : tabs.currentIndex === 3 ? panel.storageScience.status : panel.investigator.status
    }
    RowLayout {
        Layout.fillWidth: true
        TabBar {
            id: tabs; objectName: "agentTabs"; Layout.preferredWidth: 510; Layout.maximumWidth: panel.width * 0.6
            TabButton { text: "Investigation"; onClicked: panel.history.showLive() }
            TabButton { text: "Comparison" }
            TabButton { text: "History"; onClicked: panel.history.refresh() }
            TabButton { text: "Science" }
        }
        Item { Layout.fillWidth: true }
        PlainLabel { text: panel.scienceVisible ? "CURRENT NATIVE FORECAST · READ ONLY" : panel.history.historical ? "HISTORICAL · READ ONLY" : "CURRENT SESSION"; color: !panel.scienceVisible && panel.history.historical ? "#ffcf70" : "#9eb5c2"; font.pixelSize: 10 }
        Button { visible: panel.history.historical && !panel.scienceVisible; text: "Return to current"; onClicked: panel.history.showLive() }
    }
    StackLayout {
        Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumHeight: 0
        currentIndex: tabs.currentIndex
        RowLayout {
            spacing: 16
            ColumnLayout {
                Layout.fillWidth: true; Layout.fillHeight: true; spacing: 4
                SectionLabel { text: "RECOMMENDATION / STORED EVIDENCE" }
                ScrollView {
                    Layout.fillWidth: true; Layout.fillHeight: true; contentWidth: availableWidth
                    TextArea {
                        readOnly: true; selectByMouse: true; wrapMode: TextEdit.Wrap; textFormat: TextEdit.PlainText; font.pixelSize: 12
                        text: {
                            const r = panel.investigator.recommendation
                            if (!r || !r.status) return "Start an investigation on the current network. Numerical tools execute the C++ engine; recommendations require stored validation evidence.\n\nSimulated controls change only after you review and apply a validated plan.\n\n" + panel.investigator.storageStatus
                            let summary = (r.explanation || "") + "\n\nRun: " + (r.selected_run_id || "none")
                            if (r.ok) summary += "\nCost: " + panel.number(r.cost) + " synthetic units · Horizon: " + panel.number(r.horizon_h) + " h\nValidation: " + (r.validation_id || "none")
                            return summary + "\nApproval: " + (r.approval_status || "unavailable") + "\n\nSynthetic model. Quality is not evaluated.\n" + panel.investigator.storageStatus
                        }
                    }
                }
            }
            ColumnLayout {
                Layout.preferredWidth: Math.min(370, panel.width * 0.36); Layout.fillHeight: true; spacing: 4
                SectionLabel { text: "ACTIVITY · ACTUAL TOOL EXECUTIONS" }
                ListView {
                    id: activityList; Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 5
                    model: panel.investigator.events; onCountChanged: positionViewAtEnd(); ScrollBar.vertical: ScrollBar {}
                    delegate: PlainLabel {
                        required property var modelData
                        width: activityList.width - 12; wrapMode: Text.WordWrap; font.pixelSize: 11
                        text: modelData.sequence + "  " + modelData.type + (modelData.tool_name ? " · " + modelData.tool_name : "") + (modelData.run_id ? "\nRun " + panel.shortId(modelData.run_id) : "") + (modelData.error ? "\n" + modelData.error.message : "")
                        color: modelData.type === "tool_failed" || modelData.type === "agent_failed" ? "#f5aa9a" : "#b1c7d3"
                    }
                    PlainLabel { anchors.centerIn: parent; width: parent.width - 20; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap; visible: activityList.count === 0; text: "No tool executions yet."; color: "#8ca5b4" }
                }
            }
        }
        RowLayout {
            spacing: 14
            ColumnLayout {
                Layout.preferredWidth: Math.min(315, panel.width * 0.30)
                Layout.maximumWidth: Math.min(315, panel.width * 0.30)
                Layout.fillHeight: true; spacing: 4
                SectionLabel { text: "STORED RUNS · " + panel.history.runs.length }
                ListView {
                    id: runList; Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 6
                    model: panel.history.runs; ScrollBar.vertical: ScrollBar {}
                    delegate: Rectangle {
                        required property var modelData
                        width: runList.width - 12; height: runContent.implicitHeight + 16; color: "#1d3241"; radius: 4
                        ColumnLayout {
                            id: runContent; anchors.fill: parent; anchors.margins: 8; spacing: 3
                            PlainLabel { Layout.fillWidth: true; text: (modelData.candidate_id || "baseline") + " · " + panel.shortId(modelData.run_id); font.bold: true; elide: Text.ElideRight }
                            PlainLabel { Layout.fillWidth: true; text: panel.validationText(modelData); wrapMode: Text.WordWrap; font.pixelSize: 10; color: modelData.validation && modelData.validation.feasible ? "#a8d9df" : "#f5aa9a" }
                            PlainLabel { text: "Cost " + panel.number(modelData.cost) + " · " + panel.number(modelData.horizon_h) + " h"; font.pixelSize: 11 }
                            RowLayout {
                                Layout.fillWidth: true; spacing: 6
                                Button { Layout.fillWidth: true; text: "Baseline"; checkable: true; checked: panel.history.baselineRunId === modelData.run_id; onClicked: panel.history.selectRuns(modelData.run_id, panel.history.candidateRunId) }
                                Button { Layout.fillWidth: true; text: "Candidate"; checkable: true; checked: panel.history.candidateRunId === modelData.run_id; onClicked: panel.history.selectRuns(panel.history.baselineRunId, modelData.run_id) }
                            }
                        }
                    }
                    PlainLabel { anchors.centerIn: parent; width: parent.width - 18; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap; visible: runList.count === 0; text: "Completed numerical runs appear here."; color: "#8ca5b4" }
                }
            }
            ColumnLayout {
                Layout.fillWidth: true; Layout.fillHeight: true; spacing: 5
                RowLayout {
                    Layout.fillWidth: true
                    PlainLabel { text: "Tank"; color: "#9eb5c2" }
                    ComboBox {
                        id: tankSelector; Layout.preferredWidth: 205
                        model: panel.history.tanks; textRole: "label"; valueRole: "id"; enabled: count > 0
                        currentIndex: {
                            const options = panel.history.tanks
                            for (let i = 0; i < options.length; ++i) if (options[i].id === panel.history.selectedTankId) return i
                            return -1
                        }
                        onActivated: panel.history.selectTank(currentValue)
                        contentItem: PlainLabel { text: tankSelector.displayText; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight; leftPadding: 8; rightPadding: 24 }
                        delegate: ItemDelegate {
                            required property var modelData
                            required property int index
                            width: tankSelector.width; highlighted: tankSelector.highlightedIndex === index
                            contentItem: PlainLabel { text: modelData.label; elide: Text.ElideRight }
                        }
                    }
                    PlainLabel { Layout.fillWidth: true; text: panel.comparisonData.reason || "Select stored runs"; wrapMode: Text.WordWrap; font.pixelSize: 10; color: panel.comparisonData.compatible ? "#9eb5c2" : "#ffcf70" }
                }
                TrajectoryChart { Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumHeight: 85; chart: panel.history.chart }
                RowLayout {
                    Layout.fillWidth: true; spacing: 14
                    PlainLabel { Layout.fillWidth: true; text: "BASELINE · " + panel.shortId(panel.history.baselineRunId) + "\n" + panel.metricText(panel.comparisonData.baseline, panel.comparisonData.baseline_violation); font.pixelSize: 10; wrapMode: Text.WordWrap }
                    PlainLabel { Layout.fillWidth: true; text: "CANDIDATE · " + panel.shortId(panel.history.candidateRunId) + "\n" + panel.metricText(panel.comparisonData.candidate, panel.comparisonData.candidate_violation); font.pixelSize: 10; wrapMode: Text.WordWrap }
                }
                RowLayout {
                    Layout.fillWidth: true
                    PlainLabel { Layout.fillWidth: true; text: panel.comparisonData.compatible ? "Candidate − baseline cost: " + panel.number(panel.comparisonData.cost_delta) + " synthetic units" : "Incompatible runs are not overlaid."; font.pixelSize: 11 }
                    Button { text: "Control changes"; enabled: !!panel.comparisonData.compatible; onClicked: changesDialog.open() }
                }
            }
        }
        RowLayout {
            spacing: 18
            ColumnLayout {
                Layout.fillWidth: true; Layout.fillHeight: true; spacing: 4
                RowLayout {
                    Layout.fillWidth: true
                    SectionLabel { text: "SAVED INVESTIGATIONS · " + panel.history.sessions.length }
                    Item { Layout.fillWidth: true }
                    Button { text: "Refresh"; enabled: !panel.history.busy; onClicked: panel.history.refresh() }
                }
                ListView {
                    id: sessionList; Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 6
                    model: panel.history.sessions; ScrollBar.vertical: ScrollBar {}
                    delegate: ItemDelegate {
                        required property var modelData
                        width: sessionList.width - 12; highlighted: panel.history.selectedSessionId === modelData.session_id; enabled: !panel.history.busy
                        onClicked: panel.history.loadSession(modelData.session_id)
                        contentItem: ColumnLayout {
                            spacing: 3
                            PlainLabel { Layout.fillWidth: true; text: modelData.objective || "Investigation"; elide: Text.ElideRight; font.bold: true }
                            PlainLabel { Layout.fillWidth: true; text: (modelData.status || "unknown") + " · " + (modelData.created_at || "") + "\nSession " + modelData.session_id; font.pixelSize: 10; wrapMode: Text.WrapAnywhere; color: "#9eb5c2" }
                        }
                    }
                    PlainLabel { anchors.centerIn: parent; width: parent.width - 20; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap; visible: sessionList.count === 0; text: "No saved investigations yet. Completed tool evidence is saved as the investigation runs."; color: "#8ca5b4" }
                }
            }
            ColumnLayout {
                Layout.preferredWidth: Math.min(405, panel.width * 0.39); Layout.fillHeight: true; spacing: 8
                SectionLabel { text: "SELECTED EVIDENCE" }
                PlainLabel { Layout.fillWidth: true; text: panel.history.selectedSessionId || "No session selected"; wrapMode: Text.WrapAnywhere; font.pixelSize: 11 }
                PlainLabel { Layout.fillWidth: true; text: panel.history.historical ? "Historical and imported sessions are read-only evidence. They cannot apply controls to the current network." : "Current investigation. Select stored runs in Comparison to inspect outcomes."; wrapMode: Text.WordWrap; color: panel.history.historical ? "#ffcf70" : "#9eb5c2" }
                PlainLabel { text: panel.history.runs.length + " stored numerical runs" }
                RowLayout {
                    Button { text: "Compare runs"; enabled: panel.history.runs.length > 0; onClicked: tabs.currentIndex = 1 }
                    Button { text: "Export JSON"; enabled: !panel.history.busy && panel.history.selectedSessionId.length > 0; onClicked: exportDialog.open() }
                    Button { text: "Import JSON"; enabled: !panel.history.busy; onClicked: importDialog.open() }
                }
                PlainLabel { Layout.fillWidth: true; text: "Versioned session JSON includes captured state, stored runs, validation evidence and execution events."; wrapMode: Text.WordWrap; font.pixelSize: 10; color: "#8ca5b4" }
                Item { Layout.fillHeight: true }
            }
        }
        StorageSciencePanel { scientificWorker: panel.scientificWorker; storageScience: panel.storageScience }
    }
    FileDialog { id: exportDialog; title: "Export investigation session"; fileMode: FileDialog.SaveFile; defaultSuffix: "json"; nameFilters: ["BREAKWATER session (*.json)"]; onAccepted: panel.history.exportSession(selectedFile) }
    FileDialog { id: importDialog; title: "Import historical investigation"; nameFilters: ["BREAKWATER session (*.json)"]; onAccepted: panel.history.importSession(selectedFile) }
    Dialog {
        id: reviewDialog; objectName: "reviewPlanDialog"; parent: Overlay.overlay
        x: Math.round((parent.width - width) / 2); y: Math.round((parent.height - height) / 2)
        width: Math.min(760, parent.width - 48); height: Math.min(610, parent.height - 60)
        modal: true; title: "Review staged simulated plan"; closePolicy: Popup.CloseOnEscape
        contentItem: ScrollView {
            id: reviewScroll; contentWidth: availableWidth
            ColumnLayout {
                width: reviewScroll.availableWidth; spacing: 12
                PlainLabel { Layout.fillWidth: true; text: panel.reviewData.message || "No validated plan is staged."; wrapMode: Text.WordWrap; color: panel.reviewData.eligible ? "#a8d9df" : "#ffcf70" }
                PlainLabel { Layout.fillWidth: true; text: "Run: " + (panel.reviewData.run_id || "none") + "\nValidation: " + (panel.reviewData.validation_id || "none"); wrapMode: Text.WrapAnywhere; font.pixelSize: 11 }
                PlainLabel { Layout.fillWidth: true; text: "Forecast horizon: " + panel.number(panel.reviewData.horizon_h) + " h\nVerified cost: " + panel.number(panel.reviewData.cost) + " synthetic units\nMinimum storage headroom: " + panel.number(panel.reviewData.minimum_headroom_m3) + " m³"; wrapMode: Text.WordWrap }
                SectionLabel { text: "CONTROL SETTINGS · CURRENT → PROPOSED" }
                Repeater {
                    model: panel.reviewData.controls || []
                    delegate: RowLayout {
                        required property var modelData
                        Layout.fillWidth: true; spacing: 12
                        PlainLabel { Layout.fillWidth: true; text: modelData.control_id; wrapMode: Text.WrapAnywhere }
                        PlainLabel { text: panel.number(modelData.before) + " → " + panel.number(modelData.after); font.bold: true }
                        PlainLabel { Layout.preferredWidth: 55; text: panel.unit(modelData.unit); color: "#9eb5c2" }
                    }
                }
                SectionLabel { text: "MODEL LIMITATIONS" }
                Repeater { model: panel.reviewData.warnings || []; delegate: PlainLabel { required property var modelData; Layout.fillWidth: true; text: String(modelData); wrapMode: Text.WordWrap; color: "#aebfca" } }
                PlainLabel { Layout.fillWidth: true; text: "Applying changes only the simulated operating controls. Numerical or topology edits can make this plan ineligible while this dialog is open."; wrapMode: Text.WordWrap; color: "#9eb5c2" }
            }
        }
        footer: DialogButtonBox {
            Button { text: "Cancel"; DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
            Button { objectName: "applySimulatedControls"; text: "Apply simulated controls"; highlighted: true; enabled: panel.investigator.canApprove && !!panel.reviewData.eligible; DialogButtonBox.buttonRole: DialogButtonBox.ActionRole; onClicked: { if (panel.investigator.approvePlan()) reviewDialog.close() } }
            onRejected: reviewDialog.close()
        }
    }
    Dialog {
        id: changesDialog; parent: Overlay.overlay
        x: Math.round((parent.width - width) / 2); y: Math.round((parent.height - height) / 2)
        width: Math.min(720, parent.width - 48); height: Math.min(430, parent.height - 80)
        modal: true; title: "Baseline → candidate control changes"; standardButtons: Dialog.Close
        contentItem: ScrollView {
            id: changesScroll; contentWidth: availableWidth
            ColumnLayout {
                width: changesScroll.availableWidth; spacing: 12
                PlainLabel { Layout.fillWidth: true; text: panel.comparisonData.reason || ""; wrapMode: Text.WordWrap; color: "#9eb5c2" }
                Repeater {
                    model: panel.comparisonData.control_changes || []
                    delegate: RowLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        PlainLabel { Layout.fillWidth: true; text: modelData.control_id; wrapMode: Text.WrapAnywhere }
                        PlainLabel { text: panel.number(modelData.before) + " → " + panel.number(modelData.after) + " " + panel.unit(modelData.unit) }
                    }
                }
                PlainLabel { visible: !(panel.comparisonData.control_changes || []).length; text: "No control settings differ between these runs."; color: "#9eb5c2" }
            }
        }
    }
}

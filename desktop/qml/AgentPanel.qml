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
    readonly property bool advisoryOnly: investigator.advisoryOnly === true
    readonly property var observedSource: advisoryOnly ? investigator.sourceSummary : ({})
    readonly property var nextObservedSource: advisoryOnly ? investigator.nextSourceSummary : ({})
    readonly property bool nextSourceDiffers: advisoryOnly && investigator.nextSourceDiffers === true
    readonly property bool scienceVisible: tabs.currentIndex === 3
    readonly property var reviewData: investigator.review || ({})
    signal openPipeEvidence(var evidence)
    readonly property bool dialogOpen: reviewDialog.visible || comparisonPanel.dialogOpen
    signal configureKeyRequested(Item source)
    signal navigationRequested(int tabIndex)
    readonly property bool advisoryReviewVisible: advisoryOnly && reviewDialog.visible
    property bool embedded: false
    property string currentNetworkHash: ""
    function applyHistoryFilter() { history.setFilter(matchNetwork.checked ? currentNetworkHash : "", sessionQuery.text) }
    onCurrentNetworkHashChanged: Qt.callLater(applyHistoryFilter)
    spacing: 8
    Frame {
        visible: panel.advisoryOnly; Layout.fillWidth: true; padding: 9
        background: Rectangle { color: Theme.raised; radius: Theme.cornerRadius; border.color: Theme.divider }
        contentItem: ColumnLayout {
          spacing: 6
          PlainLabel {
            Layout.fillWidth: true
            objectName: panel.advisoryOnly ? "observedInvestigationOrigin":""; wrapMode: Text.WrapAnywhere; color: Theme.secondary; font.pixelSize: Theme.captionSize
            text: "OBSERVED ADVISORY · " + (panel.observedSource.status || "No capture")
                + (panel.observedSource.snapshot_id ? " · as of " + panel.number(panel.observedSource.as_of_s) + " s · expires " + panel.number(panel.observedSource.expires_s) + " s\nCapture " + panel.observedSource.snapshot_id : "")
                + (panel.nextSourceDiffers ? "\nDisplayed investigation evidence. It will not be replaced until a new investigation starts."
                    : "\nMeasurements stay separate from commands. This investigation cannot apply controls.")
          }
          PlainLabel {
            visible: panel.nextSourceDiffers; Layout.fillWidth: true
            objectName: panel.advisoryOnly ? "observedInvestigationNextSource" : ""; wrapMode: Text.WrapAnywhere; font.pixelSize: Theme.captionSize
            text: "NEXT INVESTIGATION · " + (panel.nextObservedSource.status || "No capture")
                + " · as of " + panel.number(panel.nextObservedSource.as_of_s) + " s · expires " + panel.number(panel.nextObservedSource.expires_s) + " s"
                + "\nCapture " + (panel.nextObservedSource.snapshot_id || "none") + " · advisory only; no controls can be applied."
          }
        }
    }
    function showTab(index) { tabs.currentIndex = index; if (index === 2) history.refresh() }
    function showScience() { tabs.currentIndex = 3 }
    function navigateTo(index) {
        if (index === 0) history.showLive()
        showTab(index)
        if (embedded) navigationRequested(index)
    }
    function number(value, digits) {
        return value !== undefined && value !== null && Number.isFinite(Number(value))
                ? Number(value).toFixed(digits === undefined ? 2 : digits) : "—"
    }
    function shortId(value) { return value ? String(value).substring(0, 8) : "none" }
    function modelNarrative(recommendation) {
        if (!recommendation) return ""
        return recommendation.model_explanation || (recommendation.explanation_source === "retained_evidence" ? "" : recommendation.explanation || "")
    }
    function unit(value) { return value === "m3/h" ? "m³/h" : (value || "") }
    component PlainLabel: Label { textFormat: Text.PlainText; color: Theme.text; font.pixelSize: Theme.bodySize }
    component SectionLabel: PlainLabel { color: Theme.secondary; font.pixelSize: Theme.captionSize; font.letterSpacing: 1 }

    Connections {
        target: panel.history
        function onChanged() {
            if (panel.history.historical && tabs.currentIndex === 0) tabs.currentIndex = 1
        }
    }

    RowLayout {
        visible: !panel.embedded || tabs.currentIndex === 0
        Layout.fillWidth: true; spacing: 12
        ColumnLayout {
            Layout.fillWidth: true; spacing: 3
            SectionLabel { text: panel.investigator.localOnly ? "LOCAL NATIVE SEARCH" : "NEMOTRON INVESTIGATOR"; font.bold: true }
            PlainLabel {
                Layout.fillWidth: true; elide: Text.ElideRight; font.pixelSize: Theme.captionSize
                text: panel.credentials.status
                color: panel.credentials.configured ? Theme.secondary : Theme.warning
            }
        }
        ActionButton {
            iconName: "key-round"
            id: configureKeyButton; objectName: "configureNemotronKey"
            text: panel.credentials.configured ? "API key settings" : "Set API key"
            enabled: !panel.investigator.running
            onClicked: panel.configureKeyRequested(configureKeyButton)
        }
    }

    GridLayout {
        visible: !panel.embedded || tabs.currentIndex === 0
        Layout.fillWidth: true; columns: panel.width < 1100 ? 1 : 2; columnSpacing: 12; rowSpacing: 8
        TextField {
            id: objective; objectName: panel.advisoryOnly ? "observedInvestigationObjective":"investigationObjective"; Layout.fillWidth: true; Layout.minimumWidth: 0
            text: panel.advisoryOnly ? "Inspect these frozen observations and incident evidence. Find a nominally validated advisory response at low modeled cost, or explain missing coverage."
                : "Our main transfer pump failed. Find a validated response for the next three hours at low modeled cost."
            placeholderText: "Operator objective"; maximumLength: 4000; enabled: !panel.investigator.running
            Accessible.name: "Operator objective"
            onAccepted: { if (investigateButton.enabled) investigateButton.clicked() }
        }
        RowLayout {
            Layout.fillWidth: panel.width < 1100; spacing: 8
            ActionButton { iconName: "git-compare-arrows"; objectName: panel.advisoryOnly ? "observedLocalSearch":"localNativeSearch"; text: "Local search"; tooltip: "Real native finite-grid search; no API key or model request. Up to four available controls, flow first, at most six values each."
                enabled: panel.investigator.localSearchAvailable
                onClicked: { panel.history.showLive(); tabs.currentIndex = 0; panel.investigator.searchLocally() }
            }
            ActionButton { iconName: "workflow";
                id: investigateButton; text: panel.nextSourceDiffers ? "Investigate latest capture" : "Investigate"; highlighted: true
                tooltip: panel.advisoryOnly ? "Uses the latest captured measurements. Retained investigation evidence stays read only." : "Investigate the current immutable network."
                enabled: !panel.investigator.running && objective.text.trim().length > 0 && (!panel.advisoryOnly || panel.investigator.localSearchAvailable)
                onClicked: { panel.history.showLive(); tabs.currentIndex = 0; panel.investigator.investigate(objective.text) }
            }
            ActionButton { iconName: "square"; text: "Cancel"; enabled: panel.investigator.running; onClicked: panel.investigator.cancel() }
            ActionButton { iconName: "clipboard-check"; objectName: panel.advisoryOnly ? "reviewObservedAdvisory":"reviewStagedPlan"; text: "Review staged plan"; enabled: !!panel.reviewData.run_id || panel.investigator.canApprove; onClicked: reviewDialog.open() }
            Item { Layout.fillWidth: true; visible: panel.width < 1100 }
        }
    }
    Frame {
        visible: tabs.currentIndex !== 1
        Layout.fillWidth: true; padding: 7
        background: Rectangle { color: Theme.raised; radius: Theme.cornerRadius; border.color: Theme.divider }
        contentItem: PlainLabel {
            elide: Text.ElideRight; color: Theme.secondary; font.pixelSize: Theme.smallSize
            text: tabs.currentIndex === 2 || (tabs.currentIndex === 3 && panel.history.historical) ? panel.history.status : tabs.currentIndex === 3 ? panel.storageScience.status : panel.investigator.status
        }
    }
    RowLayout {
        visible: !panel.embedded || tabs.currentIndex !== 1
        Layout.fillWidth: true; spacing: 12
        TabBar {
            id: tabs; objectName: panel.advisoryOnly ? "observedAgentTabs":"agentTabs"; visible: !panel.embedded; Layout.preferredWidth: 510; Layout.maximumWidth: panel.width * 0.6
            spacing: 4
            TabButton { text: "Investigation"; onClicked: panel.history.showLive() }
            TabButton { text: "Comparison" }
            TabButton { text: "History"; onClicked: panel.history.refresh() }
            TabButton { text: "Science"; enabled: !panel.advisoryOnly; visible: !panel.advisoryOnly }
        }
        Item { Layout.fillWidth: true }
        PlainLabel {
            text: panel.scienceVisible && panel.storageScience.sourceMode === "candidate"
                ? "STORED CANDIDATE · READ ONLY"
                : panel.history.historical ? "HISTORICAL · READ ONLY"
                : panel.scienceVisible ? "CURRENT NATIVE FORECAST · READ ONLY" : panel.advisoryOnly ? "OBSERVED ADVISORY" : "CURRENT SESSION"
            color: panel.history.historical ? Theme.warning : Theme.secondary
            font.pixelSize: Theme.captionSize
        }
        ActionButton { iconName: "rotate-ccw"; objectName: "returnToCurrentEvidence"; visible: panel.history.historical; text: "Return to current"; onClicked: panel.history.showLive() }
    }
    StackLayout {
        Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumHeight: 0
        currentIndex: tabs.currentIndex
        RowLayout {
            spacing: 12
            Frame {
                Layout.fillWidth: true; Layout.preferredWidth: panel.width * 0.58; Layout.fillHeight: true; padding: 10
                background: Rectangle { color: Theme.raised; radius: Theme.cornerRadius; border.color: Theme.divider }
                contentItem: ColumnLayout {
                    spacing: 8
                    SectionLabel { text: "RECOMMENDATION / STORED EVIDENCE" }
                    ScrollView {
                        Layout.fillWidth: true; Layout.fillHeight: true; contentWidth: availableWidth
                        TextArea {
                            readOnly: true; selectByMouse: true; wrapMode: TextEdit.Wrap; textFormat: TextEdit.PlainText; font.pixelSize: Theme.smallSize
                            padding: 0; color: Theme.text; background: null
                            text: {
                                const r = panel.investigator.recommendation
                                if (!r || !r.status) return (panel.advisoryOnly
                                    ? "Investigate the frozen observations explicitly. Missing coverage yields no numerical plan. Review records advisory acknowledgement only; no controls can be applied.\n\n"
                                    : "Start an investigation on the current network. Numerical tools execute the C++ engine; recommendations require stored validation evidence.\n\nSimulated controls change only after you review and apply a validated plan.\n\n") + panel.investigator.storageStatus
                                let summary = (r.explanation_source === "retained_evidence" ? r.explanation
                                    : "Legacy recommendation. Inspect the retained run and validation evidence. Its model narrative is unverified.")
                                    + "\n\nRun: " + (r.selected_run_id || "none")
                                if (r.ok) summary += "\nCost: " + panel.number(r.cost) + " synthetic units · Horizon: " + panel.number(r.horizon_h) + " h\nValidation: " + (r.validation_id || "none")
                                return summary + "\nApproval: " + (r.approval_status || "unavailable") + "\n\nSynthetic model. Quality is not evaluated.\n" + panel.investigator.storageStatus
                            }
                        }
                    }
                    CheckBox {
                        id: showModelNarrative; objectName: "showUnverifiedModelExplanation"
                        Layout.fillWidth: true
                        visible: panel.modelNarrative(panel.investigator.recommendation).length > 0
                        text: "Show model explanation · unverified narrative"
                    }
                    ScrollView {
                        Layout.fillWidth: true; Layout.preferredHeight: 90
                        visible: showModelNarrative.visible && showModelNarrative.checked
                        contentWidth: availableWidth; clip: true
                        TextArea {
                            objectName: "unverifiedModelExplanation"
                            readOnly: true; selectByMouse: true; wrapMode: TextEdit.Wrap; textFormat: TextEdit.PlainText
                            text: panel.modelNarrative(panel.investigator.recommendation)
                            font.pixelSize: Theme.smallSize; color: Theme.secondary; background: null
                        }
                    }
                }
            }
            Frame {
                Layout.fillWidth: true; Layout.preferredWidth: panel.width * 0.42; Layout.fillHeight: true; padding: 10
                background: Rectangle { color: Theme.raised; radius: Theme.cornerRadius; border.color: Theme.divider }
                contentItem: ColumnLayout {
                    spacing: 8
                    SectionLabel { text: "ACTIVITY · ACTUAL TOOL EXECUTIONS" }
                    ListView {
                        id: activityList; Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 6
                        model: panel.investigator.events; onCountChanged: positionViewAtEnd(); ScrollBar.vertical: ScrollBar {}
                        delegate: Frame {
                            required property var modelData
                            width: activityList.width - 12; padding: 8
                            background: Rectangle { color: Theme.surface; radius: Theme.cornerRadius; border.color: Theme.divider }
                            contentItem: PlainLabel {
                                wrapMode: Text.WordWrap; font.pixelSize: Theme.captionSize
                                text: modelData.sequence + "  " + modelData.type + (modelData.tool_name ? " · " + modelData.tool_name : "") + (modelData.run_id ? "\nRun " + panel.shortId(modelData.run_id) : "") + (modelData.error ? "\n" + modelData.error.message : "")
                                color: modelData.type === "tool_failed" || modelData.type === "agent_failed" ? Theme.error : Theme.secondary
                            }
                        }
                        PlainLabel { anchors.left: parent.left; anchors.top: parent.top; width: parent.width - 12; wrapMode: Text.WordWrap; visible: activityList.count === 0; text: "No tool executions yet."; color: Theme.muted; font.pixelSize: Theme.smallSize }
                    }
                }
            }
        }
        RunComparisonPanel {
            id: comparisonPanel
            history: panel.history
            onInvestigationRequested: panel.navigateTo(0)
            onHistoryRequested: panel.navigateTo(2)
        }
        RowLayout {
            spacing: 18
            ColumnLayout {
                Layout.fillWidth: true; Layout.fillHeight: true; spacing: 4
                RowLayout {
                    Layout.fillWidth: true
                    SectionLabel { text: "SAVED SESSIONS · " + panel.history.sessions.length }
                    Item { Layout.fillWidth: true }
                    ActionButton { iconName: "history"; text: "Refresh"; enabled: !panel.history.busy; onClicked: panel.applyHistoryFilter() }
                }
                RowLayout {
                    Layout.fillWidth: true
                    TextField { id: sessionQuery; Layout.fillWidth: true; placeholderText: "Search objective, status or session"; maximumLength: 256; onEditingFinished: panel.applyHistoryFilter() }
                    CheckBox { id: matchNetwork; text: "Current network"; onToggled: panel.applyHistoryFilter() }
                }
                PlainLabel { text: "Most recent 500 matching sessions · stored evidence only"; font.pixelSize: Theme.captionSize; color: Theme.muted }
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
                            PlainLabel { Layout.fillWidth: true; text: (modelData.status || "unknown") + " · " + (modelData.created_at || "") + "\nSession " + modelData.session_id; font.pixelSize: Theme.captionSize; wrapMode: Text.WrapAnywhere; color: Theme.secondary }
                        }
                    }
                    PlainLabel { anchors.centerIn: parent; width: parent.width - 20; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap; visible: sessionList.count === 0; text: "No saved sessions yet. Investigation evidence and completed scientific comparisons are saved automatically."; color: Theme.muted }
                }
            }
            ScrollView {
                id: selectedEvidenceScroll; objectName: "selectedEvidenceScroll"
                Layout.preferredWidth: Math.min(405, panel.width * 0.39); Layout.fillHeight: true
                contentWidth: availableWidth; clip: true
                ScrollBar.vertical.policy: ScrollBar.AsNeeded
                ColumnLayout {
                width: selectedEvidenceScroll.availableWidth; spacing: 8
                SectionLabel { text: "SELECTED EVIDENCE" }
                PlainLabel { Layout.fillWidth: true; text: panel.history.selectedSessionId || "No session selected"; wrapMode: Text.WrapAnywhere; font.pixelSize: Theme.captionSize }
                PlainLabel { Layout.fillWidth: true; text: panel.history.historical ? "Historical and imported sessions are read-only evidence. They cannot apply controls to the current network." : "Current investigation. Select stored runs in Comparison to inspect outcomes."; wrapMode: Text.WordWrap; color: panel.history.historical ? Theme.warning : Theme.secondary }
                PlainLabel { text: panel.history.runs.length + " stored numerical runs" }
                PlainLabel { text: panel.history.scientificEvidence.length + " stored scientific results" }
                Repeater {
                    model: panel.history.retainedEvidence || []
                    ColumnLayout {
                        required property var modelData
                        Layout.fillWidth: true; spacing: 8
                        PlainLabel { Layout.fillWidth: true; text: "RETAINED · " + modelData.kind.replace(/_/g," "); wrapMode: Text.Wrap; color: Theme.secondary }
                        PlainLabel {
                            Layout.fillWidth: true; wrapMode: Text.Wrap; font.pixelSize: Theme.captionSize; color: Theme.muted
                            text: modelData.kind === "pipe_transport"
                                ? modelData.payload.result.route_id + " · " + modelData.payload.result.method_version + "\nSource run " + modelData.payload.result.run_id
                                : modelData.kind === "observed_origin"
                                    ? "OBSERVED · as of " + Number(modelData.payload.snapshot.captured_event_time_s).toFixed(2) + " s · expires " + Number(modelData.payload.snapshot.expires_event_time_s).toFixed(2) + " s"
                                    : modelData.kind.indexOf("observation_") === 0
                                        ? (modelData.payload.events || []).length + " event records · numerical witness retained · controls unavailable"
                                        : "Immutable selected-candidate source retained with this scientific result."
                        }
                        ActionButton {
                            objectName: "viewRetainedPipeHistory"; visible: modelData.kind === "pipe_transport"; text: "View retained pipe result"; iconName: "droplets"
                            onClicked: panel.openPipeEvidence(modelData.payload.result)
                        }
                    }
                }
                ActionButton { iconName: "flask-conical"; objectName: "viewHistoricalScience"; text: "View scientific evidence"; visible: !panel.advisoryOnly; enabled: panel.history.historical && panel.history.scientificEvidence.length > 0; onClicked: { panel.storageScience.setSourceMode("forecast"); tabs.currentIndex = 3 } }
                Flow {
                    Layout.fillWidth: true; spacing: 8
                    ActionButton { iconName: "git-compare-arrows"; text: "Compare runs"; enabled: panel.history.runs.length > 0; onClicked: panel.navigateTo(1) }
                    ActionButton { iconName: "arrow-up-from-line"; text: "Export JSON"; enabled: !panel.history.busy && panel.history.selectedSessionId.length > 0; onClicked: exportDialog.open() }
                    ActionButton { iconName: "arrow-down-to-line"; text: "Import JSON"; enabled: !panel.history.busy; onClicked: importDialog.open() }
                }
                PlainLabel { Layout.fillWidth: true; text: "Versioned session JSON includes captured state, stored runs, validation evidence, execution events and any retained scientific results."; wrapMode: Text.WordWrap; font.pixelSize: Theme.captionSize; color: Theme.muted }
                Item { Layout.fillHeight: true }
                }
            }
        }
        Loader {
            active: !panel.advisoryOnly
            sourceComponent: StorageSciencePanel { scientificWorker: panel.scientificWorker; storageScience: panel.storageScience; history: panel.history }
        }
    }
    FileDialog { id: exportDialog; title: "Export investigation session"; fileMode: FileDialog.SaveFile; defaultSuffix: "json"; nameFilters: ["BREAKWATER session (*.json)"]; onAccepted: panel.history.exportSession(selectedFile) }
    FileDialog { id: importDialog; title: "Import historical investigation"; nameFilters: ["BREAKWATER session (*.json)"]; onAccepted: panel.history.importSession(selectedFile) }
    Dialog {
        id: reviewDialog; objectName: panel.advisoryOnly ? "reviewObservedAdvisoryDialog":"reviewPlanDialog"; parent: Overlay.overlay
        x: Math.round((parent.width - width) / 2); y: Math.round((parent.height - height) / 2)
        width: Math.min(760, parent.width - 48); height: Math.min(610, parent.height - 60)
        modal: true; title: panel.advisoryOnly ? "Review observed advisory response":"Review staged simulated plan"; closePolicy: Popup.CloseOnEscape
        header: DialogHeading { owner: reviewDialog }
        contentItem: ScrollView {
            id: reviewScroll; contentWidth: availableWidth
            ColumnLayout {
                width: reviewScroll.availableWidth; spacing: 12
                PlainLabel { Layout.fillWidth: true; text: panel.reviewData.message || "No validated plan is staged."; wrapMode: Text.WordWrap; color: panel.reviewData.eligible ? Theme.text : Theme.warning }
                PlainLabel { Layout.fillWidth: true; text: "Run: " + (panel.reviewData.run_id || "none") + "\nValidation: " + (panel.reviewData.validation_id || "none"); wrapMode: Text.WrapAnywhere; font.pixelSize: Theme.captionSize }
                PlainLabel { Layout.fillWidth: true; text: "Forecast horizon: " + panel.number(panel.reviewData.horizon_h) + " h\nVerified cost: " + panel.number(panel.reviewData.cost) + " synthetic units\nMinimum storage headroom: " + panel.number(panel.reviewData.minimum_headroom_m3) + " m³"; wrapMode: Text.WordWrap }
                PlainLabel {
                    visible: panel.advisoryOnly; Layout.fillWidth: true; wrapMode: Text.WrapAnywhere; font.pixelSize: Theme.captionSize; color: Theme.secondary
                    text: "Observed origin: " + (panel.reviewData.snapshot_id || "none") + " · " + (panel.reviewData.status || "unavailable")
                        + "\nAs of " + panel.number(panel.reviewData.as_of_s) + " s · expires " + panel.number(panel.reviewData.expires_s) + " s"
                        + "\nDesign hash: " + (panel.reviewData.design_physical_hash || "none") + "\nEffective hash: " + (panel.reviewData.effective_physical_hash || "none")
                }
                SectionLabel { text: panel.advisoryOnly ? "CONTROL SETTINGS · FROZEN COMMAND → PROPOSED":"CONTROL SETTINGS · CURRENT → PROPOSED" }
                Repeater {
                    model: panel.reviewData.controls || []
                    delegate: RowLayout {
                        required property var modelData
                        Layout.fillWidth: true; spacing: 12
                        PlainLabel { Layout.fillWidth: true; text: modelData.control_id; wrapMode: Text.WrapAnywhere }
                        PlainLabel { text: panel.number(modelData.before) + " → " + panel.number(modelData.after); font.bold: true }
                        PlainLabel { Layout.preferredWidth: 55; text: panel.unit(modelData.unit); color: Theme.secondary }
                    }
                }
                SectionLabel { visible: panel.advisoryOnly; text: "MEASURED STATE · SEPARATE FROM COMMANDS" }
                Repeater {
                    model: panel.advisoryOnly ? panel.reviewData.measurements || []:[]
                    delegate: PlainLabel {
                        required property var modelData
                        Layout.fillWidth: true; wrapMode: Text.WrapAnywhere; font.pixelSize: Theme.captionSize
                        text: modelData.attachment_id + " · " + modelData.quantity + ": " + modelData.value + " " + panel.unit(modelData.unit)
                            + " · absolute bound " + panel.number(modelData.absolute_bound,4)
                    }
                }
                Repeater { model: panel.advisoryOnly ? panel.observedSource.assumptions || []:[]; delegate: PlainLabel { required property var modelData; Layout.fillWidth: true; text: String(modelData); wrapMode: Text.Wrap; color: Theme.secondary } }
                SectionLabel { text: "MODEL LIMITATIONS" }
                Repeater { model: panel.reviewData.warnings || []; delegate: PlainLabel { required property var modelData; Layout.fillWidth: true; text: String(modelData); wrapMode: Text.WordWrap; color: Theme.secondary } }
                PlainLabel { Layout.fillWidth: true; text: panel.advisoryOnly ? "Acknowledgement records human advisory review only. No operating control, publisher command or incident clearing occurs. Expiry, replacement, edits or a mismatched run make review ineligible."
                    : "Applying changes only the simulated operating controls. Numerical or topology edits can make this plan ineligible while this dialog is open."; wrapMode: Text.WordWrap; color: Theme.secondary }
            }
        }
        footer: DialogButtonBox {
            ActionButton { iconName: "square"; text: "Cancel"; DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
            ActionButton { iconName: "check-check"; objectName: panel.advisoryOnly ? "acknowledgeObservedAdvisory":"applySimulatedControls"; text: panel.advisoryOnly ? "Acknowledge advisory review":"Apply simulated controls"; highlighted: true
                enabled: (panel.advisoryOnly ? panel.investigator.canAcknowledge:panel.investigator.canApprove) && !!panel.reviewData.eligible
                DialogButtonBox.buttonRole: DialogButtonBox.ActionRole
                onClicked: { if (panel.advisoryOnly ? panel.investigator.acknowledgePlan():panel.investigator.approvePlan()) reviewDialog.close() }
            }
            onRejected: reviewDialog.close()
        }
    }
}

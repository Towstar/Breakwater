import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Dialog {
    id: panel
    required property var analysis
    required property var eog
    property string exportId: ""
    property var reviewSnapshot: ({})
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(1160, parent ? parent.width - 28 : 1160)
    height: Math.min(820, parent ? parent.height - 28 : 820)
    modal: true; title: "EOG investigation and retained evidence"
    standardButtons: Dialog.Close
    function pretty(value) { return JSON.stringify(value, null, 2) }
    function money(value) { return typeof value === "number" && isFinite(value) ? "$" + value.toFixed(2) : "unavailable" }
    function utc(value) { return typeof value === "number" ? new Date(value * 1000).toISOString() : "unavailable" }
    function planIndex(id) { for (let i = 0; i < analysis.plans.length; ++i) if (analysis.plans[i].plan_id === id) return i; return -1 }
    onOpened: analysis.refreshHistory(0)
    FileDialog {
        id: importDialog; nameFilters: ["EOG evidence (*.eog-session.json)"]
        onAccepted: panel.analysis.importHistory(selectedFile)
    }
    FileDialog {
        id: exportDialog; fileMode: FileDialog.SaveFile; defaultSuffix: "eog-session.json"
        nameFilters: ["EOG evidence (*.eog-session.json)"]
        onAccepted: panel.analysis.exportHistory(panel.exportId, selectedFile)
    }
    Dialog {
        id: editor; parent: Overlay.overlay; anchors.centerIn: parent; modal: true; title: "Evaluate an explicit counterfactual plan"
        width: Math.min(800, parent ? parent.width - 40 : 800); height: Math.min(650, parent ? parent.height - 40 : 650)
        standardButtons: Dialog.Cancel
        contentItem: ColumnLayout {
            Label { Layout.fillWidth: true; text: "Editing a proposal never changes the CSV or sandbox controls. The numerical provider checks all actions against captured operations."; wrapMode: Text.Wrap }
            ScrollView { Layout.fillWidth: true; Layout.fillHeight: true; TextArea { id: planText; font.family: "Consolas"; wrapMode: TextEdit.WrapAnywhere; selectByMouse: true } }
            Button { text: "Evaluate proposal"; enabled: !panel.analysis.busy && !panel.analysis.historical; onClicked: { panel.analysis.evaluatePlan(planText.text); editor.close() } }
        }
    }
    Dialog {
        id: review; parent: Overlay.overlay; anchors.centerIn: parent; modal: true; title: "Record advisory review"
        width: Math.min(800, parent ? parent.width - 40 : 800); height: Math.min(660, parent ? parent.height - 40 : 660)
        standardButtons: Dialog.Cancel
        contentItem: ColumnLayout {
            Label { Layout.fillWidth: true; text: "Review records advice viewed. It performs no truck dispatch, well control, CSV modification or sandbox application."; wrapMode: Text.Wrap; color: "#e5bd72" }
            ScrollView { Layout.fillWidth: true; Layout.fillHeight: true; TextArea { readOnly: true; text: panel.pretty(panel.reviewSnapshot); wrapMode: TextEdit.WrapAnywhere; selectByMouse: true; font.family: "Consolas" } }
            TextField { id: reviewNote; Layout.fillWidth: true; placeholderText: "Optional review note"; maximumLength: 4000 }
            Button {
                text: "Record review of this exact plan"
                enabled: panel.analysis.canReview && panel.analysis.selectedValidation.validation_id === panel.reviewSnapshot.validation_id
                    && panel.analysis.selectedValidation.plan_hash === panel.reviewSnapshot.plan_hash
                onClicked: { panel.analysis.recordReview(reviewNote.text); review.close() }
            }
        }
    }
    contentItem: ColumnLayout {
        spacing: 8
        Label { Layout.fillWidth: true; text: (panel.analysis.historical ? "HISTORICAL · " : "CURRENT CAPTURE · ") + panel.analysis.status; textFormat: Text.PlainText; wrapMode: Text.Wrap; color: "#cde5ef" }
        Label { Layout.fillWidth: true; text: panel.analysis.storageStatus; textFormat: Text.PlainText; wrapMode: Text.Wrap; color: "#c7b484"; font.pixelSize: 12 }
        Label { Layout.fillWidth: true; visible: panel.analysis.evidenceContext.capture_id !== undefined; text: "Capture " + (panel.analysis.evidenceContext.capture_id || "") + " · cutoff " + panel.utc(panel.analysis.evidenceContext.cutoff_unix) + " · " + (panel.analysis.evidenceContext.context || ""); textFormat: Text.PlainText; wrapMode: Text.WrapAnywhere; font.pixelSize: 11; color: "#91afbe" }
        Flow {
            Layout.fillWidth: true; spacing: 6
            Button { text: "Detect events"; enabled: panel.eog.loaded && !panel.analysis.busy && !panel.analysis.historical; onClicked: panel.analysis.detect() }
            Button { text: "Search plans"; enabled: panel.eog.loaded && !panel.analysis.busy && !panel.analysis.historical; onClicked: panel.analysis.searchPlans() }
            Button { text: "Finish session"; enabled: !panel.analysis.busy && !panel.analysis.historical && panel.analysis.sessionId.length > 0; onClicked: panel.analysis.finishSession() }
            Button { text: "Cancel"; enabled: panel.analysis.busy; onClicked: panel.analysis.cancel() }
            Button { text: "Return to live"; enabled: panel.analysis.historical; onClicked: panel.analysis.returnToLive() }
            Button { text: "Import history"; enabled: !panel.analysis.busy; onClicked: importDialog.open() }
        }
        RowLayout {
            Layout.fillWidth: true
            TextField { id: objective; Layout.fillWidth: true; maximumLength: 4000; placeholderText: "Objective for an on-demand Nemotron investigation" }
            Button { text: "Ask Nemotron"; enabled: panel.eog.loaded && !panel.analysis.busy && !panel.analysis.historical && objective.text.trim().length > 0; onClicked: panel.analysis.investigate(objective.text) }
        }
        Label { Layout.fillWidth: true; text: panel.analysis.providerConfigured ? (panel.analysis.providerVerified ? "Local provider exchange verified; numerical qualification is separate." : "Provider configured; execution not yet verified.") : "Numerical provider unavailable. The recorded 3D network and measurements remain usable."; wrapMode: Text.Wrap; color: "#9eb9c8"; font.pixelSize: 12 }
        TabBar {
            id: tabs; Layout.fillWidth: true
            TabButton { text: "Events (" + panel.analysis.events.length + ")" }
            TabButton { text: "Plans (" + panel.analysis.plans.length + ")" }
            TabButton { text: "Trace and explanation" }
            TabButton { text: "History" }
        }
        StackLayout {
            currentIndex: tabs.currentIndex; Layout.fillWidth: true; Layout.fillHeight: true
            ScrollView {
                id: eventScroll; clip: true; contentWidth: availableWidth
                ColumnLayout {
                    width: eventScroll.availableWidth
                    Label { Layout.fillWidth: true; visible: panel.analysis.events.length === 0; text: "No completed detector events retained. An empty list before analysis does not establish a healthy network."; wrapMode: Text.Wrap }
                    Repeater {
                        model: panel.analysis.events
                        delegate: Frame {
                            required property var modelData
                            Layout.fillWidth: true
                            ColumnLayout {
                                anchors.fill: parent
                                Label { Layout.fillWidth: true; text: modelData.category + " · " + modelData.state + " · " + modelData.asset_id; textFormat: Text.PlainText; wrapMode: Text.Wrap; font.bold: true }
                                Label { Layout.fillWidth: true; text: "Onset " + panel.utc(modelData.onset_unix) + " · shutdown countdown: " + (typeof modelData.countdown_hours === "number" ? modelData.countdown_hours.toFixed(3) + " h" : "unavailable") + " · exposure " + panel.money(modelData.lost_revenue_per_hour) + "/h · coverage " + modelData.coverage; textFormat: Text.PlainText; wrapMode: Text.Wrap }
                                RowLayout {
                                    Button { text: "Inspect evidence"; onClicked: evidence.text = panel.pretty(panel.analysis.eventEvidence(modelData.event_id)) }
                                    Button { text: "Acknowledge"; enabled: !panel.analysis.historical && !panel.analysis.busy; onClicked: panel.analysis.acknowledge(modelData.event_id, "Reviewed in EOG console; acknowledgement does not clear the event.") }
                                }
                            }
                        }
                    }
                    TextArea { id: evidence; Layout.fillWidth: true; readOnly: true; selectByMouse: true; wrapMode: TextEdit.WrapAnywhere; font.family: "Consolas" }
                }
            }
            ScrollView {
                id: planScroll; clip: true; contentWidth: availableWidth
                ColumnLayout {
                    width: planScroll.availableWidth
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: "Candidate" }
                        ComboBox { Layout.fillWidth: true; model: panel.analysis.plans; textRole: "plan_id"; currentIndex: panel.planIndex(panel.analysis.selectedPlanId); onActivated: panel.analysis.selectPlan(panel.analysis.plans[currentIndex].plan_id) }
                        Label { text: "Baseline" }
                        ComboBox { Layout.fillWidth: true; model: panel.analysis.plans; textRole: "plan_id"; currentIndex: panel.planIndex(panel.analysis.baselinePlanId); onActivated: panel.analysis.selectBaseline(panel.analysis.plans[currentIndex].plan_id) }
                    }
                    Label {
                        Layout.fillWidth: true; wrapMode: Text.Wrap; textFormat: Text.PlainText
                        text: {
                            const c = panel.analysis.selectedEvaluation, b = panel.analysis.baselineEvaluation
                            const cost = c.cost || {}, base = b.cost || {}
                            return "Candidate: " + (c.outcome || "unavailable") + " · coverage " + (c.coverage || "unavailable")
                                + " · trips " + panel.money(cost.truck_cost) + " · lost revenue " + panel.money(cost.lost_revenue) + " · total " + panel.money(cost.total)
                                + (panel.analysis.comparisonCompatible ? "\nBaseline total " + panel.money(base.total) + " · candidate − baseline " + (typeof cost.total === "number" && typeof base.total === "number" ? panel.money(cost.total - base.total) : "unavailable") : "\nNumerical comparison unavailable until matching horizons and context are selected.")
                        }
                    }
                    EogPlanChart { Layout.fillWidth: true; candidate: panel.analysis.selectedEvaluation; baselineRun: panel.analysis.baselineEvaluation; compatible: panel.analysis.comparisonCompatible }
                    Flow {
                        Layout.fillWidth: true; spacing: 6
                        Button { text: "Evaluate no action"; enabled: !panel.analysis.busy && !panel.analysis.historical && panel.analysis.selectedPlanId.length > 0; onClicked: panel.analysis.evaluateNoAction() }
                        Button { text: "Edit proposal"; enabled: !panel.analysis.busy && !panel.analysis.historical; onClicked: { planText.text = panel.analysis.selectedPlanJson; editor.open() } }
                        Button { text: "Review validated advice"; enabled: panel.analysis.canReview; onClicked: { panel.reviewSnapshot = {plan: JSON.parse(panel.analysis.selectedPlanJson), validation_id: panel.analysis.selectedValidation.validation_id, plan_hash: panel.analysis.selectedValidation.plan_hash, validation: panel.analysis.selectedValidation, cost: panel.analysis.selectedEvaluation.cost}; reviewNote.text = ""; review.open() } }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        TextField { id: evaluator; Layout.fillWidth: true; maximumLength: 256; placeholderText: "Actual provider evaluator method ID (from provider documentation)" }
                        Button { text: "Independently validate"; enabled: !panel.analysis.busy && !panel.analysis.historical && evaluator.text.trim().length > 0 && panel.analysis.selectedPlanId.length > 0; onClicked: panel.analysis.validateSelected(evaluator.text.trim()) }
                    }
                    TextArea { Layout.fillWidth: true; readOnly: true; selectByMouse: true; wrapMode: TextEdit.WrapAnywhere; text: panel.pretty({plan: panel.analysis.selectedEvaluation.plan || {}, fleet: panel.analysis.selectedEvaluation.fleet_occupancy || [], shutdowns: panel.analysis.selectedEvaluation.shutdowns || [], violations: panel.analysis.selectedEvaluation.violations || [], assumptions: panel.analysis.selectedEvaluation.assumptions || [], limitations: panel.analysis.selectedEvaluation.limitations || [], validation: panel.analysis.selectedValidation, search: panel.analysis.searchDetails}); font.family: "Consolas" }
                }
            }
            ScrollView {
                clip: true; contentWidth: availableWidth
                TextArea { readOnly: true; selectByMouse: true; wrapMode: TextEdit.WrapAnywhere; text: panel.analysis.summary + "\n\n" + panel.pretty(panel.analysis.trace) + (panel.analysis.historical ? "\n\nRetained operator records:\n" + panel.pretty(panel.analysis.operatorRecords) : "\n\nOperator records are confirmed in storage status and retained in the session archive."); font.family: "Consolas" }
            }
            ScrollView {
                id: historyScroll; clip: true; contentWidth: availableWidth
                ColumnLayout {
                    width: historyScroll.availableWidth
                    Button { text: "Refresh first page"; onClicked: panel.analysis.refreshHistory(0) }
                    Repeater {
                        model: panel.analysis.sessions
                        delegate: Frame {
                            required property var modelData
                            Layout.fillWidth: true
                            ColumnLayout {
                                anchors.fill: parent
                                Label { Layout.fillWidth: true; text: (modelData.objective || "") + " · " + modelData.status + "\n" + modelData.session_id; textFormat: Text.PlainText; wrapMode: Text.WrapAnywhere }
                                RowLayout {
                                    Button { text: "Open historical evidence"; visible: modelData.session_id.length > 0; enabled: !panel.analysis.busy; onClicked: panel.analysis.openHistory(modelData.session_id) }
                                    Button { text: "Export new archive"; visible: modelData.session_id.length > 0; onClicked: { panel.exportId = modelData.session_id; exportDialog.open() } }
                                    Button { text: "Next page"; visible: modelData.next_offset !== undefined; onClicked: panel.analysis.refreshHistory(modelData.next_offset) }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

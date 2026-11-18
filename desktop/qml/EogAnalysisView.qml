import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Pane {
    id: panel
    required property var analysis
    required property var eog
    property string exportId: ""
    property var reviewSnapshot: ({})
    property string selectedEventId: ""
    property string eventSessionId: ""
    readonly property var selectedEventEvidence: {
        // Reading both properties makes the lookup follow restored/replaced
        // evidence even when a provider reuses its event identifier.
        const rows = analysis.events, session = analysis.sessionId
        for (let i = 0; i < rows.length; ++i)
            if (rows[i].event_id === selectedEventId)
                return analysis.eventEvidence(selectedEventId)
        return ({})
    }
    padding: 12
    clip: true
    property bool showTabs: true
    property alias activeTab: tabs.currentIndex
    function showTab(index) { tabs.currentIndex = index; if (index === 3) analysis.refreshHistory(0) }
    background: Rectangle { color: Theme.surface }
    function pretty(value) { return JSON.stringify(value, null, 2) }
    function money(value) { return typeof value === "number" && isFinite(value) ? "$" + value.toFixed(2) : "unavailable" }
    function utc(value) { return typeof value === "number" ? new Date(value * 1000).toISOString() : "unavailable" }
    readonly property var planChoices: analysis.plans.map((row, index) => ({plan_id: row.plan_id, label: (row.no_action ? "No action" : "Response " + (index + 1)) + " · " + money((row.cost || {}).total) + " · " + (row.outcome || "unavailable")}))
    function planIndex(id) { for (let i = 0; i < analysis.plans.length; ++i) if (analysis.plans[i].plan_id === id) return i; return -1 }
    function traceTitle(row) {
        if (row.type === "model_request") return "Model exchange · " + String(row.model_identity || "identity unavailable")
        if (row.type === "tool_call") return "Tool requested · " + String(row.name || "unnamed tool")
        if (row.type === "tool_result") return (row.ok === true ? "Tool completed · " : row.ok === false ? "Tool failed · " : "Tool result · ") + String(row.name || "unnamed tool")
        if (row.type === "final") return "Final explanation recorded"
        if (row.type === "cancel") return "Investigation cancelled"
        if (row.type === "failure" && String(row.code || "").indexOf("final_format_rejected_") === 0)
            return "Final format correction requested"
        if (row.type === "failure") return "Investigation incomplete"
        return "Recorded event · " + String(row.type || "unavailable")
    }
    function traceDetail(row) {
        if (row.type === "model_request") {
            const elapsed = typeof row.duration_ms === "number" && Number.isFinite(row.duration_ms) ? row.duration_ms + " ms" : "duration unavailable"
            return (typeof row.http_status === "number" ? "HTTP " + row.http_status : "HTTP status unavailable")
                + " · " + elapsed + " · retries " + (typeof row.retry_count === "number" ? row.retry_count : "unavailable")
        }
        if (row.type === "tool_call") return "Call " + String(row.call_id || "unavailable") + " · arguments retained in exact trace"
        if (row.type === "tool_result") return "Call " + String(row.call_id || "unavailable")
            + (row.analysis_request_id ? "\nNumerical evidence request " + row.analysis_request_id : "")
            + (row.summary ? "\n" + row.summary : "")
        if (row.type === "final") return (row.selected_plan_id ? "Selected plan " + row.selected_plan_id : "No plan selected")
            + (row.validation_id ? "\nIndependent validation " + row.validation_id : "\nNo validation selected")
            + "\nReferenced events: " + (row.event_ids || []).length
        return row.code ? "Code: " + row.code : "Exact retained data is available below."
    }
    function traceIcon(row) {
        return row.type === "failure" || row.type === "tool_result" && row.ok === false ? "triangle-alert"
            : row.type === "cancel" ? "square" : row.type === "final" ? "clipboard-check"
            : row.type === "model_request" ? "workflow" : "activity"
    }
    onVisibleChanged: { if (visible && analysis) analysis.refreshHistory(0) }
    Connections {
        target: panel.analysis
        function onChanged() {
            if (panel.eventSessionId !== panel.analysis.sessionId) {
                panel.eventSessionId = panel.analysis.sessionId
                panel.selectedEventId = ""
                panel.reviewSnapshot = ({})
                review.close()
                editor.close()
            }
        }
    }
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
        header: DialogHeading { owner: editor; closeName: "eogPlanEditorClose" }
        footer: DialogFooter { owner: editor; label: "Cancel" }
        width: Math.min(800, parent ? parent.width - 40 : 800); height: Math.min(650, parent ? parent.height - 40 : 650)
        standardButtons: Dialog.Cancel
        contentItem: ColumnLayout {
            Label { Layout.fillWidth: true; text: "Editing a proposal never changes the CSV or sandbox controls. The numerical provider checks all actions against captured operations."; wrapMode: Text.Wrap }
            ScrollView { Layout.fillWidth: true; Layout.fillHeight: true; TextArea { id: planText; font.family: Theme.monoFamily; wrapMode: TextEdit.WrapAnywhere; selectByMouse: true } }
            ActionButton { iconName: "play"; text: "Evaluate proposal"; enabled: !panel.analysis.busy && !panel.analysis.historical; onClicked: { panel.analysis.evaluatePlan(planText.text); editor.close() } }
        }
    }
    Dialog {
        id: review; parent: Overlay.overlay; anchors.centerIn: parent; modal: true; title: "Record advisory review"
        header: DialogHeading { owner: review; closeName: "eogReviewClose" }
        footer: DialogFooter { owner: review; label: "Cancel" }
        width: Math.min(800, parent ? parent.width - 40 : 800); height: Math.min(660, parent ? parent.height - 40 : 660)
        standardButtons: Dialog.Cancel
        contentItem: ColumnLayout {
            Label { Layout.fillWidth: true; text: "Review records advice viewed. It performs no truck dispatch, well control, CSV modification or sandbox application."; wrapMode: Text.Wrap; color: Theme.warning }
            ScrollView {
                id: reviewScroll; Layout.fillWidth: true; Layout.fillHeight: true; contentWidth: availableWidth
                EogPlanDetails { width: reviewScroll.availableWidth; evaluation: panel.reviewSnapshot.evaluation || ({}); validation: panel.reviewSnapshot.validation || ({}) }
            }
            TextField { id: reviewNote; Layout.fillWidth: true; placeholderText: "Optional review note"; maximumLength: 4000 }
            ActionButton { iconName: "clipboard-check"; highlighted: true;
                text: "Record review of this exact plan"
                enabled: panel.analysis.canReview && panel.analysis.selectedValidation.validation_id === panel.reviewSnapshot.validation_id
                    && panel.analysis.selectedValidation.plan_hash === panel.reviewSnapshot.plan_hash
                onClicked: { panel.analysis.recordReview(reviewNote.text); review.close() }
            }
        }
    }
    contentItem: ColumnLayout {
        spacing: 8
        Label { Layout.fillWidth: true; text: (panel.analysis.historical ? "HISTORICAL · " : "CURRENT CAPTURE · ") + panel.analysis.status; textFormat: Text.PlainText; wrapMode: Text.Wrap; color: Theme.text }
        Label { Layout.fillWidth: true; visible: panel.showTabs || tabs.currentIndex === 2 || /fail|unavailable|error/i.test(panel.analysis.storageStatus); text: panel.analysis.storageStatus; textFormat: Text.PlainText; wrapMode: Text.Wrap; color: Theme.warning; font.pixelSize: Theme.smallSize }
        Label { Layout.fillWidth: true; visible: (panel.analysis.lastOperation.diagnostics || []).length > 0; text: String(panel.analysis.lastOperation.operation || "Analysis") + ": " + String(panel.analysis.lastOperation.status || "unavailable") + (panel.analysis.lastOperation.diagnostics && panel.analysis.lastOperation.diagnostics.length ? " · see retained diagnostics" : ""); textFormat: Text.PlainText; wrapMode: Text.WrapAnywhere; color: Theme.warning; font.pixelSize: Theme.smallSize }
        Label { Layout.fillWidth: true; visible: (panel.showTabs || tabs.currentIndex === 2) && panel.analysis.evidenceContext.capture_id !== undefined; text: "Capture " + String(panel.analysis.evidenceContext.capture_id || "").substring(0, 12) + " · cutoff " + panel.utc(panel.analysis.evidenceContext.cutoff_unix) + " · " + (panel.analysis.evidenceContext.context || ""); textFormat: Text.PlainText; wrapMode: Text.WrapAnywhere; font.pixelSize: Theme.captionSize; color: Theme.muted }
        Flow {
            Layout.fillWidth: true; spacing: 6
            ActionButton { iconName: "triangle-alert"; text: "Detect events"; visible: !panel.analysis.historical; enabled: panel.eog.loaded && !panel.analysis.busy; onClicked: panel.analysis.detect() }
            ActionButton { iconName: "git-compare-arrows"; text: "Search plans"; visible: !panel.analysis.historical; enabled: panel.eog.loaded && !panel.analysis.busy; onClicked: panel.analysis.searchPlans() }
            ActionButton { iconName: "check"; text: "Finish session"; visible: !panel.analysis.historical && panel.analysis.sessionId.length > 0; enabled: !panel.analysis.busy; onClicked: panel.analysis.finishSession() }
            ActionButton { iconName: "square"; text: "Cancel"; visible: panel.analysis.busy; onClicked: panel.analysis.cancel() }
            ActionButton { iconName: "rotate-ccw"; text: "Return to live"; visible: panel.analysis.historical; onClicked: panel.analysis.returnToLive() }
            ActionButton { iconName: "arrow-down-to-line"; text: "Import history"; enabled: !panel.analysis.busy; onClicked: importDialog.open() }
        }
        RowLayout {
            Layout.fillWidth: true; visible: panel.showTabs || tabs.currentIndex === 2
            TextField { id: objective; Layout.fillWidth: true; maximumLength: 4000; placeholderText: "Objective for an on-demand Nemotron investigation" }
            ActionButton { iconName: "workflow"; highlighted: true; text: "Ask Nemotron"; enabled: panel.eog.loaded && !panel.analysis.busy && !panel.analysis.historical && objective.text.trim().length > 0; onClicked: panel.analysis.investigate(objective.text) }
        }
        Label { Layout.fillWidth: true; visible: panel.showTabs || tabs.currentIndex === 2 || !panel.analysis.providerConfigured; text: panel.analysis.providerConfigured ? (panel.analysis.providerVerified ? "Local provider exchange verified; numerical qualification is separate." : "Provider configured; execution not yet verified.") : "Numerical provider unavailable. The recorded 3D network and measurements remain usable."; wrapMode: Text.Wrap; color: Theme.secondary; font.pixelSize: Theme.smallSize }
        TabBar {
            id: tabs; visible: panel.showTabs; Layout.fillWidth: true
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
                    Label { Layout.fillWidth: true; visible: panel.analysis.detectionDetails.status !== undefined; text: "Detection " + (panel.analysis.detectionDetails.status || "unavailable") + " · coverage " + (panel.analysis.detectionDetails.coverage || "unavailable") + "\n" + panel.pretty({assumptions: panel.analysis.detectionDetails.assumptions || [], limitations: panel.analysis.detectionDetails.limitations || []}); textFormat: Text.PlainText; wrapMode: Text.WrapAnywhere; color: Theme.warning }
                    Label { Layout.fillWidth: true; visible: panel.analysis.events.length === 0; text: "No completed detector events retained. An empty list before analysis does not establish a healthy network."; wrapMode: Text.Wrap }
                    ListView {
                        id: alerts
                        Layout.fillWidth: true
                        Layout.preferredHeight: count ? Math.min(340, Math.max(140, count * 130)) : 0
                        clip: true; spacing: 6
                        ScrollBar.vertical: ScrollBar {}
                        model: panel.analysis.events
                        delegate: Frame {
                            required property var modelData
                            width: alerts.width
                            contentItem: ColumnLayout {
                                Label { Layout.fillWidth: true; text: modelData.category + " · " + modelData.state + " · " + modelData.asset_id; textFormat: Text.PlainText; wrapMode: Text.Wrap; font.bold: true }
                                Label { Layout.fillWidth: true; text: "Onset " + panel.utc(modelData.onset_unix) + " · shutdown countdown: " + (typeof modelData.countdown_hours === "number" ? modelData.countdown_hours.toFixed(3) + " h" : "unavailable") + " · exposure " + panel.money(modelData.lost_revenue_per_hour) + "/h · coverage " + modelData.coverage; textFormat: Text.PlainText; wrapMode: Text.Wrap }
                                RowLayout {
                                    ActionButton { iconName: "chart-line"; text: "Inspect evidence"; onClicked: panel.selectedEventId = modelData.event_id }
                                    ActionButton { iconName: "check"; text: "Acknowledge"; enabled: !panel.analysis.historical && !panel.analysis.busy; onClicked: panel.analysis.acknowledge(modelData.event_id, "Reviewed in EOG console; acknowledgement does not clear the event.") }
                                }
                            }
                        }
                    }
                    EogEventDetails {
                        Layout.fillWidth: true
                        event: panel.selectedEventEvidence.event || ({})
                        evidence: panel.selectedEventEvidence
                    }
                }
            }
            ScrollView {
                id: planScroll; clip: true; contentWidth: availableWidth
                ColumnLayout {
                    width: planScroll.availableWidth
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: "Candidate" }
                        AppComboBox { Layout.fillWidth: true; model: panel.planChoices; textRole: "label"; currentIndex: panel.planIndex(panel.analysis.selectedPlanId); onActivated: panel.analysis.selectPlan(panel.analysis.plans[currentIndex].plan_id) }
                        Label { text: "Baseline" }
                        AppComboBox { Layout.fillWidth: true; model: panel.planChoices; textRole: "label"; currentIndex: panel.planIndex(panel.analysis.baselinePlanId); onActivated: panel.analysis.selectBaseline(panel.analysis.plans[currentIndex].plan_id) }
                    }
                    Label {
                        Layout.fillWidth: true; wrapMode: Text.Wrap; textFormat: Text.PlainText
                        text: {
                            const c = panel.analysis.selectedEvaluation, b = panel.analysis.baselineEvaluation
                            const cost = c.cost || {}, base = b.cost || {}
                            return "Candidate: " + (c.outcome || "unavailable") + " · coverage " + (c.coverage || "unavailable")
                                + " · trucking cost " + panel.money(cost.truck_cost) + " · lost revenue " + panel.money(cost.lost_revenue) + " · total " + panel.money(cost.total)
                                + (panel.analysis.comparisonCompatible ? "\nBaseline total " + panel.money(base.total) + " · candidate − baseline " + (typeof cost.total === "number" && typeof base.total === "number" ? panel.money(cost.total - base.total) : "unavailable") : "\nNumerical comparison unavailable until matching horizons and context are selected.")
                        }
                    }
                    EogPlanChart { Layout.fillWidth: true; candidate: panel.analysis.selectedEvaluation; baselineRun: panel.analysis.baselineEvaluation; compatible: panel.analysis.comparisonCompatible }
                    Flow {
                        Layout.fillWidth: true; spacing: 6
                        ActionButton { iconName: "activity"; text: "Evaluate no action"; enabled: !panel.analysis.busy && !panel.analysis.historical && panel.analysis.selectedPlanId.length > 0; onClicked: panel.analysis.evaluateNoAction() }
                        ActionButton { iconName: "settings-2"; text: "Edit proposal"; enabled: !panel.analysis.busy && !panel.analysis.historical; onClicked: { planText.text = panel.analysis.newPlanDraftJson(); editor.open() } }
                        ActionButton { iconName: "clipboard-check"; text: "Review validated advice"; enabled: panel.analysis.canReview; onClicked: { panel.reviewSnapshot = {evaluation: panel.analysis.selectedEvaluation, validation_id: panel.analysis.selectedValidation.validation_id, plan_hash: panel.analysis.selectedValidation.plan_hash, validation: panel.analysis.selectedValidation}; reviewNote.text = ""; review.open() } }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        ActionButton { iconName: "check-check"; text: "Validate exact plan"; tooltip: "Run the independent validator against the selected plan and the evaluator identity retained by its numerical result."
                            enabled: !panel.analysis.busy && !panel.analysis.historical && panel.analysis.selectedEvaluatorMethod.length > 0 && panel.analysis.selectedPlanId.length > 0
                            onClicked: panel.analysis.validateSelected(panel.analysis.selectedEvaluatorMethod)
                        }
                        Label { Layout.fillWidth: true; text: panel.analysis.selectedEvaluatorMethod.length ? "Independent validation is required before advisory review." : "No supported numerical evaluator identity is retained for this plan."; wrapMode: Text.Wrap; color: Theme.muted; font.pixelSize: Theme.smallSize }
                    }
                    EogPlanDetails { Layout.fillWidth: true; evaluation: panel.analysis.selectedEvaluation; validation: panel.analysis.selectedValidation }
                    Label { Layout.fillWidth: true; text: "Search coverage: " + (panel.analysis.searchDetails.optimality || "unavailable") + "\n" + (panel.analysis.searchDetails.space_description || "No completed search scope retained."); textFormat: Text.PlainText; wrapMode: Text.WrapAnywhere; color: Theme.secondary }
                    Label { Layout.fillWidth: true; text: (panel.analysis.searchDetails.limitations || []).join("\n"); textFormat: Text.PlainText; wrapMode: Text.WrapAnywhere; color: Theme.warning }
                }
            }
            ColumnLayout {
                id: tracePage
                spacing: 8
                Label {
                    Layout.fillWidth: true
                    text: panel.analysis.summary || "Actual model exchanges and tool executions appear here as they are retained. No model explanation has completed."
                    textFormat: Text.PlainText; wrapMode: Text.Wrap; maximumLineCount: 4; elide: Text.ElideRight
                    color: Theme.text; font.pixelSize: Theme.bodySize
                }
                Label { Layout.fillWidth: true; text: "EXECUTION FEED · " + panel.analysis.trace.length + " retained events"; color: Theme.secondary; font.pixelSize: Theme.captionSize; font.bold: true }
                ListView {
                    id: traceFeed
                    objectName: "eogExecutionFeed"
                    Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumHeight: 80
                    model: panel.analysis.trace; clip: true; spacing: 6
                    ScrollBar.vertical: ScrollBar {}
                    delegate: Frame {
                        id: traceRow
                        required property var modelData
                        required property int index
                        width: traceFeed.width - 12
                        padding: 10
                        background: Rectangle { color: Theme.raised; radius: Theme.cornerRadius; border.color: Theme.divider }
                        contentItem: RowLayout {
                            spacing: 9
                            Image { Layout.alignment: Qt.AlignTop; Layout.preferredWidth: 18; Layout.preferredHeight: 18; source: Theme.icon(panel.traceIcon(traceRow.modelData)); sourceSize.width: 18; sourceSize.height: 18; fillMode: Image.PreserveAspectFit; Accessible.ignored: true }
                            ColumnLayout {
                                Layout.fillWidth: true; spacing: 4
                                Label {
                                    Layout.fillWidth: true; text: (traceRow.index + 1) + " · " + panel.traceTitle(traceRow.modelData)
                                    textFormat: Text.PlainText; wrapMode: Text.WrapAnywhere; font.bold: true; font.pixelSize: Theme.bodySize
                                    color: traceRow.modelData.type === "failure" || traceRow.modelData.type === "tool_result" && traceRow.modelData.ok === false ? Theme.error : Theme.text
                                }
                                Label { Layout.fillWidth: true; text: panel.traceDetail(traceRow.modelData); textFormat: Text.PlainText; wrapMode: Text.WrapAnywhere; color: Theme.secondary; font.pixelSize: Theme.smallSize }
                                ActionButton {
                                    visible: traceRow.modelData.type === "final" && !!traceRow.modelData.selected_plan_id
                                    enabled: panel.planIndex(traceRow.modelData.selected_plan_id) >= 0
                                    iconName: "chart-line"; text: "Inspect selected plan"
                                    onClicked: { panel.analysis.selectPlan(traceRow.modelData.selected_plan_id); tabs.currentIndex = 1 }
                                }
                            }
                        }
                    }
                    Label { anchors.centerIn: parent; width: parent.width - 24; visible: traceFeed.count === 0; text: "No model exchanges or tool executions retained for this session."; textFormat: Text.PlainText; wrapMode: Text.Wrap; horizontalAlignment: Text.AlignHCenter; color: Theme.muted }
                }
                Label {
                    Layout.fillWidth: true
                    text: panel.analysis.historical ? panel.analysis.operatorRecords.length + " retained operator records · historical evidence cannot authorize current actions."
                        : "Operator records are confirmed in storage status and retained in the session archive."
                    textFormat: Text.PlainText; wrapMode: Text.Wrap; color: Theme.muted; font.pixelSize: Theme.captionSize
                }
                CheckBox { id: exactTrace; objectName: "showExactEogTrace"; text: "Show full explanation and exact trace / operator JSON" }
                Loader {
                    objectName: "exactEogTraceDisclosure"
                    Layout.fillWidth: true; Layout.preferredHeight: active ? Math.max(100, tracePage.height * 0.35) : 0
                    active: exactTrace.checked
                    sourceComponent: ScrollView {
                        clip: true; contentWidth: availableWidth
                        TextArea {
                            readOnly: true; selectByMouse: true; wrapMode: TextEdit.WrapAnywhere
                            text: panel.pretty({summary: panel.analysis.summary, trace: panel.analysis.trace, operator_records: panel.analysis.operatorRecords})
                            font.family: Theme.monoFamily; font.pixelSize: Theme.smallSize
                        }
                    }
                }
            }
            ScrollView {
                id: historyScroll; clip: true; contentWidth: availableWidth
                ColumnLayout {
                    width: historyScroll.availableWidth
                    ActionButton { iconName: "history"; text: "Refresh first page"; onClicked: panel.analysis.refreshHistory(0) }
                    Repeater {
                        model: panel.analysis.sessions
                        delegate: Frame {
                            required property var modelData
                            Layout.fillWidth: true
                            ColumnLayout {
                                anchors.fill: parent
                                Label { Layout.fillWidth: true; text: (modelData.objective || "Investigation") + " · " + modelData.status + (modelData.cutoff_unix ? "\nCutoff " + panel.utc(modelData.cutoff_unix) : "") + (modelData.retained_analyses !== undefined ? " · " + modelData.retained_analyses + " retained analyses" : "") + "\nSession " + String(modelData.session_id || "").substring(0, 12); textFormat: Text.PlainText; wrapMode: Text.WrapAnywhere }
                                RowLayout {
                                    ActionButton { iconName: "history"; text: "Open historical evidence"; visible: modelData.session_id.length > 0; enabled: !panel.analysis.busy; onClicked: panel.analysis.openHistory(modelData.session_id) }
                                    ActionButton { iconName: "arrow-up-from-line"; text: "Export new archive"; visible: modelData.session_id.length > 0; onClicked: { panel.exportId = modelData.session_id; exportDialog.open() } }
                                    ActionButton { iconName: "chevron-down"; text: "Next page"; visible: modelData.next_offset !== undefined; onClicked: panel.analysis.refreshHistory(modelData.next_offset) }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

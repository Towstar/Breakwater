pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    id: panel
    objectName: "storageSciencePanel"
    required property var scientificWorker
    required property var storageScience
    required property var history
    readonly property bool historicalView: history.historical
    readonly property var resultData: (historicalView ? history.scientificResult : storageScience.result) || ({})
    readonly property var displayTanks: (historicalView ? history.scientificTanks : storageScience.tanks) || []
    readonly property string displayTankId: historicalView ? history.selectedScientificTankId : storageScience.selectedTankId
    readonly property var displaySensitivities: (historicalView ? history.scientificSensitivityRows : storageScience.sensitivityRows) || []
    readonly property var readiness: scientificWorker.readiness || ({})
    readonly property bool hasResult: !!resultData.run_id
    readonly property bool busy: scientificWorker.busy || storageScience.busy
    contentWidth: availableWidth
    clip: true

    function number(value) {
        if (value === undefined || value === null || !Number.isFinite(Number(value))) return "—"
        const n = Number(value), magnitude = Math.abs(n)
        return magnitude > 1e7 || (magnitude > 0 && magnitude < 0.001) ? n.toExponential(5) : n.toFixed(4)
    }
    function detected(value) { return value === undefined ? "Not checked" : value ? "Detected" : "Not detected" }
    function selectedColumn(id) {
        const ids = storageScience.selectedControlIds
        for (let i = 0; i < ids.length; ++i) if (ids[i] === id) return i + 1
        return 0
    }
    function comparisonChart() {
        const data = (historicalView ? history.scientificChart : storageScience.chart) || ({})
        const rows = data.series || [], series = []
        for (let i = 0; i < rows.length; ++i) {
            const row = Object.assign({}, rows[i])
            row.reference = row.label === "Native"
            series.push(row)
        }
        return Object.assign({}, data, {series: series})
    }
    component PlainLabel: Label { textFormat: Text.PlainText; color: "#d2e1e9"; font.pixelSize: 12 }
    component SectionLabel: PlainLabel { color: "#9eb5c2"; font.pixelSize: 10; font.letterSpacing: 1 }

    ColumnLayout {
        width: panel.availableWidth
        spacing: 9
        SectionLabel { text: "SCIENTIFIC WORKER · READ-ONLY STORAGE COMPARISON" }
        PlainLabel {
            objectName: "historicalScienceBanner"
            Layout.fillWidth: true; visible: panel.historicalView; wrapMode: Text.WordWrap; color: "#ffcf70"
            text: "HISTORICAL SCIENTIFIC EVIDENCE · Saved or imported records cannot authenticate worker execution, restore a current capture or authorize controls."
        }
        RowLayout {
            Layout.fillWidth: true; visible: panel.historicalView; spacing: 8
            PlainLabel { text: "Saved result"; color: "#9eb5c2" }
            ComboBox {
                id: historicalScienceSelector
                objectName: "historicalScienceResultSelector"
                Layout.fillWidth: true
                model: panel.history.scientificEvidence; textRole: "result_id"; valueRole: "result_id"
                enabled: count > 0
                currentIndex: {
                    const rows = panel.history.scientificEvidence
                    for (let i = 0; i < rows.length; ++i) if (rows[i].result_id === panel.history.selectedScientificResultId) return i
                    return -1
                }
                onActivated: panel.history.selectScientificResult(currentValue)
            }
        }
        RowLayout {
            visible: !panel.historicalView
            Layout.fillWidth: true; spacing: 8
            Button { objectName: "checkScienceHealth"; text: "Check health"; enabled: !panel.busy; onClicked: panel.scientificWorker.checkHealth() }
            Button { objectName: "checkScienceCapabilities"; text: "Check capabilities"; enabled: !panel.busy; onClicked: panel.scientificWorker.checkCapabilities() }
            Button { objectName: "runStorageScience"; text: "Compare with PyTorch"; highlighted: true; enabled: panel.storageScience.canRun && !panel.busy; onClicked: panel.storageScience.run() }
            Button { objectName: "cancelStorageScience"; text: "Cancel"; enabled: panel.busy; onClicked: { if (panel.storageScience.busy) panel.storageScience.cancel(); else panel.scientificWorker.cancel() } }
            Item { Layout.fillWidth: true }
        }
        PlainLabel {
            objectName: "storageScienceStatus"
            Layout.fillWidth: true; wrapMode: Text.WordWrap; color: "#60d2d1"
            text: panel.historicalView ? (panel.hasResult ? "Historical " + panel.resultData.analysis_mode + " result · read only" : "This session contains no stored scientific result.") : panel.storageScience.status
        }
        PlainLabel {
            objectName: "storageScienceHistoryStatus"
            Layout.fillWidth: true; visible: text.length > 0; wrapMode: Text.WordWrap; font.pixelSize: 11
            color: "#ffcf70"
            text: panel.storageScience.historyStatus ? "Local science history: " + panel.storageScience.historyStatus : ""
        }
        PlainLabel {
            Layout.fillWidth: true; wrapMode: Text.WordWrap; font.pixelSize: 11; color: "#9eb5c2"
            visible: !panel.historicalView && !panel.hasResult && !panel.storageScience.busy
            text: panel.storageScience.canRun ? "A captured native forecast is ready. The first comparison can run before worker execution is verified." : "Run a native forecast on the current network to capture inputs for comparison. Scientific output does not apply controls or approve a plan."
        }
        PlainLabel {
            objectName: "storageAgreement"
            Layout.fillWidth: true; visible: panel.hasResult; wrapMode: Text.WordWrap; font.bold: true
            color: panel.resultData.within_tolerance ? "#a8d9df" : "#ffcf70"
            text: panel.resultData.within_tolerance ? "Returned native / PyTorch samples agree within the declared tolerance." : "Returned native / PyTorch samples differ beyond the declared tolerance."
        }
        PlainLabel {
            Layout.fillWidth: true; visible: panel.hasResult; wrapMode: Text.WordWrap; font.pixelSize: 11
            text: "Native cost: " + panel.number(panel.resultData.native_cost) + " · PyTorch cost: " + panel.number(panel.resultData.worker_cost) + " · Difference (PyTorch − native): " + panel.number(panel.resultData.cost_difference) + " synthetic units"
        }
        RowLayout {
            Layout.fillWidth: true; visible: panel.hasResult; spacing: 10
            PlainLabel { text: "Tank"; color: "#9eb5c2" }
            ComboBox {
                id: scienceTankSelector
                objectName: "storageScienceTankSelector"
                Layout.preferredWidth: 240; Layout.maximumWidth: panel.width * 0.5
                model: panel.displayTanks; textRole: "label"; valueRole: "id"
                enabled: count > 0
                currentIndex: {
                    const rows = panel.displayTanks
                    for (let i = 0; i < rows.length; ++i) if (rows[i].id === panel.displayTankId) return i
                    return -1
                }
                onActivated: { if (panel.historicalView) panel.history.selectScientificTank(currentValue); else panel.storageScience.selectTank(currentValue) }
                contentItem: PlainLabel { text: scienceTankSelector.displayText; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight; leftPadding: 8; rightPadding: 24 }
                delegate: ItemDelegate {
                    id: tankOption
                    required property var modelData
                    required property int index
                    width: scienceTankSelector.width; highlighted: scienceTankSelector.highlightedIndex === index
                    contentItem: PlainLabel { text: tankOption.modelData.label; elide: Text.ElideRight }
                }
            }
            PlainLabel { Layout.fillWidth: true; text: "Native is authoritative for volume feasibility. Agreement grants no approval."; font.pixelSize: 11; wrapMode: Text.WordWrap; color: "#ffcf70" }
        }
        TrajectoryChart {
            objectName: "storageScienceChart"
            Layout.fillWidth: true; Layout.preferredHeight: 230; Layout.minimumHeight: 180
            visible: panel.hasResult
            chart: panel.comparisonChart()
        }
        SectionLabel { visible: panel.hasResult; text: "ALL TANKS · RETURNED SAMPLES · NUMERICAL DIFFERENCE" }
        Repeater {
            model: panel.resultData.tank_errors || []
            delegate: PlainLabel {
                required property var modelData
                Layout.fillWidth: true; wrapMode: Text.WordWrap; font.pixelSize: 11
                color: modelData.within_tolerance ? "#b1c7d3" : "#ffcf70"
                text: modelData.tank_id + " · max absolute error " + panel.number(modelData.max_absolute_error_m3) + " m³ · scaled error " + panel.number(modelData.max_scaled_error) + " · " + (modelData.within_tolerance ? "within tolerance" : "outside tolerance")
            }
        }
        SectionLabel { visible: panel.hasResult; text: "CAPTURED PROVENANCE" }
        PlainLabel {
            objectName: "storageScienceProvenance"
            Layout.fillWidth: true; visible: panel.hasResult; wrapMode: Text.WrapAnywhere; font.pixelSize: 11
            text: "Result: " + (panel.resultData.result_id || "—") + " · " + (panel.resultData.created_at || "—") + " · " + (panel.resultData.analysis_mode || "—") +
                  "\nRun: " + (panel.resultData.run_id || "—") + "\nSnapshot: " + (panel.resultData.snapshot_id || "—") +
                  "\nPhysical hash: " + (panel.resultData.physical_hash || "—") + "\nNumerical revision: " + panel.resultData.numerical_revision +
                  "\nHorizon: " + panel.number(panel.resultData.horizon_h) + " h · Output step: " + panel.number(panel.resultData.step_h) + " h" +
                  "\nNative solver: " + (panel.resultData.native_solver_version || "—") + "\nImplementation: " + ((panel.resultData.provenance || {}).implementation_version || "—") +
                  " · dtype: " + ((panel.resultData.provenance || {}).dtype || "—") + " · device: " + ((panel.resultData.provenance || {}).device || "—")
        }
        Repeater {
            model: panel.resultData.warnings || []
            delegate: PlainLabel { required property var modelData; Layout.fillWidth: true; text: String(modelData); wrapMode: Text.WordWrap; font.pixelSize: 11; color: "#ffcf70" }
        }
        SectionLabel { visible: !panel.historicalView; text: "SELECTED-CONTROL SENSITIVITIES · CAPTURED VALUES"; Layout.topMargin: 8 }
        PlainLabel {
            visible: !panel.historicalView
            Layout.fillWidth: true; wrapMode: Text.WordWrap; font.pixelSize: 11; color: "#9eb5c2"
            text: "Select interior captured controls to calculate terminal-volume and cost derivatives. Values and bounds are read only. Unselected controls remain captured constants; no input is shifted into the derivative domain."
        }
        PlainLabel {
            Layout.fillWidth: true; visible: !panel.historicalView && !panel.storageScience.controls.length; wrapMode: Text.WordWrap; font.pixelSize: 11; color: "#9eb5c2"
            text: "Capture a native forecast to inspect its control values and derivative eligibility."
        }
        ListView {
            id: controlsList
            objectName: "storageSensitivityControls"
            Layout.fillWidth: true; Layout.preferredHeight: Math.min(320, Math.max(110, contentHeight))
            visible: !panel.historicalView && count > 0
            model: panel.storageScience.controls
            clip: true; reuseItems: true; spacing: 6
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}
            delegate: Rectangle {
                id: controlRow
                required property var modelData
                width: controlsList.width - 14; height: controlContent.implicitHeight + 16
                color: "#1d3241"; radius: 4
                RowLayout {
                    id: controlContent
                    anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 8
                    spacing: 10
                    CheckBox {
                        objectName: "selectSensitivityControl_" + controlRow.modelData.control_id
                        Accessible.name: "Select sensitivity control " + controlRow.modelData.control_id
                        Layout.alignment: Qt.AlignTop
                        checked: panel.selectedColumn(controlRow.modelData.control_id) > 0
                        enabled: controlRow.modelData.eligible && !panel.busy
                        onClicked: panel.storageScience.selectControl(controlRow.modelData.control_id, checked)
                    }
                    ColumnLayout {
                        Layout.fillWidth: true; spacing: 3
                        PlainLabel {
                            Layout.fillWidth: true; wrapMode: Text.WrapAnywhere; font.bold: true
                            text: controlRow.modelData.label + (panel.selectedColumn(controlRow.modelData.control_id) ? " · selected column " + panel.selectedColumn(controlRow.modelData.control_id) : "")
                        }
                        PlainLabel { Layout.fillWidth: true; text: controlRow.modelData.control_id; wrapMode: Text.WrapAnywhere; font.pixelSize: 11; color: "#aebfca" }
                        PlainLabel {
                            Layout.fillWidth: true; wrapMode: Text.WordWrap; font.pixelSize: 11
                            text: "Captured: " + panel.number(controlRow.modelData.captured_value) + " " + controlRow.modelData.unit + " · Bounds: [" + panel.number(controlRow.modelData.minimum) + ", " + panel.number(controlRow.modelData.maximum) + "] " + controlRow.modelData.unit
                        }
                        PlainLabel {
                            Layout.fillWidth: true; wrapMode: Text.WordWrap; font.pixelSize: 11
                            color: controlRow.modelData.eligible ? "#9eb5c2" : "#ffcf70"
                            text: controlRow.modelData.reason || (controlRow.modelData.eligible ? "Eligible interior captured value." : "Unavailable for derivatives at this captured value.")
                        }
                    }
                }
            }
        }
        RowLayout {
            visible: !panel.historicalView
            Layout.fillWidth: true; spacing: 10
            Button {
                objectName: "runStorageSensitivities"
                text: "Run selected sensitivities"
                enabled: panel.storageScience.canRunSensitivities && !panel.busy
                onClicked: panel.storageScience.runSensitivities()
            }
            PlainLabel {
                objectName: "storageSensitivitySelection"
                Layout.fillWidth: true; wrapMode: Text.WordWrap; font.pixelSize: 11; color: "#9eb5c2"
                text: panel.storageScience.selectedControlIds.length + " selected · columns follow the captured catalog order. The forward comparison uses captured controls independently of this selection."
            }
        }
        SectionLabel {
            Layout.fillWidth: true; wrapMode: Text.WrapAnywhere
            visible: panel.displaySensitivities.length > 0
            text: "TERMINAL SENSITIVITIES · " + panel.displayTankId
        }
        PlainLabel {
            Layout.fillWidth: true; visible: panel.displaySensitivities.length > 0
            wrapMode: Text.WordWrap; font.pixelSize: 11; color: "#9eb5c2"
            text: "Showing the selected tank's terminal-volume Jacobian and the cost gradient, in result column order. Select another tank above to inspect its derivatives."
        }
        ListView {
            id: sensitivityList
            objectName: "storageSensitivityResults"
            Layout.fillWidth: true; Layout.preferredHeight: Math.min(300, Math.max(100, contentHeight))
            visible: count > 0
            model: panel.displaySensitivities
            clip: true; reuseItems: true; spacing: 6
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}
            delegate: Rectangle {
                id: sensitivityRow
                required property var modelData
                required property int index
                width: sensitivityList.width - 14; height: sensitivityContent.implicitHeight + 16
                color: "#1d3241"; radius: 4
                ColumnLayout {
                    id: sensitivityContent
                    anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 8
                    spacing: 5
                    PlainLabel { Layout.fillWidth: true; text: "Column " + (sensitivityRow.index + 1) + " · " + sensitivityRow.modelData.control_id; wrapMode: Text.WrapAnywhere; font.bold: true }
                    GridLayout {
                        Layout.fillWidth: true; columns: panel.width < 820 ? 1 : 3; columnSpacing: 18; rowSpacing: 5
                        PlainLabel { Layout.fillWidth: true; text: "Captured: " + panel.number(sensitivityRow.modelData.captured_value) + " " + sensitivityRow.modelData.unit; wrapMode: Text.WordWrap; font.pixelSize: 11 }
                        PlainLabel { Layout.fillWidth: true; text: "Terminal-volume Jacobian: " + panel.number(sensitivityRow.modelData.terminal_volume_jacobian_h) + " h"; wrapMode: Text.WordWrap; font.pixelSize: 11 }
                        PlainLabel { Layout.fillWidth: true; text: "Cost gradient: " + panel.number(sensitivityRow.modelData.cost_gradient) + " synthetic units / (m³/h)"; wrapMode: Text.WordWrap; font.pixelSize: 11 }
                    }
                }
            }
        }
        PlainLabel {
            Layout.fillWidth: true; wrapMode: Text.WordWrap; font.pixelSize: 11; color: "#ffcf70"
            text: "Derivatives apply only to the explicitly selected interior captured controls. They do not differentiate through bounds, candidate rejection, discrete search or operator approval, and never change operating controls."
        }
        SectionLabel { visible: !panel.historicalView; text: "LATEST WORKER PROBE / SESSION"; Layout.topMargin: 6 }
        PlainLabel { visible: !panel.historicalView; Layout.fillWidth: true; text: panel.scientificWorker.status; wrapMode: Text.WordWrap; font.pixelSize: 11; color: "#9eb5c2" }
        GridLayout {
            visible: !panel.historicalView
            Layout.fillWidth: true; columns: 2; columnSpacing: 16; rowSpacing: 6
            PlainLabel { text: "Legacy health"; color: "#9eb5c2" }
            PlainLabel { Layout.fillWidth: true; text: panel.readiness.health_status || "unchecked"; wrapMode: Text.WordWrap }
            PlainLabel { text: "Python version"; color: "#9eb5c2" }
            PlainLabel { Layout.fillWidth: true; text: panel.readiness.python_version || "Not checked"; wrapMode: Text.WordWrap }
            PlainLabel { text: "PyTorch package"; color: "#9eb5c2" }
            PlainLabel { Layout.fillWidth: true; text: panel.detected(panel.readiness.torch_detected); wrapMode: Text.WordWrap }
            PlainLabel { text: "PhysicsNeMo package"; color: "#9eb5c2" }
            PlainLabel { Layout.fillWidth: true; text: panel.detected(panel.readiness.physicsnemo_detected); wrapMode: Text.WordWrap }
            PlainLabel { text: "Storage mapper deployed"; color: "#9eb5c2" }
            PlainLabel { Layout.fillWidth: true; text: panel.readiness.storage_implemented === undefined ? "Not checked" : panel.readiness.storage_implemented ? (panel.readiness.storage_execution_verified ? "Implemented" : "Implemented · execution not yet verified") : "Unavailable"; wrapMode: Text.WordWrap }
            PlainLabel { text: "Storage execution"; color: "#9eb5c2" }
            PlainLabel { Layout.fillWidth: true; text: panel.readiness.storage_execution_verified === undefined ? "Not checked" : panel.readiness.storage_execution_verified ? "Verified in the latest worker session" : "Not verified in the latest worker process"; wrapMode: Text.WordWrap }
        }
        PlainLabel {
            visible: !panel.historicalView
            Layout.fillWidth: true; wrapMode: Text.WordWrap; font.pixelSize: 11; color: "#9eb5c2"
            text: "Health is a compatibility check. Package detection and deployed code do not establish scientific execution. Execution verification is process-local and resets when the worker restarts. PhysicsNeMo route inference is unavailable."
        }
        PlainLabel {
            Layout.fillWidth: true; wrapMode: Text.WordWrap; font.pixelSize: 11; color: "#ffcf70"
            text: panel.historicalView ? "Historical evidence is preserved independently of current numerical edits. Its scientific comparison and derivatives grant no feasibility verdict or approval." : "This read-only comparison does not optimize controls, evaluate chemical quality or change feasibility and approval. Numerical edits invalidate the captured result."
        }
    }
}

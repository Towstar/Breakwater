pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    id: panel
    objectName: "storageSciencePanel"
    required property var scientificWorker
    required property var storageScience
    readonly property var resultData: storageScience.result || ({})
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
    function comparisonChart() {
        const data = storageScience.chart || ({})
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
        RowLayout {
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
            text: panel.storageScience.status
        }
        PlainLabel {
            Layout.fillWidth: true; wrapMode: Text.WordWrap; font.pixelSize: 11; color: "#9eb5c2"
            visible: !panel.hasResult && !panel.storageScience.busy
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
                model: panel.storageScience.tanks; textRole: "label"; valueRole: "id"
                enabled: count > 0
                currentIndex: {
                    const rows = panel.storageScience.tanks
                    for (let i = 0; i < rows.length; ++i) if (rows[i].id === panel.storageScience.selectedTankId) return i
                    return -1
                }
                onActivated: panel.storageScience.selectTank(currentValue)
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
            text: "Run: " + (panel.resultData.run_id || "—") + "\nSnapshot: " + (panel.resultData.snapshot_id || "—") +
                  "\nPhysical hash: " + (panel.resultData.physical_hash || "—") + "\nNumerical revision: " + panel.resultData.numerical_revision +
                  "\nHorizon: " + panel.number(panel.resultData.horizon_h) + " h · Output step: " + panel.number(panel.resultData.step_h) + " h" +
                  "\nImplementation: " + ((panel.resultData.provenance || {}).implementation_version || "—") +
                  " · dtype: " + ((panel.resultData.provenance || {}).dtype || "—") + " · device: " + ((panel.resultData.provenance || {}).device || "—")
        }
        Repeater {
            model: panel.resultData.warnings || []
            delegate: PlainLabel { required property var modelData; Layout.fillWidth: true; text: String(modelData); wrapMode: Text.WordWrap; font.pixelSize: 11; color: "#ffcf70" }
        }
        SectionLabel { text: "LATEST WORKER PROBE / SESSION"; Layout.topMargin: 6 }
        PlainLabel { Layout.fillWidth: true; text: panel.scientificWorker.status; wrapMode: Text.WordWrap; font.pixelSize: 11; color: "#9eb5c2" }
        GridLayout {
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
            Layout.fillWidth: true; wrapMode: Text.WordWrap; font.pixelSize: 11; color: "#9eb5c2"
            text: "Health is a compatibility check. Package detection and deployed code do not establish scientific execution. Execution verification is process-local and resets when the worker restarts. PhysicsNeMo route inference is unavailable."
        }
        PlainLabel {
            Layout.fillWidth: true; wrapMode: Text.WordWrap; font.pixelSize: 11; color: "#ffcf70"
            text: "This read-only comparison does not optimize controls, evaluate chemical quality or change feasibility and approval. Numerical edits invalidate the captured result."
        }
    }
}

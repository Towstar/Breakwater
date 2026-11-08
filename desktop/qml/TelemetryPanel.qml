import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Dialog {
    id: panel
    required property var telemetry
    parent: Overlay.overlay
    width: Math.min(920, parent ? parent.width - 236 : 920)
    height: Math.min(720, parent ? parent.height - 36 : 720)
    x: 220 + Math.max(0, ((parent ? parent.width : 1156) - 236 - width) / 2)
    y: Math.max(16, ((parent ? parent.height : 756) - height) / 2)
    modal: false
    closePolicy: Popup.CloseOnEscape
    title: "Synthetic monitoring and replay"
    standardButtons: Dialog.Close
    font.family: Theme.fontFamily
    font.pixelSize: Theme.bodySize
    padding: 14
    palette.window: Theme.surface
    palette.base: Theme.raised
    palette.button: Theme.raised
    palette.buttonText: Theme.text
    palette.text: Theme.text
    palette.windowText: Theme.text
    palette.placeholderText: Theme.muted
    palette.highlight: Theme.text
    palette.highlightedText: Theme.background
    palette.mid: Theme.controlBorder
    background: Rectangle { color: Theme.surface; radius: Theme.cornerRadius; border.color: Theme.divider }
    header: ToolBar {
        implicitHeight: 52; padding: 8
        background: Rectangle { color: Theme.surface; Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 1; color: Theme.divider } }
        contentItem: RowLayout {
            spacing: 12
            Label { Layout.fillWidth: true; text: panel.title; color: Theme.text; font.pixelSize: 18; font.weight: Font.DemiBold }
            ActionButton { objectName: "telemetryPanelClose"; iconName: "x"; text: "Close"; tooltip: "Close"; display: AbstractButton.IconOnly; Layout.preferredWidth: 36; onClicked: panel.close() }
        }
    }
    property string notice: ""
    property string selectedSensorId: ""
    readonly property int renderedNumericSamples: traceChart.visible ? traceChart.numericSampleCount : 0
    readonly property var selectedSensor: {
        for (const row of telemetry.sensors) if (row.sensor_id === selectedSensorId) return row
        return ({})
    }
    function reconcileSelection() {
        const rows = telemetry.sensors
        let index = -1
        for (let i = 0; i < rows.length; ++i) if (rows[i].sensor_id === selectedSensorId) index = i
        if (index < 0 && rows.length > 0) {
            index = 0
            for (let i = 0; i < rows.length; ++i) if (rows[i].quantity !== "pump_state") { index = i; break }
            selectedSensorId = rows[index].sensor_id
        }
        if (sensorChoice.currentIndex !== index) sensorChoice.currentIndex = index
    }
    function statusLabel(value) { return value.replace(/_/g, " ") }
    function modeLabel(mode) {
        if (mode === "synthetic_live") return "Synthetic live"
        if (mode === "replay") return "Replay"
        if (mode === "historical") return "Historical"
        return "Manual"
    }
    Connections {
        target: panel.telemetry
        function onFailed(code, message) { panel.notice = code + ": " + message }
        function onRecordingExported(destination) { panel.notice = "Raw G1 frames saved. Replay reconstructs event time; original arrival jitter is not recorded." }
    }
    FileDialog {
        id: replayDialog
        nameFilters: ["Telemetry recording (*.ndjson *.jsonl)"]
        onAccepted: { panel.notice = ""; panel.telemetry.startReplay(selectedFile) }
    }
    FileDialog {
        id: historicalDialog
        nameFilters: ["Telemetry recording (*.ndjson *.jsonl)"]
        onAccepted: { panel.notice = ""; panel.telemetry.showHistorical(selectedFile) }
    }
    FileDialog {
        id: saveDialog
        fileMode: FileDialog.SaveFile
        defaultSuffix: "ndjson"
        nameFilters: ["Telemetry recording (*.ndjson)"]
        onAccepted: panel.telemetry.exportRecording(selectedFile)
    }
    contentItem: ScrollView {
        id: scroll
        clip: true
        contentWidth: availableWidth
        ScrollBar.vertical.policy: ScrollBar.AsNeeded
        ColumnLayout {
            width: scroll.availableWidth
            spacing: 14
            Label {
                Layout.fillWidth: true
                text: "Hypothetical sensors only. Measurements stay separate from design controls. Detection and observation-based forecasts are not connected in this increment."
                wrapMode: Text.Wrap
                color: Theme.secondary
            }
            Frame {
                Layout.fillWidth: true; padding: 12
                background: Rectangle { color: Theme.raised; radius: Theme.cornerRadius; border.color: Theme.divider }
                contentItem: ColumnLayout {
                spacing: 10
                Label { text: "INPUT SOURCE"; color: Theme.secondary; font.pixelSize: Theme.captionSize; font.bold: true }
                Flow {
                Layout.fillWidth: true; spacing: 8
                ActionButton { text: "Manual"; onClicked: panel.telemetry.manual() }
                ActionButton {
                    text: "Start synthetic input"
                    enabled: panel.telemetry.publisherConfigured && panel.telemetry.configurationHash.length > 0
                    onClicked: { panel.notice = ""; panel.telemetry.startConfiguredPublisher() }
                }
                ActionButton { text: "Open replay"; enabled: panel.telemetry.configurationHash.length > 0; onClicked: replayDialog.open() }
                ActionButton { text: "Historical view"; enabled: panel.telemetry.configurationHash.length > 0; onClicked: historicalDialog.open() }
                ActionButton { text: "Save recording"; enabled: panel.telemetry.tracePreview.length > 0; onClicked: saveDialog.open() }
                }
                }
            }
            Frame {
                Layout.fillWidth: true; padding: 12
                background: Rectangle { color: Theme.raised; radius: Theme.cornerRadius; border.color: Theme.divider }
                contentItem: ColumnLayout {
                spacing: 10
                Label { text: "PLAYBACK AND STREAM STATE"; color: Theme.secondary; font.pixelSize: Theme.captionSize; font.bold: true }
            Label {
                Layout.fillWidth: true
                visible: !panel.telemetry.publisherConfigured
                text: "No local plant publisher is configured. A publisher may be supplied separately; recorded synthetic fixtures can be replayed now."
                wrapMode: Text.Wrap
                color: Theme.warning
            }
            Label {
                Layout.fillWidth: true
                text: panel.modeLabel(panel.telemetry.mode) + " · " + panel.statusLabel(panel.telemetry.status) + " · event " + panel.telemetry.eventTime.toFixed(2) + " s · " + panel.telemetry.rate.toFixed(2) + "×"
                font.bold: true
                wrapMode: Text.Wrap
            }
            Label {
                Layout.fillWidth: true
                visible: panel.telemetry.mode === "replay" || panel.telemetry.mode === "historical"
                text: "Event-time replay; original arrival jitter and receiver freshness classifications are not reconstructed."
                color: Theme.warning
                wrapMode: Text.Wrap
            }
            Label {
                Layout.fillWidth: true
                visible: panel.telemetry.snapshot.replay_prefix === true
                text: "This recording is a retained prefix without an end frame. It is historical evidence, not a complete stream epoch."
                color: Theme.warning
                wrapMode: Text.Wrap
            }
            Flow {
                Layout.fillWidth: true
                spacing: 8
                ActionButton { text: panel.telemetry.paused ? "Resume" : "Pause"; enabled: panel.telemetry.active; onClicked: panel.telemetry.paused ? panel.telemetry.resume() : panel.telemetry.pause() }
                ComboBox {
                    id: speed
                    width: 112
                    model: ["0.25×", "0.5×", "1×", "2×", "4×"]
                    currentIndex: 2
                    enabled: panel.telemetry.active
                    onActivated: panel.telemetry.setRate([0.25, 0.5, 1, 2, 4][currentIndex])
                }
                ActionButton {
                    text: "Restart epoch"
                    enabled: panel.telemetry.mode === "replay" || panel.telemetry.mode === "historical" || panel.telemetry.active
                    onClicked: panel.telemetry.restart()
                    ToolTip.visible: hovered
                    ToolTip.text: "Starts a new epoch and resets recent evidence. Save a recording first to retain the current epoch."
                }
                ActionButton { text: "Stop"; enabled: panel.telemetry.active; onClicked: panel.telemetry.stop() }
            }
                }
            }
            Label {
                Layout.fillWidth: true
                text: panel.telemetry.error.length > 0 ? panel.telemetry.error : panel.notice
                visible: text.length > 0
                textFormat: Text.PlainText
                color: Theme.warning
                wrapMode: Text.Wrap
            }
            Frame {
                Layout.fillWidth: true; padding: 12
                background: Rectangle { color: Theme.raised; radius: Theme.cornerRadius; border.color: Theme.divider }
                contentItem: ColumnLayout {
                spacing: 10
            Label { text: "SENSOR OBSERVATIONS"; color: Theme.secondary; font.pixelSize: Theme.captionSize; font.bold: true }
            ComboBox {
                id: sensorChoice
                Layout.fillWidth: true
                model: panel.telemetry.sensors
                textRole: "label"
                valueRole: "sensor_id"
                onActivated: panel.selectedSensorId = currentValue
                onModelChanged: Qt.callLater(panel.reconcileSelection)
                enabled: count > 0
            }
            Label {
                Layout.fillWidth: true
                text: {
                    const row = panel.selectedSensor
                    if (!row.sensor_id) return "Place sensors in the Sensors editor to configure monitoring."
                    const value = row.canonical_value === undefined ? "missing" : String(row.canonical_value)
                    const age = typeof row.age_s === "number" ? row.age_s.toFixed(2) + " s" : "unavailable"
                    return value + " " + (row.unit || "") + " · " + row.status + " · age " + age
                        + "\nOriginal: " + (row.original_value === undefined ? "missing" : String(row.original_value)) + " " + (row.original_unit || "")
                        + "; canonical display: " + (row.canonical_unit || row.unit || "")
                        + "\nReported quality: " + (row.reported_quality || "missing") + "; receiver: " + (row.receiver_status || row.status || "missing")
                }
                textFormat: Text.PlainText
                wrapMode: Text.Wrap
            }
            SensorTraceChart {
                id: traceChart
                Layout.fillWidth: true
                Layout.preferredHeight: 180
                Layout.minimumHeight: 160
                samples: panel.telemetry.tracePreview
                sensorId: panel.selectedSensor.sensor_id || ""
                unit: panel.selectedSensor.unit || ""
                visible: panel.selectedSensor.quantity !== "pump_state"
            }
                }
            }
            Label {
                Layout.fillWidth: true
                text: "Recent trace: " + (panel.telemetry.snapshot.trace_readings || 0) + " readings / "
                    + (panel.telemetry.snapshot.trace_bytes || 0) + " bytes. Dropped: " + (panel.telemetry.counters.trace_dropped_readings || 0)
                    + ". Late: " + (panel.telemetry.counters.late_frames || 0) + ". Future: " + (panel.telemetry.counters.future_frames || 0)
                    + ". Duplicates: " + (panel.telemetry.counters.duplicate_frames || 0) + "."
                wrapMode: Text.Wrap
                color: Theme.secondary
            }
            Label {
                Layout.fillWidth: true
                text: "Finite negative and out-of-range measurements are retained. Replay and historical evidence cannot authorize operating changes. No observation mapper or physical fault diagnosis is supplied here."
                wrapMode: Text.Wrap
                color: Theme.secondary
            }
        }
    }
}

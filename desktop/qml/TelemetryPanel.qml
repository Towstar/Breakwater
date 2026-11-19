import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Dialog {
    id: panel
    required property var telemetry
    signal openInvestigation()
    parent: Overlay.overlay
    width: Math.min(920, parent ? parent.width - 32 : 920)
    height: Math.min(720, parent ? parent.height - 32 : 720)
    x: Math.max(16, ((parent ? parent.width : width + 32) - width) / 2)
    y: Math.max(16, ((parent ? parent.height : height + 32) - height) / 2)
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
    header: DialogHeading { owner: panel; closeName: "telemetryPanelClose" }
    footer: DialogFooter { owner: panel }
    property string notice: ""
    property string selectedSensorId: ""
    property string pendingRecordingKind: "journal"
    readonly property var capturedState: telemetry.observedCapture || ({})
    readonly property var captureDiagnostics: telemetry.observedCaptureDiagnostics || ({})
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
        function onObservedCaptureFailed(request, code, message) { panel.notice = code + ": " + message }
        function onRecordingExported(destination) {
            panel.notice = panel.pendingRecordingKind === "journal"
                ? "Receiver-timed recording saved. A missing end or interrupted source remains marked as incomplete."
                : "Raw event frames saved. This format does not retain original arrival timing."
        }
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
        property bool legacy: false
        fileMode: FileDialog.SaveFile
        defaultSuffix: "ndjson"
        nameFilters: ["Telemetry recording (*.ndjson)"]
        onAccepted: {
            panel.pendingRecordingKind = legacy ? "g1" : "journal"
            if (legacy) panel.telemetry.exportRecording(selectedFile)
            else panel.telemetry.exportReceiverJournal(selectedFile)
        }
    }
    contentItem: ScrollView {
        id: scroll; objectName: "telemetryScroll"
        clip: true
        contentWidth: availableWidth
        ScrollBar.vertical.policy: ScrollBar.AsNeeded
        ColumnLayout {
            width: scroll.availableWidth
            spacing: 14
            Label {
                Layout.fillWidth: true
                text: "Local synthetic measurements. Acquisition, frozen observations and commanded controls stay separate."
                wrapMode: Text.Wrap
                color: Theme.secondary
            }
            Frame {
                Layout.fillWidth: true; padding: 12
                background: Rectangle { color: Theme.raised; radius: Theme.cornerRadius; border.color: Theme.divider }
                contentItem: ColumnLayout {
                spacing: 10
                Label { text: "INPUT SOURCE"; color: Theme.secondary; font.pixelSize: Theme.captionSize; font.bold: true }
                RowLayout {
                    Layout.fillWidth: true; visible: panel.telemetry.bundledPublisher; spacing: 10
                    Label { text: "Preset"; color: Theme.secondary }
                    AppComboBox {
                        id: presetChoice; objectName: "syntheticPreset"; Layout.fillWidth: true
                        model: panel.telemetry.syntheticPresets
                        displayText: currentText.replace(/_/g, " ")
                        Accessible.name: "Local synthetic preset"
                    }
                    Label { text: "Seed"; color: Theme.secondary }
                    TextField {
                        id: seedInput; objectName: "syntheticSeed"; Layout.preferredWidth: 100; text: "1337"
                        validator: IntValidator { bottom: 0; top: 2147483647 }
                        selectByMouse: true; Accessible.name: "Local synthetic seed"
                    }
                }
                Flow {
                Layout.fillWidth: true; spacing: 8
                ActionButton { text: "Manual"; onClicked: panel.telemetry.manual() }
                ActionButton {
                    objectName: "startLocalInput"
                    text: panel.telemetry.active && panel.telemetry.mode === "synthetic_live" ? "New synthetic epoch" : "Start synthetic input"
                    enabled: panel.telemetry.publisherConfigured && panel.telemetry.configurationHash.length > 0
                        && (!panel.telemetry.bundledPublisher || seedInput.acceptableInput)
                    tooltip: "Starts a new local epoch with the chosen seed and preset. Pump loss needs an available pump."
                    onClicked: {
                        panel.notice = ""
                        if (panel.telemetry.bundledPublisher) panel.telemetry.startSynthetic(presetChoice.currentText, Number(seedInput.text))
                        else panel.telemetry.startConfiguredPublisher()
                    }
                }
                ActionButton { text: "Open replay"; enabled: panel.telemetry.configurationHash.length > 0; onClicked: replayDialog.open() }
                ActionButton { text: "Historical view"; enabled: panel.telemetry.configurationHash.length > 0; onClicked: historicalDialog.open() }
                ActionButton {
                    objectName: "saveTimedRecording"; text: "Save timed recording"
                    enabled: panel.telemetry.mode === "synthetic_live" && panel.telemetry.tracePreview.length > 0
                    onClicked: { saveDialog.legacy = false; saveDialog.open() }
                }
                ActionButton {
                    text: "Export event frames"; enabled: panel.telemetry.tracePreview.length > 0
                    onClicked: { saveDialog.legacy = true; saveDialog.open() }
                    tooltip: "Compatibility export. It does not retain original receiver arrival timing."
                }
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
                text: "Configure local Python to run the seeded plant, or open a recorded synthetic session."
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
                text: panel.telemetry.snapshot.recording_format === "receiver_journal_v1"
                    ? "Receiver-timed replay. Arrival delays and freshness follow the recording's playback clock; evidence stays read-only."
                    : "Event-only replay. Original arrival delays and receiver freshness cannot be reconstructed from this format."
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
                AppComboBox {
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
            Frame {
                Layout.fillWidth: true; padding: 12
                background: Rectangle { color: Theme.raised; radius: Theme.cornerRadius; border.color: Theme.divider }
                contentItem: ColumnLayout {
                    spacing: 8
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: "FROZEN OBSERVATION"; color: Theme.secondary; font.pixelSize: Theme.captionSize; font.bold: true; Layout.fillWidth: true }
                        ActionButton {
                            objectName: "captureObservedState"; text: panel.telemetry.observedCaptureBusy ? "Capturing…" : "Capture state"; iconName: "clipboard-check"
                            enabled: panel.telemetry.mode !== "manual" && !panel.telemetry.observedCaptureBusy
                            onClicked: { panel.notice = ""; panel.telemetry.requestObservedCapture() }
                        }
                    }
                    Label {
                        objectName: "observedCaptureSummary"; Layout.fillWidth: true; wrapMode: Text.Wrap
                        color: panel.telemetry.observedCaptureDisposition === "available" ? Theme.text : Theme.secondary
                        text: panel.capturedState.snapshot_id
                            ? "OBSERVED · " + panel.statusLabel(panel.telemetry.observedCaptureDisposition) + " · captured "
                                + Number(panel.capturedState.captured_event_time_s).toFixed(2) + " s · expires "
                                + Number(panel.capturedState.expires_event_time_s).toFixed(2) + " s"
                            : "Capture an immutable state to check calibration, coverage and measured delivery."
                    }
                    Label {
                        Layout.fillWidth: true; wrapMode: Text.Wrap; font.pixelSize: Theme.captionSize; color: Theme.muted
                        text: panel.capturedState.snapshot_id
                            ? (panel.capturedState.samples || []).length + " samples · " + (panel.captureDiagnostics.issues || []).length
                                + " mapping issues. The design and commanded controls are unchanged."
                            : "Project → Local monitoring example includes complete volume and pump feedback coverage."
                    }
                    Repeater {
                        model: panel.captureDiagnostics.issues || []
                        Label {
                            required property var modelData
                            textFormat: Text.PlainText
                            Layout.fillWidth: true; wrapMode: Text.Wrap; font.pixelSize: Theme.captionSize; color: Theme.warning
                            text: panel.statusLabel(modelData.code) + (modelData.attachment_id ? " · " + modelData.attachment_id : "")
                        }
                    }
                    Flow {
                        Layout.fillWidth: true; spacing: 8
                        ActionButton {
                            objectName: "openObservedInvestigation"; iconName: "workflow"; text: "Forecast / advisory review"
                            enabled: !!panel.capturedState.snapshot_id
                            onClicked: panel.openInvestigation()
                        }
                    }
                    Label {
                        Layout.fillWidth: true; wrapMode: Text.Wrap; font.pixelSize: Theme.captionSize; color: Theme.secondary
                        text: "For a repeatable local investigation, pause synthetic input, then capture state. Its event clock stays frozen while heartbeats continue. Forecasts are nominal; review never applies controls."
                    }
                }
            }
            Frame {
                Layout.fillWidth: true; padding: 12
                background: Rectangle { color: Theme.raised; radius: Theme.cornerRadius; border.color: Theme.divider }
                contentItem: ColumnLayout {
                    spacing: 10
                    Label { text: "INCIDENT EVIDENCE"; color: Theme.secondary; font.pixelSize: Theme.captionSize; font.bold: true }
                    Label { Layout.fillWidth: true; wrapMode: Text.Wrap; text: panel.telemetry.evidenceStorageStatus; color: Theme.muted; font.pixelSize: Theme.captionSize }
                    Label {
                        objectName: "detectionSummary"; Layout.fillWidth: true; wrapMode: Text.Wrap
                        text: "Volume balance · " + panel.statusLabel(panel.telemetry.detection.balance_status || "waiting_for_samples")
                            + " · " + Number(panel.telemetry.detection.window_s || 0).toFixed(1) + " / 30 s"
                        color: Theme.secondary
                    }
                    Label {
                        Layout.fillWidth: true; wrapMode: Text.Wrap; visible: !!panel.telemetry.detection.error
                        text: panel.telemetry.detection.error || ""; color: Theme.warning
                    }
                    Label {
                        Layout.fillWidth: true; wrapMode: Text.Wrap; visible: (panel.telemetry.detection.events || []).length === 0
                        text: "No detected physical symptom in the available evidence. Missing coverage or an unknown temporal model cannot establish a healthy balance."
                        color: Theme.muted; font.pixelSize: Theme.captionSize
                    }
                    Repeater {
                        model: panel.telemetry.detection.events || []
                        Frame {
                            required property var modelData
                            Layout.fillWidth: true; padding: 10
                            background: Rectangle { color: Theme.surface; radius: Theme.cornerRadius; border.color: Theme.divider }
                            contentItem: ColumnLayout {
                                spacing: 8
                                RowLayout {
                                    Layout.fillWidth: true
                                    Label {
                                        Layout.fillWidth: true; wrapMode: Text.Wrap; font.bold: true
                                        textFormat: Text.PlainText
                                        text: panel.statusLabel(modelData.detector_id) + " · " + modelData.asset_ids.join(", ") + " · " + modelData.status
                                        color: modelData.status === "active" ? Theme.warning : Theme.secondary
                                    }
                                    ActionButton {
                                        text: modelData.acknowledgement ? "Acknowledged" : "Acknowledge"
                                        enabled: !modelData.acknowledgement
                                        onClicked: panel.telemetry.acknowledgeDetection(modelData.event_id)
                                        tooltip: "Records operator acknowledgement. It does not clear the event or change controls."
                                    }
                                }
                                Label {
                                    Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.secondary; font.pixelSize: Theme.captionSize
                                    text: "Onset " + Number(modelData.onset_event_time_s).toFixed(2) + " s · updated "
                                        + Number(modelData.updated_event_time_s).toFixed(2) + " s · diagnosis unresolved"
                                }
                                Repeater {
                                    model: modelData.evidence
                                    Label {
                                        required property var modelData
                                        Layout.fillWidth: true; wrapMode: Text.Wrap; font.pixelSize: Theme.captionSize; color: Theme.secondary
                                        text: panel.statusLabel(modelData.metric) + ": " + Number(modelData.value).toPrecision(5)
                                            + " " + modelData.unit + " · absolute bound " + Number(modelData.threshold).toPrecision(5) + " " + modelData.unit
                                    }
                                }
                                Label { Layout.fillWidth: true; wrapMode: Text.Wrap; textFormat: Text.PlainText; text: modelData.explanation; color: Theme.muted; font.pixelSize: Theme.captionSize }
                            }
                        }
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
            AppComboBox {
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
                text: "Negative and out-of-range measurements are retained. Frozen observations and replay cannot authorize operating changes."
                wrapMode: Text.Wrap
                color: Theme.secondary
            }
        }
    }
}

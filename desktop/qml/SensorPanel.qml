import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: panel
    required property var controller
    parent: Overlay.overlay
    title: "Sensor editor"
    modal: false
    closePolicy: Popup.CloseOnEscape
    width: Math.min(960, parent ? parent.width - 236 : 960)
    height: Math.min(760, parent ? parent.height - 96 : 760)
    x: 220 + Math.max(0, ((parent ? parent.width : 1196) - 236 - width) / 2)
    y: 76 + Math.max(0, ((parent ? parent.height : 856) - 96 - height) / 2)
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
            ActionButton { objectName: "sensorPanelClose"; iconName: "x"; text: "Close"; tooltip: "Close"; display: AbstractButton.IconOnly; Layout.preferredWidth: 36; onClicked: panel.close() }
        }
    }
    property bool creating: true
    property bool submitting: false
    property string problem: ""
    readonly property var quantities: ["inventory", "level", "source_flow", "route_flow", "pump_state", "concentration", "pressure", "temperature", "custom_numeric"]
    readonly property string quantity: quantityBox.currentText
    readonly property string canonicalUnit: ({inventory:"m3", level:"m", source_flow:"m3/h", route_flow:"m3/h", pump_state:"state", concentration:"mg/L", pressure:"kPa", temperature:"degC"})[quantity] || unitField.text
    readonly property bool routeRequired: quantity === "route_flow" || quantity === "pump_state" || quantity === "concentration"
    readonly property bool genericAttachment: quantity === "pressure" || quantity === "temperature" || quantity === "custom_numeric"
    readonly property string attachmentKind: routeRequired ? "route" : genericAttachment ? attachmentKindBox.currentText : "asset"
    readonly property var attachmentChoices: {
        let result = []
        const list = attachmentKind === "route" ? controller.routes : controller.assets
        for (let i = 0; i < list.length; ++i) {
            const item = list[i]
            if ((quantity === "inventory" || quantity === "level") && item.kind !== "tank") continue
            if (quantity === "source_flow" && item.kind !== "source") continue
            if (quantity === "pump_state" && !item.pump) continue
            result.push({id: item.id, label: item.label || item.id})
        }
        return result
    }
    function indexOfValue(list, value) { const index = list.indexOf(value); return index < 0 ? 0 : index }
    function selectAttachment(id) {
        for (let i = 0; i < attachmentChoices.length; ++i) if (attachmentChoices[i].id === id) { attachmentBox.currentIndex = i; return }
        attachmentBox.currentIndex = 0
    }
    function beginNew() {
        creating = true; controller.selectSensor(""); problem = ""
        quantityBox.currentIndex = 0; attachmentKindBox.currentIndex = 0; attachmentBox.currentIndex = 0
        labelField.text = ""; sourceField.text = "synthetic-publisher"; channelField.text = ""; unitField.text = "custom"
        enabledBox.checked = true; periodField.text = "1"; ageField.text = "5"; uncertaintyField.text = "0"
        groupField.text = ""; roleBox.currentIndex = 0; minimumField.text = ""; maximumField.text = ""
        calibrationIdField.text = "identity"; calibrationVersionField.text = "1"; calibrationErrorField.text = "0"; knotsField.text = ""
        supportBox.currentIndex = 0; cellField.text = "0"; startField.text = "0"; endField.text = "1"
        acquisitionField.text = "0"; delayField.text = "0"
    }
    function populate() {
        const sensor = controller.selectedSensor
        if (!sensor.sensor_id) return
        quantityBox.currentIndex = indexOfValue(quantities, sensor.quantity)
        attachmentKindBox.currentIndex = sensor.attachment.kind === "route" ? 1 : 0
        selectAttachment(sensor.attachment.id)
        labelField.text = sensor.label; sourceField.text = sensor.source_id; channelField.text = sensor.channel_id; unitField.text = sensor.unit
        enabledBox.checked = sensor.enabled; periodField.text = String(sensor.expected_period_s); ageField.text = String(sensor.max_age_s)
        uncertaintyField.text = String(sensor.uncertainty_bound); groupField.text = sensor.group_id; roleBox.currentIndex = sensor.role === "redundant" ? 1 : 0
        minimumField.text = sensor.instrument_min === undefined ? "" : String(sensor.instrument_min)
        maximumField.text = sensor.instrument_max === undefined ? "" : String(sensor.instrument_max)
        calibrationIdField.text = sensor.calibration.calibration_id; calibrationVersionField.text = String(sensor.calibration.version)
        calibrationErrorField.text = String(sensor.calibration.error_bound)
        const knots = sensor.calibration.knots || []; let lines = []
        for (let i = 0; i < knots.length; ++i) lines.push(knots[i].level_m + ":" + knots[i].volume_m3)
        knotsField.text = lines.join("\n")
        const support = sensor.physical_support
        supportBox.currentIndex = indexOfValue(["cell_average", "interval_average", "point_unavailable"], support.kind)
        cellField.text = String(support.cell_index === undefined ? 0 : support.cell_index)
        startField.text = String(support.start_m === undefined ? 0 : support.start_m)
        endField.text = String(support.end_m === undefined ? 1 : support.end_m)
        acquisitionField.text = String(support.acquisition_window_s); delayField.text = String(support.delay_s)
        xField.text = String(sensor.marker.x); yField.text = String(sensor.marker.y); zField.text = String(sensor.marker.z)
    }
    function number(text, name) {
        if (text.trim().length === 0 || !isFinite(Number(text))) throw new Error("Enter a finite number for " + name)
        return Number(text)
    }
    function fields() {
        const result = {attachment: {kind: attachmentKind, id: attachmentBox.currentValue}, source_id: sourceField.text,
            enabled: enabledBox.checked, expected_period_s: number(periodField.text, "sampling period"), max_age_s: number(ageField.text, "maximum age"),
            uncertainty_bound: number(uncertaintyField.text, "uncertainty"), role: roleBox.currentText}
        if (labelField.text.length > 0 || !creating) result.label = labelField.text
        if (channelField.text.length > 0 || !creating) result.channel_id = channelField.text
        if (groupField.text.length > 0 || !creating) result.group_id = groupField.text
        if (quantity === "custom_numeric") result.unit = unitField.text
        if (minimumField.text.length > 0) result.instrument_min = number(minimumField.text, "instrument minimum")
        else if (!creating) result.instrument_min = null
        if (maximumField.text.length > 0) result.instrument_max = number(maximumField.text, "instrument maximum")
        else if (!creating) result.instrument_max = null
        const calibration = {calibration_id: calibrationIdField.text, version: number(calibrationVersionField.text, "calibration version"),
            kind: quantity === "level" ? "level_volume_piecewise_linear" : "identity", error_bound: number(calibrationErrorField.text, "calibration error bound")}
        if (quantity === "level") {
            calibration.knots = []; const lines = knotsField.text.trim().split(/\n/)
            for (let i = 0; i < lines.length; ++i) {
                const pair = lines[i].split(":")
                if (pair.length !== 2) throw new Error("Each level calibration line needs level_m:volume_m3")
                calibration.knots.push({level_m: number(pair[0], "level knot"), volume_m3: number(pair[1], "volume knot")})
            }
        }
        result.calibration = calibration
        const support = {kind: quantity === "concentration" ? supportBox.currentText : "none",
            acquisition_window_s: number(acquisitionField.text, "acquisition window"), delay_s: number(delayField.text, "measurement delay")}
        if (quantity === "concentration") {
            if (support.kind === "cell_average") support.cell_index = number(cellField.text, "cell index")
            else { support.start_m = number(startField.text, "pipe start"); support.end_m = support.kind === "point_unavailable" ? support.start_m : number(endField.text, "pipe end") }
        }
        result.physical_support = support
        return result
    }
    function apply() {
        submitting = true
        try {
            const entry = fields()
            const ok = creating ? controller.addSensor(quantity, attachmentBox.currentValue || "", entry)
                                : controller.configureSensor(controller.selectedSensorId, entry)
            if (ok) { creating = false; problem = ""; populate() }
            else problem = controller.summary
        } catch (error) { problem = String(error.message || error) }
        finally { submitting = false }
    }
    onOpened: { creating = controller.selectedSensorId.length === 0; if (creating) beginNew(); else populate() }
    Connections { target: panel.controller; function onChanged() {
        if (!panel.creating && !panel.submitting) {
            if (panel.controller.selectedSensorId.length > 0) panel.populate()
            else panel.beginNew()
        }
    } }
    contentItem: ColumnLayout {
        spacing: 12
        Label { Layout.fillWidth: true; text: "SYNTHETIC ONLY · Measurements stay separate from commands and native forecasts."; wrapMode: Text.WordWrap; color: Theme.secondary }
        RowLayout {
            Layout.fillWidth: true; spacing: 8
            ActionButton { text: "New sensor"; iconName: "gauge"; onClicked: panel.beginNew() }
            ActionButton { text: "Delete sensor"; iconName: "trash-2"; enabled: !panel.creating && controller.selectedSensorId.length > 0
                onClicked: { if (controller.deleteSensor(controller.selectedSensorId)) panel.beginNew(); else panel.problem = controller.summary } }
            ComboBox {
                Layout.fillWidth: true; Layout.minimumWidth: 140
                model: controller.sensorMarkers; textRole: "label"; valueRole: "id"
                displayText: currentIndex >= 0 ? currentText : (count ? "Select a saved sensor" : "No saved sensors")
                Accessible.name: "Saved sensor"
                currentIndex: {
                    const rows = controller.sensorMarkers
                    for (let i = 0; i < rows.length; ++i) if (rows[i].id === controller.selectedSensorId) return i
                    return -1
                }
                onActivated: { panel.creating = false; panel.problem = ""; controller.selectSensor(currentValue); panel.populate() }
            }
        }
        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.divider }
        ScrollView {
                id: formScroll; Layout.fillWidth: true; Layout.fillHeight: true; clip: true; contentWidth: availableWidth
                ScrollBar.vertical.policy: ScrollBar.AsNeeded
                ColumnLayout {
                    width: formScroll.availableWidth; spacing: 12
                    Label { text: panel.creating ? "PLACE A SENSOR" : "EDIT SENSOR"; color: Theme.secondary; font.pixelSize: Theme.smallSize; font.bold: true }
                    Label { Layout.fillWidth: true; text: panel.creating ? "A stable sensor ID is generated when placed." : "Sensor " + controller.selectedSensorId; textFormat: Text.PlainText; wrapMode: Text.WrapAnywhere; color: Theme.secondary }
                    GridLayout {
                        id: identityGrid
                        columns: formScroll.availableWidth < 600 ? 1 : 2; Layout.fillWidth: true; columnSpacing: 18; rowSpacing: 8
                        Label { text: "Quantity" }
                        ComboBox { id: quantityBox; Layout.fillWidth: true; model: panel.quantities; enabled: panel.creating }
                        Label { text: "Attachment kind"; visible: panel.genericAttachment }
                        ComboBox { id: attachmentKindBox; visible: panel.genericAttachment; Layout.fillWidth: true; model: ["asset", "route"] }
                        Label { text: "Equipment / route" }
                        ComboBox { id: attachmentBox; Layout.fillWidth: true; model: panel.attachmentChoices; textRole: "label"; valueRole: "id" }
                        Label { text: "Label" }
                        TextField { id: labelField; Layout.fillWidth: true; placeholderText: "Uses quantity and attachment when blank" }
                        Label { text: "Custom unit"; visible: panel.quantity === "custom_numeric" }
                        TextField { id: unitField; Layout.fillWidth: true; visible: panel.quantity === "custom_numeric" }
                        Label { text: "Source identity" }
                        TextField { id: sourceField; Layout.fillWidth: true }
                        Label { text: "Channel identity" }
                        TextField { id: channelField; Layout.fillWidth: true; placeholderText: "Generated from sensor ID when blank" }
                        Label { text: "Enabled" }
                        CheckBox { id: enabledBox }
                        Label { Layout.columnSpan: identityGrid.columns; Layout.topMargin: 8; text: "SAMPLING AND QUALITY"; color: Theme.secondary; font.pixelSize: Theme.captionSize; font.bold: true }
                        Label { text: "Sampling period (s)" }
                        TextField { id: periodField; Layout.fillWidth: true; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                        Label { text: "Maximum age (s)" }
                        TextField { id: ageField; Layout.fillWidth: true; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                        Label { text: "Uncertainty bound (" + panel.canonicalUnit + ")" }
                        TextField { id: uncertaintyField; Layout.fillWidth: true; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                        Label { Layout.columnSpan: identityGrid.columns; Layout.topMargin: 8; text: "REDUNDANCY AND INSTRUMENT RANGE"; color: Theme.secondary; font.pixelSize: Theme.captionSize; font.bold: true }
                        Label { text: "Redundancy group" }
                        TextField { id: groupField; Layout.fillWidth: true; placeholderText: "Independent group when blank" }
                        Label { text: "Group role" }
                        ComboBox { id: roleBox; Layout.fillWidth: true; model: ["primary", "redundant"] }
                        Label { text: "Instrument minimum" }
                        TextField { id: minimumField; Layout.fillWidth: true; placeholderText: "Optional; measurements are retained beyond this" }
                        Label { text: "Instrument maximum" }
                        TextField { id: maximumField; Layout.fillWidth: true; placeholderText: "Optional" }
                    }
                    Label { Layout.fillWidth: true; text: panel.quantity === "level" ? "Declare a level-volume table. Scene tank geometry supplies no physical calibration." : "Calibration metadata"; wrapMode: Text.WordWrap; color: Theme.secondary }
                    GridLayout {
                        columns: formScroll.availableWidth < 600 ? 1 : 2; Layout.fillWidth: true; columnSpacing: 18; rowSpacing: 8
                        Label { text: "Calibration ID" }
                        TextField { id: calibrationIdField; Layout.fillWidth: true }
                        Label { text: "Version" }
                        TextField { id: calibrationVersionField; Layout.fillWidth: true }
                        Label { text: "Calibration error bound" }
                        TextField { id: calibrationErrorField; Layout.fillWidth: true }
                    }
                    TextArea { id: knotsField; visible: panel.quantity === "level"; Layout.fillWidth: true; Layout.preferredHeight: 100
                        placeholderText: "At least two increasing level_m:volume_m3 lines\nUse independently declared physical calibration values."; wrapMode: TextEdit.NoWrap }
                    Label { visible: panel.quantity === "concentration"; Layout.fillWidth: true; wrapMode: Text.WordWrap
                        text: "Concentration requires declared pipe transport geometry. Point observations remain explicitly unavailable for mapping."; color: Theme.warning }
                    GridLayout {
                        columns: formScroll.availableWidth < 600 ? 1 : 2; Layout.fillWidth: true; columnSpacing: 18; rowSpacing: 8
                        Label { visible: panel.quantity === "concentration"; text: "Physical support" }
                        ComboBox { id: supportBox; visible: panel.quantity === "concentration"; Layout.fillWidth: true; model: ["cell_average", "interval_average", "point_unavailable"] }
                        Label { visible: panel.quantity === "concentration" && supportBox.currentText === "cell_average"; text: "Zero-based cell index" }
                        TextField { id: cellField; visible: panel.quantity === "concentration" && supportBox.currentText === "cell_average"; Layout.fillWidth: true }
                        Label { visible: panel.quantity === "concentration" && supportBox.currentText !== "cell_average"; text: "Physical start / point (m)" }
                        TextField { id: startField; visible: panel.quantity === "concentration" && supportBox.currentText !== "cell_average"; Layout.fillWidth: true }
                        Label { visible: panel.quantity === "concentration" && supportBox.currentText === "interval_average"; text: "Physical end (m)" }
                        TextField { id: endField; visible: panel.quantity === "concentration" && supportBox.currentText === "interval_average"; Layout.fillWidth: true }
                        Label { text: "Acquisition window (s)" }
                        TextField { id: acquisitionField; Layout.fillWidth: true }
                        Label { text: "Declared delay (s)" }
                        TextField { id: delayField; Layout.fillWidth: true }
                    }
                    Label { visible: panel.quantity === "pump_state"; Layout.fillWidth: true; wrapMode: Text.WordWrap; color: Theme.secondary; text: "Feedback distinguishes commanded-off, running, delivery loss, unavailable and unknown. It does not set pump commands." }
                    Label { visible: panel.genericAttachment; Layout.fillWidth: true; wrapMode: Text.WordWrap; color: Theme.secondary; text: "Display/history only. Native storage/transport physics does not predict this channel; detection integration is a later increment." }
                    Label { visible: !panel.creating; text: "Marker presentation coordinates (scene units)"; color: Theme.secondary }
                    RowLayout {
                        visible: !panel.creating; Layout.fillWidth: true
                        TextField { id: xField; Layout.fillWidth: true; placeholderText: "x" }
                        TextField { id: yField; Layout.fillWidth: true; placeholderText: "y" }
                        TextField { id: zField; Layout.fillWidth: true; placeholderText: "z" }
                        ActionButton { text: "Move marker"; onClicked: { try {
                            if (!controller.moveSensor(controller.selectedSensorId, panel.number(xField.text, "x"), panel.number(yField.text, "y"), panel.number(zField.text, "z")))
                                panel.problem = "Marker coordinates must lie within 10000 scene units."
                            else panel.problem = ""
                        } catch (error) { panel.problem = String(error.message || error) } } }
                    }
                }
        }
        ActionButton { highlighted: true; text: panel.creating ? "Place sensor" : "Save sensor configuration"; enabled: attachmentBox.count > 0; Layout.fillWidth: true; onClicked: panel.apply() }
        Label { Layout.fillWidth: true; visible: panel.problem.length > 0; text: panel.problem; textFormat: Text.PlainText; wrapMode: Text.WordWrap; color: Theme.warning }
    }
}

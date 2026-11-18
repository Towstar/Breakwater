pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Pane {
    id: inspector
    required property var controller
    property var numericRejections: ({})
    property bool pipeTransportReady: false
    signal numericCommitted(string field, string value)
    signal pipeTransportRequested()
    signal focusRequested()
    readonly property var selection: controller.selected
    readonly property var equipment: controller.assets
    readonly property var routes: controller.routes
    readonly property bool hasSelection: !!selection.kind
    readonly property var selectedRow: equipment.find(row => row.id === selection.id) || ({})
    objectName: "assetInspector"
    padding: 12
    background: Rectangle { color: Theme.surface }

    function assetLabel(id) {
        const row = equipment.find(asset => asset.id === id)
        return row ? row.label : id
    }
    function number(value) {
        return typeof value === "number" && isFinite(value) ? value.toLocaleString(Qt.locale(), "f", 2) : "unavailable"
    }
    component SectionHeading: Label {
        Layout.fillWidth: true; Layout.topMargin: 7
        color: Theme.secondary; font.pixelSize: Theme.captionSize; font.bold: true; font.letterSpacing: 1
    }

    contentItem: ScrollView {
        id: inspectorScroll
        clip: true; contentWidth: availableWidth
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        ScrollBar.vertical: ScrollBar {
            id: propertyScrollBar; objectName: "propertyInspectorScrollBar"
            policy: ScrollBar.AsNeeded; active: true; interactive: true
            width: 10; padding: 1; minimumSize: 0.12
            background: Rectangle { color: Theme.background; border.color: Theme.divider; radius: 4 }
            contentItem: Rectangle { implicitWidth: 8; implicitHeight: 32; radius: 3; color: propertyScrollBar.pressed ? Theme.text : propertyScrollBar.hovered ? Theme.secondary : Theme.controlBorder }
        }
        ColumnLayout {
            width: Math.max(0, inspectorScroll.availableWidth - 12); spacing: 8
            RowLayout {
                Layout.fillWidth: true
                Label { Layout.fillWidth: true; text: inspector.hasSelection ? "EQUIPMENT INSPECTOR" : "NETWORK OVERVIEW"; color: Theme.secondary; font.pixelSize: Theme.captionSize; font.letterSpacing: 1; font.bold: true }
                ActionButton { objectName: "inspectorFocus"; visible: inspector.hasSelection && inspector.selection.kind !== "route"; iconName: "focus"; display: AbstractButton.IconOnly; text: "Focus selected equipment"; onClicked: inspector.focusRequested() }
            }
            Pane {
                Layout.fillWidth: true; visible: inspector.hasSelection; padding: 10
                background: Rectangle { color: Theme.raised; radius: Theme.cornerRadius; border.color: Theme.divider }
                contentItem: ColumnLayout {
                    spacing: 5
                    RowLayout {
                        Layout.fillWidth: true; spacing: 8
                        Image { Layout.preferredWidth: 18; Layout.preferredHeight: 18; source: Theme.equipmentIcon(inspector.selection.kind || ""); sourceSize.width: 18; sourceSize.height: 18; Accessible.ignored: true }
                        Label { Layout.fillWidth: true; text: Theme.equipmentName(inspector.selection.kind || ""); color: Theme.secondary; font.pixelSize: Theme.smallSize; wrapMode: Text.WordWrap }
                    }
                    TextField {
                        objectName: "equipmentLabel"
                        Layout.fillWidth: true; visible: inspector.selection.kind !== "route"
                        text: inspector.selection.label || ""; font.weight: Font.DemiBold
                        Accessible.name: "Equipment label"; placeholderText: "Equipment label"
                        onEditingFinished: inspector.controller.editSelected("label", text)
                    }
                    Label { Layout.fillWidth: true; visible: inspector.selection.kind === "route"; text: inspector.selection.id || ""; textFormat: Text.PlainText; wrapMode: Text.WrapAnywhere; font.weight: Font.DemiBold }
                    Label { Layout.fillWidth: true; visible: inspector.selection.kind !== "route"; text: "ID  " + (inspector.selection.id || ""); textFormat: Text.PlainText; wrapMode: Text.WrapAnywhere; color: Theme.muted; font.family: Theme.monoFamily; font.pixelSize: Theme.captionSize }
                }
            }
            Label {
                Layout.fillWidth: true; visible: inspector.controller.selectedIds.length > 1
                text: "Primary of " + inspector.controller.selectedIds.length + " selected. These fields edit only this asset; use the scene Edit menu for group actions."
                wrapMode: Text.WordWrap; color: Theme.secondary; font.pixelSize: Theme.smallSize
            }
            Label { Layout.fillWidth: true; visible: !inspector.hasSelection; text: inspector.equipment.length + " assets · " + inspector.routes.length + " routes"; color: Theme.text; font.pixelSize: 18; wrapMode: Text.WordWrap }
            Label { Layout.fillWidth: true; visible: !inspector.hasSelection; text: "Select equipment in the scene or Network list to inspect its inputs. Ctrl-click adds to the selection."; wrapMode: Text.WordWrap; color: Theme.secondary; font.pixelSize: Theme.smallSize }
            Repeater {
                model: inspector.hasSelection ? [] : [
                    {kind:"tank",name:"Storage tanks",count:inspector.equipment.filter(row => row.kind === "tank").length},
                    {kind:"source",name:"Production sources",count:inspector.equipment.filter(row => row.kind === "source").length},
                    {kind:"treatment",name:"Treatment outlets",count:inspector.equipment.filter(row => row.kind === "treatment").length},
                    {kind:"trucking",name:"Tanker outlets",count:inspector.equipment.filter(row => row.kind === "trucking").length},
                    {kind:"pump",name:"Pumps on routes",count:inspector.routes.filter(row => row.pump).length}
                ]
                delegate: RowLayout {
                    required property var modelData
                    Layout.fillWidth: true; spacing: 8
                    Image { Layout.preferredWidth: 16; Layout.preferredHeight: 16; source: Theme.equipmentIcon(modelData.kind); sourceSize.width: 16; sourceSize.height: 16; Accessible.ignored: true }
                    Label { Layout.fillWidth: true; text: modelData.name; color: Theme.secondary; font.pixelSize: Theme.smallSize; wrapMode: Text.WordWrap }
                    Label { text: String(modelData.count); font.family: Theme.monoFamily; font.weight: Font.DemiBold }
                }
            }
            SectionHeading { visible: inspector.hasSelection; text: "SIMULATION INPUTS" }
            Repeater {
                model: {
                    const k = inspector.selection.kind
                    if (k === "tank") return [{key:"capacity",name:"Capacity",unit:"m³"},{key:"initial",name:"Initial inventory",unit:"m³"}]
                    if (k === "source") return [{key:"inflow",name:"Production",unit:"m³/h"},{key:"curtailment_max",name:"Max reduction",unit:"m³/h"},{key:"curtailment",name:"Reduction",unit:"m³/h"},{key:"cost",name:"Reduction cost",unit:"cost / m³"}]
                    if (k === "route") return inspector.selection.sourceDriven ? [{key:"capacity",name:"Route capacity",unit:"m³/h"}] : [{key:"capacity",name:"Route capacity",unit:"m³/h"},{key:"flow",name:"Setpoint",unit:"m³/h"},{key:"cost",name:"Route cost",unit:"cost / m³"}]
                    if (k === "treatment" || k === "trucking") return [{key:"capacity",name:"Throughput",unit:"m³/h"},{key:"cost",name:"Cost",unit:"cost / m³"}]
                    return []
                }
                delegate: ColumnLayout {
                    id: numericRow
                    required property var modelData
                    readonly property var rejection: inspector.numericRejections[modelData.key] || ({})
                    Layout.fillWidth: true; spacing: 3
                    RowLayout {
                        Layout.fillWidth: true
                        Label { Layout.fillWidth: true; text: numericRow.modelData.name; color: numericRow.rejection.message ? Theme.error : Theme.secondary; font.pixelSize: Theme.smallSize; wrapMode: Text.WordWrap }
                        Label { text: numericRow.modelData.unit; color: Theme.muted; font.pixelSize: Theme.captionSize }
                    }
                    TextField {
                        objectName: "numericProperty_" + numericRow.modelData.key
                        property bool pendingUserCommit: false
                        Layout.fillWidth: true; text: String(inspector.selection[numericRow.modelData.key] ?? "")
                        inputMethodHints: Qt.ImhFormattedNumbersOnly
                        Accessible.name: numericRow.modelData.name + " (" + numericRow.modelData.unit + ")"
                        Accessible.description: numericRow.rejection.message || ""
                        onTextEdited: pendingUserCommit = true
                        Keys.onReturnPressed: function(event) { pendingUserCommit = true; event.accepted = false }
                        Keys.onEnterPressed: function(event) { pendingUserCommit = true; event.accepted = false }
                        onEditingFinished: {
                            if (!pendingUserCommit) return
                            pendingUserCommit = false
                            inspector.numericCommitted(numericRow.modelData.key, text)
                        }
                    }
                    Label {
                        objectName: "numericRejection_" + numericRow.modelData.key
                        Layout.fillWidth: true; visible: !!numericRow.rejection.message
                        text: "Not applied: “" + String(numericRow.rejection.input || "") + "”. "
                            + numericRow.rejection.message + "\nAccepted value: "
                            + String(inspector.selection[numericRow.modelData.key] ?? "unavailable")
                        textFormat: Text.PlainText; wrapMode: Text.WrapAnywhere; color: Theme.error; font.pixelSize: Theme.bodySize
                    }
                }
            }
            Label { visible: inspector.selection.kind === "route" && !!inspector.selection.sourceDriven; Layout.fillWidth: true; text: "Source-route flow follows production minus reduction."; wrapMode: Text.WordWrap; color: Theme.secondary; font.pixelSize: Theme.smallSize }
            Pane {
                Layout.fillWidth: true; visible: inspector.selection.kind === "tank"; padding: 10
                background: Rectangle { color: Theme.raised; radius: Theme.cornerRadius; border.color: inspector.selectedRow.violated ? Theme.error : Theme.divider }
                contentItem: ColumnLayout {
                    spacing: 4
                    Label { text: inspector.controller.runId.length ? "FORECAST INVENTORY" : "INITIAL INVENTORY"; color: Theme.secondary; font.pixelSize: Theme.captionSize; font.bold: true }
                    Label { objectName: "inspectorInventory"; text: inspector.number(inspector.selectedRow.volume) + " m³"; color: inspector.selectedRow.violated ? Theme.error : Theme.text; font.family: Theme.monoFamily; font.pixelSize: 19 }
                    Label { Layout.fillWidth: true; text: (inspector.controller.runId.length ? "At " + inspector.number(inspector.controller.forecastTimeH) + " h · " : "") + "Capacity " + inspector.number(inspector.selection.capacity) + " m³"; wrapMode: Text.WordWrap; color: Theme.secondary; font.pixelSize: Theme.smallSize }
                }
            }
            SectionHeading { visible: inspector.selection.kind === "route"; text: "ROUTE & PUMP" }
            Label { Layout.fillWidth: true; visible: inspector.selection.kind === "route"; text: "From  " + inspector.assetLabel(inspector.selection.source || "") + "\nTo       " + inspector.assetLabel(inspector.selection.target || ""); textFormat: Text.PlainText; wrapMode: Text.WrapAnywhere; color: Theme.secondary; font.pixelSize: Theme.smallSize }
            RowLayout {
                visible: !!inspector.selection.pump; Layout.fillWidth: true; spacing: 7
                Rectangle { Layout.preferredWidth: 6; Layout.preferredHeight: 6; radius: 3; color: inspector.selection.available ? Theme.success : Theme.error }
                Label { objectName: "inspectorPumpState"; Layout.fillWidth: true; text: inspector.selection.available ? "Pump available" : "Pump unavailable"; color: inspector.selection.available ? Theme.success : Theme.error; font.pixelSize: Theme.smallSize }
            }
            ActionButton { iconName: "gauge"; Layout.fillWidth: true; visible: inspector.selection.kind === "route" && !inspector.selection.sourceDriven && !inspector.selection.pump; text: "Attach pump"; onClicked: inspector.controller.attachPump(inspector.controller.selectedId) }
            CheckBox { visible: !!inspector.selection.pump; checked: !!inspector.selection.available; text: "Pump available"; onClicked: inspector.controller.editSelected("available", checked ? "true" : "false") }
            ActionButton { iconName: "triangle-alert"; Layout.fillWidth: true; visible: !!inspector.selection.pump; enabled: !!inspector.selection.available; text: "Fail selected pump"; onClicked: inspector.controller.failSelectedPump() }
            ActionButton { objectName: "inspectPipeTransport"; iconName: "waves"; Layout.fillWidth: true; visible: inspector.selection.kind === "route"; enabled: inspector.pipeTransportReady; text: "Inspect pipe transport"; onClicked: inspector.pipeTransportRequested() }
            Label { Layout.fillWidth: true; visible: inspector.hasSelection; text: "Numerical edits require a new forecast."; wrapMode: Text.WordWrap; color: Theme.muted; font.pixelSize: Theme.captionSize }
            ActionButton { iconName: "trash-2"; Layout.fillWidth: true; visible: inspector.hasSelection; text: "Delete selected"; tooltip: inspector.controller.selectedIds.length > 1 ? "Delete all " + inspector.controller.selectedIds.length + " selected assets" : "Delete selected equipment or route"; onClicked: inspector.controller.deleteSelected() }
            SectionHeading { text: "NETWORK CHECKS" }
            Pane {
                Layout.fillWidth: true; padding: 10
                background: Rectangle { color: Theme.raised; radius: Theme.cornerRadius; border.color: Theme.divider }
                contentItem: ColumnLayout {
                    spacing: 6
                    RowLayout {
                        Layout.fillWidth: true; spacing: 7
                        Image { Layout.preferredWidth: 16; Layout.preferredHeight: 16; source: Theme.icon(inspector.controller.draftIssues.length ? "triangle-alert" : "check"); sourceSize.width: 16; sourceSize.height: 16; Accessible.ignored: true }
                        Label { Layout.fillWidth: true; text: inspector.controller.draftIssues.length ? inspector.controller.draftIssues.length + " draft issues" : "Ready to forecast"; color: inspector.controller.draftIssues.length ? Theme.warning : Theme.text; wrapMode: Text.WordWrap; font.weight: Font.DemiBold }
                    }
                    Label { Layout.fillWidth: true; visible: !inspector.controller.draftIssues.length; text: "No draft issues. Run a forecast to assess capacity constraints."; wrapMode: Text.WordWrap; color: Theme.secondary; font.pixelSize: Theme.smallSize }
                    Repeater {
                        model: inspector.controller.draftIssues
                        delegate: Label { required property string modelData; Layout.fillWidth: true; text: modelData; textFormat: Text.PlainText; wrapMode: Text.WrapAnywhere; font.pixelSize: Theme.smallSize; color: Theme.warning }
                    }
                }
            }
        }
    }
}

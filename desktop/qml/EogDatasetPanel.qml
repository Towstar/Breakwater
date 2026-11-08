import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Dialog {
    id: panel
    required property var eog
    readonly property bool datasetSceneReady: eog.loaded && network.sceneReady
    readonly property bool historicalCursor: eog.loaded && eog.cursorUnix < eog.nowUnix
    parent: Overlay.overlay
    anchors.centerIn: parent
    title: "EOG · Recorded network console"
    modal: true
    width: Math.min(1250, parent ? parent.width - 24 : 1250)
    height: Math.min(920, parent ? parent.height - 24 : 920)
    standardButtons: Dialog.Close
    function utc(seconds) {
        if (!eog.loaded) return "—"
        const date = new Date(seconds * 1000)
        return isNaN(date.getTime()) ? "Unavailable" : date.toISOString().replace("T", " ").replace(".000Z", " UTC")
    }
    function indexFor(rows, id) { for (let i = 0; i < rows.length; ++i) if (rows[i].id === id) return i; return -1 }
    function number(value, digits) { return typeof value === "number" && isFinite(value) ? value.toLocaleString(Qt.locale(), "f", digits) : "unavailable" }
    component DatasetChoice: ComboBox {
        Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.preferredWidth: 180
        textRole: "label"; valueRole: "id"
        contentItem: Label {
            text: parent.displayText; textFormat: Text.PlainText
            elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter
        }
        delegate: ItemDelegate {
            required property var modelData
            width: ListView.view ? ListView.view.width : 180
            text: modelData.label
            contentItem: Label { text: parent.text; textFormat: Text.PlainText; elide: Text.ElideRight }
        }
    }
    FolderDialog {
        id: folder
        title: "Choose the folder containing network.json, ops.json and telemetry.csv"
        onAccepted: panel.eog.loadFolder(selectedFolder)
    }
    contentItem: ScrollView {
        id: scroll
        clip: true
        contentWidth: availableWidth
        ColumnLayout {
            width: scroll.availableWidth
            spacing: 10
            RowLayout {
                Layout.fillWidth: true
                Button { objectName: "eogOpenFolder"; text: "Open dataset folder"; onClicked: folder.open() }
                Button { text: "Cancel load"; visible: panel.eog.busy; onClicked: panel.eog.cancel() }
                BusyIndicator { running: panel.eog.busy; visible: running; Layout.preferredWidth: 28; Layout.preferredHeight: 28 }
                Label { Layout.fillWidth: true; text: panel.eog.status; textFormat: Text.PlainText; wrapMode: Text.Wrap; color: "#b6ced9" }
            }
            Label {
                Layout.fillWidth: true; visible: text.length > 0
                text: panel.eog.error; textFormat: Text.PlainText; wrapMode: Text.Wrap; color: "#f4b47d"
            }
            Label {
                Layout.fillWidth: true
                text: "Read-only EOG data · Raw sensor readings and operating inputs. Event detection, shutdown countdowns and truck/well plans are not connected yet."
                color: "#e5c78d"; wrapMode: Text.Wrap
            }
            Label {
                Layout.fillWidth: true; visible: panel.eog.loaded
                text: panel.eog.sourceFolder + "\nDataset " + panel.eog.datasetIdentity.substring(0, 16) + " · inputs: network.json, ops.json, telemetry.csv"
                textFormat: Text.PlainText; wrapMode: Text.WrapAnywhere; font.pixelSize: 11; color: "#91aebb"
            }
            GridLayout {
                Layout.fillWidth: true
                visible: panel.eog.loaded
                columns: 4; columnSpacing: 10
                Label { text: "System" }
                Label { text: "Facility" }
                Label { text: "Equipment" }
                Label { text: "Measurement" }
                DatasetChoice {
                    id: systemChoice
                    model: panel.eog.systems
                    currentIndex: panel.indexFor(panel.eog.systems, panel.eog.systemId)
                    onActivated: panel.eog.selectSystem(currentValue)
                }
                DatasetChoice {
                    id: facilityChoice
                    model: panel.eog.facilities
                    currentIndex: panel.indexFor(panel.eog.facilities, panel.eog.facilityId)
                    onActivated: panel.eog.selectFacility(currentValue)
                }
                DatasetChoice {
                    id: assetChoice
                    model: panel.eog.assets
                    currentIndex: panel.indexFor(panel.eog.assets, panel.eog.assetId)
                    onActivated: panel.eog.selectAsset(currentValue)
                }
                DatasetChoice {
                    id: metricChoice
                    model: panel.eog.metrics
                    currentIndex: panel.indexFor(panel.eog.metrics, panel.eog.metric)
                    onActivated: panel.eog.selectMetric(currentValue)
                }
            }
            RowLayout {
                Layout.fillWidth: true; visible: panel.eog.loaded
                Label { Layout.fillWidth: true; text: "Dataset now: " + panel.utc(panel.eog.nowUnix); color: "#a4c2cf"; wrapMode: Text.Wrap }
                Button { text: "View dataset now"; enabled: panel.historicalCursor; onClicked: panel.eog.setCursorUnix(panel.eog.nowUnix) }
            }
            RowLayout {
                Layout.fillWidth: true; visible: panel.eog.loaded
                Label { text: panel.historicalCursor ? "Historical cursor" : "Latest recorded time"; color: panel.historicalCursor ? "#e5c78d" : "#65d1ce" }
                Slider {
                    id: timeCursor; Layout.fillWidth: true
                    from: panel.eog.firstUnix; to: Math.max(from, panel.eog.nowUnix)
                    stepSize: 300; value: panel.eog.cursorUnix
                    enabled: panel.eog.nowUnix > panel.eog.firstUnix
                    onMoved: panel.eog.setCursorUnix(value)
                }
                Label { text: panel.utc(panel.eog.cursorUnix); font.pixelSize: 11 }
            }
            EogNetworkView {
                id: network
                eog: panel.eog
                visible: panel.eog.loaded
                Layout.fillWidth: true; Layout.preferredHeight: panel.width < 1150 ? 240 : 290
            }
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: measurementText.implicitHeight + 22
                visible: panel.eog.loaded
                radius: 5; color: "#1a303f"; border.color: "#375361"
                Label {
                    id: measurementText
                    anchors.fill: parent; anchors.margins: 11
                    text: {
                        const row = panel.eog.selectedValues
                        if (!row.available) return "No " + panel.eog.metric + " reading at or before this time. Missing values are not replaced by zero."
                        const value = panel.number(row.value, panel.eog.metric === "status" ? 0 : 2)
                        return value + " " + (row.unit || panel.eog.unit) + (row.status_label ? " · " + row.status_label : "")
                            + "\nRecorded " + panel.utc(row.event_unix) + " · age at cursor " + panel.number(row.age_s, 0) + " s"
                    }
                    textFormat: Text.PlainText; wrapMode: Text.Wrap; color: "#dceaf0"
                }
            }
            EogTrendChart {
                Layout.fillWidth: true; Layout.preferredHeight: 195
                visible: panel.eog.loaded
                samples: panel.eog.selectedSeries; unit: panel.eog.unit
                discrete: panel.eog.metric === "status"
            }
            Label {
                Layout.fillWidth: true; visible: panel.eog.loaded
                text: "Trend uses observations at or before the cursor. Gaps are shown without zero filling. Pump feedback: 0 stopped, 1 running, 2 fault; an ordinary stop is not an inferred failure event."
                wrapMode: Text.Wrap; color: "#92aebb"; font.pixelSize: 11
            }
            Frame {
                Layout.fillWidth: true; visible: panel.eog.loaded
                background: Rectangle { color: "#172b39"; radius: 5; border.color: "#365261" }
                ColumnLayout {
                    anchors.fill: parent
                    Label { text: "OPERATING INPUTS · from ops.json"; color: "#65cfcc"; font.pixelSize: 11 }
                    Label {
                        Layout.fillWidth: true
                        text: {
                            const ops = panel.eog.opsSummary
                            return "System fleet: " + panel.number(ops.fleet_size, 0) + " trucks · " + panel.number(ops.truck_capacity_bbl, 0) + " bbl per trip · $" + panel.number(ops.cost_per_trip, 2) + " per trip"
                                + "\nFacility first response: " + panel.number(ops.response_min, 0) + " min · round trip: " + panel.number(ops.cycle_min, 0) + " min · pump repair: " + panel.number(ops.repair_hours, 2) + " h"
                                + "\nOil: $" + panel.number(ops.oil_per_bbl, 2) + "/bbl · gas: $" + panel.number(ops.gas_per_mcf, 2) + "/mcf. Shared-fleet scheduling and lost-revenue planning require the separate numerical adapter."
                        }
                        textFormat: Text.PlainText; wrapMode: Text.Wrap; color: "#b8ced7"
                    }
                }
            }
            Repeater {
                model: panel.eog.warnings
                delegate: Label {
                    required property string modelData
                    Layout.fillWidth: true; text: modelData; textFormat: Text.PlainText
                    wrapMode: Text.Wrap; font.pixelSize: 11; color: "#e3bf85"
                }
            }
        }
    }
}

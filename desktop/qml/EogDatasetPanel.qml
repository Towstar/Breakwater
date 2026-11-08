import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Pane {
    id: panel
    required property var eog
    readonly property bool datasetSceneReady: eog.loaded && network.sceneReady
    readonly property int visibleSceneLabels: network.visibleSceneLabels
    readonly property bool datasetTrendReady: selectedTrend.chartReady
    readonly property int renderedTrendSamples: selectedTrend.renderedSampleCount
    readonly property int renderedTrendGuides: selectedTrend.renderedGuideCount
    readonly property bool historicalCursor: eog.loaded && eog.cursorUnix < eog.nowUnix
    objectName: "eogDatasetPanel"
    padding: 10
    background: Rectangle { color: Theme.background }
    function utc(seconds) {
        if (!eog.loaded) return "—"
        const date = new Date(seconds * 1000)
        return isNaN(date.getTime()) ? "Unavailable" : date.toISOString().replace("T", " ").replace(".000Z", " UTC")
    }
    function indexFor(rows, id) {
        for (let i = 0; i < rows.length; ++i) if (rows[i].id === id) return i
        return -1
    }
    function number(value, digits) {
        return typeof value === "number" && isFinite(value) ? value.toLocaleString(Qt.locale(), "f", digits) : "unavailable"
    }
    function measurementDescription() {
        const row = eog.selectedValues
        if (!row.has_sample) return eog.metric.length > 0
            ? "No " + eog.metric + " reading at or before the cutoff. Missing is not zero."
            : "No sensor channel on this equipment."
        return number(row.value, eog.metric === "status" ? 0 : 2) + " " + (row.unit || eog.unit)
            + (row.status_label ? " · " + row.status_label : "")
            + " · " + (row.freshness_label || (row.stale ? "stale" : "recorded"))
            + "\nRecorded " + utc(row.event_unix) + " · age " + number(row.age_s, 0) + " s"
            + (row.stale ? "\nLast value retained as evidence; unavailable as a fresh measurement." : "")
    }
    component DatasetChoice: ComboBox {
        id: choice
        Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.preferredWidth: 180
        textRole: "label"; valueRole: "id"
        contentItem: Label {
            text: choice.displayText; textFormat: Text.PlainText
            elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter
        }
        delegate: ItemDelegate {
            id: choiceRow
            required property var modelData
            width: ListView.view ? ListView.view.width : 180
            text: modelData.label
            contentItem: Label { text: choiceRow.text; textFormat: Text.PlainText; elide: Text.ElideRight }
        }
    }
    FolderDialog {
        id: folder
        title: "Choose the folder containing network.json, ops.json and telemetry.csv"
        onAccepted: panel.eog.loadFolder(selectedFolder)
    }
    Popup {
        id: details
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(760, parent ? parent.width - 32 : 760)
        height: Math.min(540, parent ? parent.height - 32 : 540)
        padding: 16
        modal: true; closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: Rectangle { color: Theme.raised; border.color: Theme.divider; radius: 5 }
        contentItem: ScrollView {
            id: detailsScroll
            clip: true; contentWidth: availableWidth
            ColumnLayout {
                width: detailsScroll.availableWidth; spacing: 12
                RowLayout {
                    Layout.fillWidth: true
                    Label { Layout.fillWidth: true; text: "DATASET & OPERATING INPUTS"; color: Theme.text; font.bold: true }
                    ActionButton { iconName: "x"; text: "Close"; onClicked: details.close() }
                }
                Label {
                    Layout.fillWidth: true
                    text: panel.eog.status
                    textFormat: Text.PlainText; wrapMode: Text.Wrap; color: Theme.secondary
                }
                Label {
                    Layout.fillWidth: true
                    text: "Read-only recorded observations. Run event detection and response analysis explicitly in Investigate / history when a numerical provider is available. Loading a dataset does not run an investigation or apply controls."
                    wrapMode: Text.Wrap; color: Theme.warning
                }
                Label {
                    Layout.fillWidth: true
                    text: panel.eog.sourceFolder + "\nDataset " + panel.eog.datasetIdentity
                        + "\nOnly network.json, ops.json and telemetry.csv are loaded."
                        + "\nDataset window: " + panel.utc(panel.eog.firstUnix) + " — " + panel.utc(panel.eog.nowUnix)
                    textFormat: Text.PlainText; wrapMode: Text.WrapAnywhere; font.pixelSize: Theme.smallSize; color: Theme.muted
                }
                Label {
                    Layout.fillWidth: true; visible: panel.eog.loaded
                    text: {
                        const ops = panel.eog.opsSummary
                        return "System fleet: " + panel.number(ops.fleet_size, 0) + " trucks · "
                            + panel.number(ops.truck_capacity_bbl, 0) + " bbl per trip · $" + panel.number(ops.cost_per_trip, 2) + " per trip"
                            + "\nFacility first response: " + panel.number(ops.response_min, 0) + " min · round trip: "
                            + panel.number(ops.cycle_min, 0) + " min · pump repair: " + panel.number(ops.repair_hours, 2) + " h"
                            + "\nOil: $" + panel.number(ops.oil_per_bbl, 2) + "/bbl · gas: $" + panel.number(ops.gas_per_mcf, 2) + "/mcf."
                    }
                    textFormat: Text.PlainText; wrapMode: Text.Wrap; color: Theme.secondary
                }
                Label {
                    Layout.fillWidth: true
                    text: "Five-minute sampling. Source gaps stay visible without zero filling. Pump feedback: 0 stopped, 1 running, 2 fault; a normal stop is not a detected failure. Layout geometry supplies no flow or travel-time calculation."
                    wrapMode: Text.Wrap; color: Theme.muted; font.pixelSize: Theme.smallSize
                }
                Repeater {
                    model: panel.eog.warnings
                    delegate: Label {
                        required property string modelData
                        Layout.fillWidth: true; text: modelData; textFormat: Text.PlainText
                        wrapMode: Text.Wrap; color: Theme.warning
                    }
                }
            }
        }
    }
    contentItem: ScrollView {
        id: scroll
        clip: true; contentWidth: availableWidth
        ColumnLayout {
            width: scroll.availableWidth; spacing: 8
            ColumnLayout {
                id: dashboardHeader
                Layout.fillWidth: true; spacing: 6
                RowLayout {
                    Layout.fillWidth: true; spacing: 8
                    Label {
                        Layout.fillWidth: true
                        text: panel.eog.loaded ? "Recorded network" : "Open an EOG dataset to begin"
                        font.pixelSize: 18; font.bold: true; color: Theme.text; elide: Text.ElideRight
                    }
                    ActionButton { iconName: "folder-open"; objectName: "eogOpenFolder"; text: "Open dataset"; onClicked: folder.open() }
                    ActionButton { iconName: "square"; text: "Cancel"; visible: panel.eog.busy; onClicked: panel.eog.cancel() }
                    BusyIndicator { running: panel.eog.busy; visible: running; Layout.preferredWidth: 26; Layout.preferredHeight: 26 }
                    ActionButton { iconName: "clipboard-check"; objectName: "eogDatasetDetails"; text: "Dataset details"; onClicked: details.open() }
                }
                Label {
                    Layout.fillWidth: true
                    text: panel.eog.busy || !panel.eog.loaded ? panel.eog.status
                        : "Read-only · dataset " + panel.eog.datasetIdentity.substring(0, 12)
                            + " · choose a cutoff, then investigate explicitly"
                            + (panel.eog.warnings.length > 0 ? " · coverage warnings in details" : "")
                    textFormat: Text.PlainText; wrapMode: Text.Wrap; color: Theme.warning; font.pixelSize: Theme.captionSize
                }
                Label {
                    Layout.fillWidth: true; visible: text.length > 0
                    text: panel.eog.error
                    textFormat: Text.PlainText; wrapMode: Text.Wrap; color: Theme.error
                }
                GridLayout {
                    Layout.fillWidth: true; visible: panel.eog.loaded
                    columns: panel.width < 650 ? 2 : 4
                    columnSpacing: 10; rowSpacing: 4
                    ColumnLayout {
                        Layout.fillWidth: true; Layout.minimumWidth: 0; spacing: 2
                        Label { text: "System"; font.pixelSize: Theme.captionSize; color: Theme.secondary }
                        DatasetChoice {
                            model: panel.eog.systems
                            currentIndex: panel.indexFor(panel.eog.systems, panel.eog.systemId)
                            onActivated: panel.eog.selectSystem(currentValue)
                        }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true; Layout.minimumWidth: 0; spacing: 2
                        Label { text: "Facility"; font.pixelSize: Theme.captionSize; color: Theme.secondary }
                        DatasetChoice {
                            model: panel.eog.facilities
                            currentIndex: panel.indexFor(panel.eog.facilities, panel.eog.facilityId)
                            onActivated: panel.eog.selectFacility(currentValue)
                        }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true; Layout.minimumWidth: 0; spacing: 2
                        Label { text: "Equipment"; font.pixelSize: Theme.captionSize; color: Theme.secondary }
                        DatasetChoice {
                            model: panel.eog.assets
                            currentIndex: panel.indexFor(panel.eog.assets, panel.eog.assetId)
                            onActivated: panel.eog.selectAsset(currentValue)
                        }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true; Layout.minimumWidth: 0; spacing: 2
                        Label { text: "Measurement"; font.pixelSize: Theme.captionSize; color: Theme.secondary }
                        DatasetChoice {
                            model: panel.eog.metrics
                            currentIndex: panel.indexFor(panel.eog.metrics, panel.eog.metric)
                            onActivated: panel.eog.selectMetric(currentValue)
                        }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true; visible: panel.eog.loaded; spacing: 8
                    ActionButton { iconName: "network"; objectName: "eogOverview"; text: "All systems"; enabled: panel.eog.sceneMode !== "overview"; onClicked: panel.eog.showOverview() }
                    ActionButton { iconName: "factory"; objectName: "eogFacility"; text: "Facility"; enabled: panel.eog.sceneMode !== "facility"; onClicked: panel.eog.showFacility(panel.eog.facilityId) }
                    Label {
                        Layout.preferredWidth: panel.width < 1000 ? 170 : 235
                        text: (panel.historicalCursor ? "Historical: " : "Cutoff: ") + panel.utc(panel.eog.cursorUnix)
                        color: panel.historicalCursor ? Theme.warning : Theme.text
                        wrapMode: Text.Wrap; font.pixelSize: Theme.captionSize
                    }
                    Slider {
                        Layout.fillWidth: true; Layout.minimumWidth: 80
                        from: panel.eog.firstUnix; to: Math.max(from, panel.eog.nowUnix)
                        stepSize: 300; value: panel.eog.cursorUnix
                        enabled: panel.eog.nowUnix > panel.eog.firstUnix
                        onMoved: panel.eog.setCursorUnix(value)
                        Accessible.name: "Recorded observation cutoff"
                    }
                    ActionButton { iconName: "history"; text: "Now"; enabled: panel.historicalCursor; onClicked: panel.eog.setCursorUnix(panel.eog.nowUnix) }
                }
            }
            GridLayout {
                id: evidenceGrid
                Layout.fillWidth: true; visible: panel.eog.loaded
                Layout.preferredHeight: Math.max(300, Math.min(540, panel.availableHeight - dashboardHeader.implicitHeight - 12))
                columns: panel.width < 900 ? 1 : 2
                columnSpacing: 10; rowSpacing: 8
                EogNetworkView {
                    id: network
                    eog: panel.eog
                    Layout.fillWidth: true; Layout.fillHeight: true
                    Layout.preferredWidth: 620; Layout.minimumHeight: 200
                }
                ColumnLayout {
                    Layout.fillWidth: true; Layout.fillHeight: true
                    Layout.preferredWidth: 450; Layout.minimumWidth: 0
                    spacing: 6
                    Label {
                        Layout.fillWidth: true
                        text: panel.eog.assetId + " · " + panel.eog.metric
                        textFormat: Text.PlainText; color: Theme.secondary; font.pixelSize: Theme.smallSize
                        elide: Text.ElideMiddle
                    }
                    EogTrendChart {
                        id: selectedTrend
                        Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumHeight: 160
                        samples: panel.eog.selectedSeries; unit: panel.eog.unit
                        guides: panel.eog.selectedThresholds
                        discrete: panel.eog.metric === "status"
                    }
                    Rectangle {
                        Layout.fillWidth: true; implicitHeight: measurementText.implicitHeight + 18
                        radius: 5; color: Theme.raised; border.color: Theme.divider
                        Label {
                            id: measurementText
                            anchors.fill: parent; anchors.margins: 9
                            text: panel.measurementDescription()
                            textFormat: Text.PlainText; wrapMode: Text.Wrap; font.pixelSize: Theme.smallSize
                            color: panel.eog.selectedValues.stale ? Theme.warning : Theme.text
                        }
                    }
                    Label {
                        Layout.fillWidth: true
                        text: "Observed through cutoff · gaps preserved · original units"
                        wrapMode: Text.Wrap; color: Theme.muted; font.pixelSize: Theme.captionSize
                    }
                }
            }
        }
    }
}

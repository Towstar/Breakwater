import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Pane {
    id: panel
    required property var eog
    property int renderQuality: 1
    property bool condensed: false
    property url sampleDatasetPath: ""
    readonly property var reading: eog.selectedValues
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
    component DatasetChoice: AppComboBox {
        Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.preferredWidth: 180
        textRole: "label"; valueRole: "id"
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
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        ScrollBar.vertical: ScrollBar {
            id: observationScrollBar; objectName: "eogWorkspaceScrollBar"
            parent: scroll
            x: scroll.mirrored ? 0 : scroll.width - width
            y: scroll.topPadding; height: scroll.availableHeight
            visible: size < 1.0; policy: ScrollBar.AsNeeded; active: true; interactive: true
            width: 10; padding: 1; minimumSize: 0.12
            Accessible.name: "Operations workspace scroll"
            background: Rectangle { color: Theme.background; border.color: Theme.divider; radius: 4 }
            contentItem: Rectangle { implicitWidth: 8; implicitHeight: 32; radius: 3; color: observationScrollBar.pressed ? Theme.text : observationScrollBar.hovered ? Theme.secondary : Theme.controlBorder }
        }
        ColumnLayout {
            x: scroll.mirrored ? 12 : 0
            width: Math.max(0, scroll.availableWidth - 12)
            height: Math.max(implicitHeight, scroll.availableHeight)
            spacing: 12
            ColumnLayout {
                id: dashboardHeader; visible: !panel.condensed
                Layout.fillWidth: true; spacing: 6
                RowLayout {
                    Layout.fillWidth: true; spacing: 8
                    Label {
                        Layout.fillWidth: true
                        text: panel.eog.loaded ? "Recorded network" : "Operations workspace"
                        font.pixelSize: 20; font.weight: Font.DemiBold; color: Theme.text; elide: Text.ElideRight
                    }
                    ActionButton { flat: true; iconName: "folder-open"; objectName: "eogOpenFolder"; text: "Open dataset"; onClicked: folder.open() }
                    ActionButton { iconName: "square"; text: "Cancel"; visible: panel.eog.busy; onClicked: panel.eog.cancel() }
                    BusyIndicator { running: panel.eog.busy; visible: running; Layout.preferredWidth: 26; Layout.preferredHeight: 26 }
                    ActionButton { flat: true; iconName: "clipboard-check"; objectName: "eogDatasetDetails"; text: "Dataset details"; onClicked: details.open() }
                }
                Label {
                    Layout.fillWidth: true
                    text: panel.eog.busy || !panel.eog.loaded ? panel.eog.status
                        : "Read-only · dataset " + panel.eog.datasetIdentity.substring(0, 12)
                            + " · original units · choose a cutoff before investigating"
                            + (panel.eog.warnings.length > 0 ? " · coverage warnings in details" : "")
                    textFormat: Text.PlainText; wrapMode: Text.Wrap
                    color: panel.eog.warnings.length > 0 ? Theme.warning : Theme.secondary
                    font.pixelSize: Theme.captionSize
                }
                Label {
                    Layout.fillWidth: true; visible: text.length > 0
                    text: panel.eog.error
                    textFormat: Text.PlainText; wrapMode: Text.Wrap; color: Theme.error
                }
                Pane {
                    Layout.fillWidth: true; visible: panel.eog.loaded; padding: 12
                    background: Rectangle { color: Theme.surface; radius: Theme.cornerRadius; border.color: Theme.divider }
                    contentItem: GridLayout {
                        columns: panel.width < 650 ? 2 : 4
                        columnSpacing: 12; rowSpacing: 8
                        ColumnLayout {
                            Layout.fillWidth: true; Layout.minimumWidth: 0; spacing: 4
                            Label { text: "System"; font.pixelSize: Theme.smallSize; color: Theme.secondary }
                            DatasetChoice {
                                objectName: "eogSystemChoice"
                                Accessible.name: "System"
                                model: panel.eog.systems
                                currentIndex: panel.indexFor(panel.eog.systems, panel.eog.systemId)
                                onActivated: panel.eog.selectSystem(currentValue)
                            }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true; Layout.minimumWidth: 0; spacing: 4
                            Label { text: "Facility"; font.pixelSize: Theme.smallSize; color: Theme.secondary }
                            DatasetChoice {
                                objectName: "eogFacilityChoice"
                                Accessible.name: "Facility"
                                model: panel.eog.facilities
                                currentIndex: panel.indexFor(panel.eog.facilities, panel.eog.facilityId)
                                onActivated: panel.eog.selectFacility(currentValue)
                            }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true; Layout.minimumWidth: 0; spacing: 4
                            Label { text: "Equipment"; font.pixelSize: Theme.smallSize; color: Theme.secondary }
                            DatasetChoice {
                                objectName: "eogAssetChoice"
                                Accessible.name: "Equipment"
                                model: panel.eog.assets
                                currentIndex: panel.indexFor(panel.eog.assets, panel.eog.assetId)
                                onActivated: panel.eog.selectAsset(currentValue)
                            }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true; Layout.minimumWidth: 0; spacing: 4
                            Label { text: "Measurement"; font.pixelSize: Theme.smallSize; color: Theme.secondary }
                            DatasetChoice {
                                objectName: "eogMetricChoice"
                                Accessible.name: "Measurement"
                                model: panel.eog.metrics
                                currentIndex: panel.indexFor(panel.eog.metrics, panel.eog.metric)
                                onActivated: panel.eog.selectMetric(currentValue)
                            }
                        }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true; visible: panel.eog.loaded; spacing: 8
                    ActionButton { flat: true; iconName: "network"; objectName: "eogOverview"; text: "All systems"; checked: panel.eog.sceneMode === "overview"; enabled: panel.eog.sceneMode !== "overview"; onClicked: panel.eog.showOverview() }
                    ActionButton { flat: true; iconName: "factory"; objectName: "eogFacility"; text: "Facility"; checked: panel.eog.sceneMode === "facility"; enabled: panel.eog.sceneMode !== "facility"; onClicked: panel.eog.showFacility(panel.eog.facilityId) }
                    ColumnLayout {
                        Layout.preferredWidth: panel.width < 1000 ? 170 : 235
                        spacing: 2
                        Label { text: panel.historicalCursor ? "HISTORICAL CUTOFF" : "OBSERVATION CUTOFF"; font.pixelSize: Theme.captionSize; color: panel.historicalCursor ? Theme.warning : Theme.muted }
                        Label { Layout.fillWidth: true; text: panel.utc(panel.eog.cursorUnix); color: Theme.text; wrapMode: Text.Wrap; font.family: Theme.monoFamily; font.pixelSize: Theme.captionSize }
                    }
                    Slider {
                        objectName: "eogCutoffSlider"
                        Layout.fillWidth: true; Layout.minimumWidth: 80
                        from: panel.eog.firstUnix; to: Math.max(from, panel.eog.nowUnix)
                        stepSize: 300; value: panel.eog.cursorUnix
                        enabled: panel.eog.nowUnix > panel.eog.firstUnix
                        onMoved: panel.eog.setCursorUnix(value)
                        Accessible.name: "Recorded observation cutoff"
                    }
                    ActionButton { objectName: "eogLatestCutoff"; flat: true; iconName: "history"; text: "Latest"; tooltip: "Return to the latest recorded cutoff"; enabled: panel.historicalCursor; onClicked: panel.eog.setCursorUnix(panel.eog.nowUnix) }
                }
            }
            Pane {
                objectName: "eogEmptyState"
                Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumHeight: 320
                visible: !panel.eog.loaded; padding: 12
                background: Item {}
                contentItem: ColumnLayout {
                    Item { Layout.fillHeight: true }
                    Pane {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.fillWidth: true; Layout.maximumWidth: 760; padding: 24
                        background: Rectangle { color: Theme.surface; radius: Theme.cornerRadius + 2; border.color: Theme.divider }
                        contentItem: ColumnLayout {
                            spacing: 16
                            RowLayout {
                                spacing: 14; Layout.fillWidth: true
                                Image { source: Theme.icon("network"); Layout.preferredWidth: 32; Layout.preferredHeight: 32; sourceSize.width: 32; sourceSize.height: 32; Accessible.ignored: true }
                                ColumnLayout {
                                    Layout.fillWidth: true; spacing: 4
                                    Label { Layout.fillWidth: true; text: "Explore a recorded network"; wrapMode: Text.WordWrap; font.pixelSize: 24; font.weight: Font.DemiBold; color: Theme.text }
                                    Label { Layout.fillWidth: true; text: "Inspect equipment and measurements, then investigate a chosen point in time."; wrapMode: Text.WordWrap; color: Theme.secondary; font.pixelSize: Theme.bodySize }
                                }
                            }
                            Label { Layout.fillWidth: true; text: "Choose a folder containing these three files:"; wrapMode: Text.WordWrap; color: Theme.secondary }
                            Label { Layout.fillWidth: true; text: "network.json   ·   ops.json   ·   telemetry.csv"; wrapMode: Text.WrapAnywhere; color: Theme.accent; font.family: Theme.monoFamily; font.pixelSize: Theme.smallSize }
                            RowLayout {
                                Layout.fillWidth: true; spacing: 8
                                ActionButton { objectName: "eogStartImport"; text: "Open dataset"; iconName: "folder-open"; highlighted: true; enabled: !panel.eog.busy; onClicked: folder.open() }
                                ActionButton { objectName: "eogOpenExample"; text: "Try a synthetic example"; iconName: "play"; visible: panel.sampleDatasetPath.toString().length > 0; enabled: !panel.eog.busy; onClicked: panel.eog.loadFolder(panel.sampleDatasetPath) }
                                Item { Layout.fillWidth: true }
                            }
                            Label { Layout.fillWidth: true; text: "Recorded data stays read-only. Investigations start only when you ask."; wrapMode: Text.WordWrap; color: Theme.muted; font.pixelSize: Theme.smallSize }
                        }
                    }
                    Item { Layout.fillHeight: true }
                }
            }
            GridLayout {
                id: evidenceGrid
                Layout.fillWidth: true; visible: panel.eog.loaded
                Layout.fillHeight: true
                Layout.preferredHeight: panel.condensed ? Math.max(160, panel.availableHeight - 12) : Math.max(340, panel.availableHeight - dashboardHeader.implicitHeight - 12)
                columns: panel.width < 900 ? 1 : 2
                columnSpacing: 10; rowSpacing: 8
                EogNetworkView { renderQuality: panel.renderQuality;
                    id: network
                    eog: panel.eog
                    Layout.fillWidth: true; Layout.fillHeight: true
                    Layout.preferredWidth: 620; Layout.minimumHeight: panel.condensed ? 120 : 200
                }
                ColumnLayout {
                    Layout.fillWidth: true; Layout.fillHeight: true
                    Layout.preferredWidth: 450; Layout.minimumWidth: 0
                    spacing: 6
                    Label {
                        objectName: "eogChannelIdentity"
                        Layout.fillWidth: true
                        text: panel.eog.assetId + " · " + panel.eog.metric
                        textFormat: Text.PlainText; color: Theme.secondary; font.family: Theme.monoFamily; font.pixelSize: Theme.smallSize
                        elide: Text.ElideMiddle
                    }
                    Pane {
                        objectName: "eogObservationReadout"
                        Layout.fillWidth: true; padding: 12
                        Accessible.role: Accessible.StaticText; Accessible.name: panel.measurementDescription()
                        background: Rectangle { radius: Theme.cornerRadius; color: Theme.surface; border.color: panel.reading.stale ? Theme.warning : Theme.divider }
                        contentItem: ColumnLayout {
                            spacing: 5
                            RowLayout {
                                Layout.fillWidth: true; spacing: 10
                                Label { objectName: "eogObservationValue"; Layout.fillWidth: true; text: panel.reading.has_sample ? panel.number(panel.reading.value, panel.eog.metric === "status" ? 0 : 2) : "No reading"; textFormat: Text.PlainText; wrapMode: Text.WrapAnywhere; font.family: panel.reading.has_sample ? Theme.monoFamily : Theme.fontFamily; font.pixelSize: 24; color: !panel.reading.has_sample ? Theme.muted : panel.reading.stale ? Theme.warning : Theme.text }
                                Label { objectName: "eogObservationUnit"; visible: !!panel.reading.has_sample; text: panel.reading.unit || panel.eog.unit; textFormat: Text.PlainText; font.pixelSize: Theme.bodySize; color: Theme.secondary }
                            }
                            Label { objectName: "eogObservationStatus"; Layout.fillWidth: true; visible: !!panel.reading.has_sample && !!panel.reading.status_label; text: panel.reading.status_label || ""; textFormat: Text.PlainText; wrapMode: Text.WordWrap; color: Theme.secondary }
                            Label { objectName: "eogObservationFreshness"; Layout.fillWidth: true; text: panel.reading.has_sample ? (panel.historicalCursor ? "Historical · " : "At cutoff · ") + (panel.reading.freshness_label || (panel.reading.stale ? "stale" : "recorded")) : "Unavailable"; textFormat: Text.PlainText; wrapMode: Text.WordWrap; color: panel.reading.stale ? Theme.warning : Theme.secondary; font.pixelSize: Theme.smallSize }
                            Label { objectName: "eogObservationTimestamp"; Layout.fillWidth: true; visible: !!panel.reading.has_sample; text: "Recorded " + panel.utc(panel.reading.event_unix) + " · age " + panel.number(panel.reading.age_s, 0) + " s"; textFormat: Text.PlainText; wrapMode: Text.WordWrap; color: Theme.muted; font.pixelSize: Theme.captionSize }
                            Label { objectName: "eogObservationMissing"; Layout.fillWidth: true; visible: !panel.reading.has_sample; text: panel.measurementDescription(); textFormat: Text.PlainText; wrapMode: Text.WordWrap; color: Theme.secondary; font.pixelSize: Theme.smallSize }
                            Label { objectName: "eogObservationStaleNotice"; Layout.fillWidth: true; visible: !!panel.reading.has_sample && !!panel.reading.stale; text: "Last value retained as evidence; unavailable as a fresh measurement."; wrapMode: Text.WordWrap; color: Theme.warning; font.pixelSize: Theme.smallSize }
                        }
                    }
                    EogTrendChart {
                        id: selectedTrend
                        Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumHeight: 160
                        samples: panel.eog.selectedSeries; unit: panel.eog.unit
                        guides: panel.eog.selectedThresholds
                        discrete: panel.eog.metric === "status"
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

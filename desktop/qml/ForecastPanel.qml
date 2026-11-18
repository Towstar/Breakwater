import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: panel
    required property var controller
    property string tankId: ""
    property var reviewSnapshot: ({})
    readonly property bool dialogOpen: review.visible
    readonly property var tanks: controller.forecastTanks
    readonly property string activeTank: tanks.some(row => row.id === tankId) ? tankId : tanks.length ? tanks[0].id : ""
    readonly property var plotted: {
        const run = controller.runId
        return controller.forecastChart(activeTank)
    }
    spacing: 6
    RowLayout {
        Layout.fillWidth: true
        Label { text: "FORECAST / VALIDATION"; color: Theme.secondary; font.pixelSize: Theme.captionSize; font.letterSpacing: 1.2; font.bold: true }
        Item { Layout.fillWidth: true }
        Label { text: controller.runId.length ? "Run " + controller.runId.substring(0, 8) : "No executed forecast"; color: Theme.muted; font.pixelSize: Theme.captionSize }
        ActionButton { iconName: "clipboard-check"; text: "Approve simulated preview"; enabled: controller.canApprove; tooltip: "Review this exact forecast before approving the preview"; onClicked: { panel.reviewSnapshot = controller.forecastDetails; review.open() } }
    }
    RowLayout {
        Layout.fillWidth: true; Layout.fillHeight: true; spacing: 16
        ScrollView {
            id: summaryScroll; Layout.preferredWidth: Math.min(280, panel.width * 0.33); Layout.fillHeight: true; contentWidth: availableWidth; clip: true
            Label { width: summaryScroll.availableWidth; text: controller.summary; textFormat: Text.PlainText; wrapMode: Text.WordWrap; color: Theme.text; font.pixelSize: Theme.smallSize }
        }
        ColumnLayout {
            Layout.fillWidth: true; Layout.fillHeight: true; spacing: 4
            RowLayout {
                Layout.fillWidth: true
                Label { text: "Tank"; color: Theme.secondary; font.pixelSize: Theme.smallSize }
                AppComboBox {
                    objectName: "forecastTankSelector"; Layout.fillWidth: true; model: panel.tanks; textRole: "label"; valueRole: "id"
                    currentIndex: panel.tanks.findIndex(row => row.id === panel.activeTank)
                    enabled: panel.tanks.length > 0
                    displayText: count ? currentText : "Run a forecast to view a tank"
                    onActivated: panel.tankId = panel.tanks[currentIndex].id
                }
                Label { text: "m³ · elapsed hours"; color: Theme.muted; font.pixelSize: Theme.captionSize }
            }
            TrajectoryChart {
                Layout.fillWidth: true; Layout.fillHeight: true; chart: panel.plotted
                emptyText: "Run a forecast to view the native tank trajectory."
            }
        }
    }
    RowLayout {
        Layout.fillWidth: true
        Label { text: "t = " + controller.forecastTimeH.toFixed(3) + " h"; Layout.preferredWidth: 95; font.pixelSize: Theme.smallSize }
        Slider { Layout.fillWidth: true; from: 0; to: 1; enabled: controller.runId.length > 0; value: controller.forecastFraction; onMoved: controller.forecastFraction = value }
        Label { text: controller.runId.length ? controller.forecastHorizonH.toFixed(3) + " h" : "No executed horizon"; color: Theme.secondary; font.pixelSize: Theme.smallSize }
    }
    Dialog {
        id: review; parent: Overlay.overlay; anchors.centerIn: parent; modal: true
        width: Math.min(700, parent ? parent.width - 32 : 700); height: Math.min(580, parent ? parent.height - 32 : 580)
        title: "Review simulated forecast"; padding: 18; standardButtons: Dialog.Cancel
        header: DialogHeading { owner: review }
        footer: DialogFooter { owner: review; label: "Cancel" }
        contentItem: ColumnLayout {
            spacing: 12
            Label { Layout.fillWidth: true; text: "Run " + (panel.reviewSnapshot.run_id || "") + "\n" + (panel.reviewSnapshot.validation_source || ""); textFormat: Text.PlainText; wrapMode: Text.WrapAnywhere }
            Label { Layout.fillWidth: true; text: "Horizon " + panel.reviewSnapshot.horizon_h + " h · modeled cost " + Number(panel.reviewSnapshot.cost || 0).toFixed(2) + " synthetic units\nMinimum headroom " + Number(panel.reviewSnapshot.minimum_headroom_m3 || 0).toFixed(2) + " m³"; wrapMode: Text.Wrap; color: Theme.secondary }
            ScrollView {
                id: controlsScroll; Layout.fillWidth: true; Layout.fillHeight: true; contentWidth: availableWidth
                ColumnLayout {
                    width: controlsScroll.availableWidth; spacing: 8
                    Repeater { model: panel.reviewSnapshot.controls || []
                        Label { required property var modelData; Layout.fillWidth: true; text: modelData.control_id + ": " + Number(modelData.value).toFixed(3) + " " + (modelData.unit === "m3/h" ? "m³/h" : modelData.unit); textFormat: Text.PlainText; wrapMode: Text.Wrap }
                    }
                }
            }
            Label { Layout.fillWidth: true; text: "Approves only this simulated preview. No operating controls or equipment commands are changed."; wrapMode: Text.Wrap; color: Theme.warning }
            ActionButton {
                text: "Approve this exact simulated preview"; highlighted: true
                enabled: controller.canApprove && controller.runId === panel.reviewSnapshot.run_id && controller.networkHash === panel.reviewSnapshot.physical_hash
                onClicked: { controller.approve(); review.close() }
            }
        }
    }
}

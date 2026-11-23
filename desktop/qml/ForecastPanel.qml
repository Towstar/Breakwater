import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: panel
    objectName: "forecastPanel"
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
    clip: true
    spacing: 6
    RowLayout {
        objectName: "forecastHeader"
        Layout.fillWidth: true
        Label { Layout.fillWidth: true; Layout.minimumWidth: 0; text: "FORECAST / VALIDATION"; elide: Text.ElideRight; color: Theme.secondary; font.pixelSize: Theme.captionSize; font.letterSpacing: 1.2; font.bold: true }
        Label { visible: controller.runId.length > 0; text: "Run " + controller.runId.substring(0, 8); color: Theme.muted; font.pixelSize: Theme.captionSize }
        ActionButton { iconName: "clipboard-check"; text: "Approve simulated preview"; enabled: controller.canApprove; tooltip: "Review this exact forecast before approving the preview"; onClicked: { panel.reviewSnapshot = controller.forecastDetails; review.open() } }
    }
    ScrollView {
        id: bodyScroll; objectName: "forecastBodyScroll"
        Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumHeight: 0
        contentWidth: availableWidth; clip: true
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        RowLayout {
            width: bodyScroll.availableWidth; height: Math.max(implicitHeight, bodyScroll.availableHeight); spacing: 16
            ScrollView {
                id: summaryScroll; Layout.preferredWidth: Math.min(260, panel.width * 0.28); Layout.fillHeight: true; Layout.minimumHeight: 0; Layout.preferredHeight: 0; contentWidth: availableWidth; clip: true; padding: 10
                background: Rectangle { color: Theme.raised; radius: Theme.cornerRadius; border.color: Theme.divider }
                Label { width: summaryScroll.availableWidth; text: controller.summary; textFormat: Text.PlainText; wrapMode: Text.WordWrap; color: Theme.text; font.pixelSize: Theme.smallSize }
            }
            ColumnLayout {
                Layout.fillWidth: true; Layout.fillHeight: true; spacing: 4
                RowLayout {
                    Layout.fillWidth: true; visible: controller.runId.length > 0
                    Label { text: "Tank"; color: Theme.secondary; font.pixelSize: Theme.smallSize }
                    AppComboBox {
                        objectName: "forecastTankSelector"; Layout.fillWidth: true; Layout.minimumWidth: 0; model: panel.tanks; textRole: "label"; valueRole: "id"
                        currentIndex: panel.tanks.findIndex(row => row.id === panel.activeTank)
                        enabled: panel.tanks.length > 0
                        displayText: count ? currentText : "Run a forecast to view a tank"
                        onActivated: panel.tankId = panel.tanks[currentIndex].id
                    }
                }
                TrajectoryChart {
                    Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumHeight: 160; chart: panel.plotted; visible: !!panel.plotted.available
                    emptyText: "Run a forecast to view the native tank trajectory."
                }
                Rectangle {
                    objectName: "forecastEmptyState"
                    Layout.fillWidth: true; Layout.fillHeight: true; visible: !panel.plotted.available
                    Layout.minimumHeight: emptyContent.implicitHeight + 24
                    color: Theme.raised; radius: Theme.cornerRadius; border.color: Theme.divider
                    ColumnLayout {
                        id: emptyContent; objectName: "forecastEmptyContent"
                        anchors.centerIn: parent; width: Math.min(380,parent.width-32); spacing: 10
                        Label { Layout.fillWidth: true; text: "Forecast this network"; font.pixelSize: 16; font.weight: Font.DemiBold; wrapMode: Text.WordWrap }
                        Label { Layout.fillWidth: true; text: "Calculate tank volumes and capacity margins from the current simulation inputs. No model key is needed."; textFormat: Text.PlainText; wrapMode: Text.WordWrap; color: Theme.secondary; font.pixelSize: Theme.smallSize }
                        ActionButton { objectName: "forecastEmptyRun"; text: controller.busy ? "Calculating…" : "Run native forecast"; iconName: "play"; enabled: !controller.busy && controller.draftIssues.length === 0; onClicked: controller.forecast() }
                    }
                }
            }
        }
    }
    RowLayout {
        objectName: "forecastTimeline"
        Layout.fillWidth: true; visible: controller.runId.length > 0
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

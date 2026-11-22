pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: settings
    required property var workspaceSettings
    required property var credentials
    required property var scientificWorker
    required property string buildInfo
    property bool investigationRunning: false
    property Item returnFocusItem: null
    signal manageKeyRequested(Item source)
    signal helpRequested()
    signal aboutRequested()
    objectName: "settingsDialog"
    parent: Overlay.overlay; anchors.centerIn: parent; modal: true; closePolicy: Popup.CloseOnEscape
    title: "Settings"; padding: 16
    width: Math.min(640, parent.width - 32); height: Math.min(620, parent.height - 32)
    header: DialogHeading { owner: settings; closeName: "closeSettings" }
    footer: DialogFooter { owner: settings }
    onOpened: { settingsTabs.currentIndex = 0; qualitySelector.forceActiveFocus(Qt.PopupFocusReason) }
    onClosed: { if (returnFocusItem && returnFocusItem.visible) returnFocusItem.forceActiveFocus(Qt.PopupFocusReason) }
    component Copy: Label { textFormat: Text.PlainText; wrapMode: Text.WordWrap; color: Theme.secondary; font.pixelSize: Theme.smallSize; Layout.fillWidth: true }
    component Section: Copy { color: Theme.text; font.weight: Font.DemiBold; font.pixelSize: Theme.bodySize }
    component Preference: CheckBox {
        property string preferenceKey
        checked: settings.workspaceSettings.values[preferenceKey]
        onToggled: settings.workspaceSettings.setValue(preferenceKey, checked)
    }
    contentItem: ColumnLayout {
        spacing: 12
        TabBar {
            id: settingsTabs; objectName: "settingsTabs"; Layout.fillWidth: true
            TabButton { objectName: "settingsWorkspaceTab"; text: "Workspace" }
            TabButton { objectName: "settingsConnectionsTab"; text: "Connections" }
            TabButton { objectName: "settingsInformationTab"; text: "Information" }
        }
        Copy { visible: settings.workspaceSettings.status.length > 0; text: settings.workspaceSettings.status; color: Theme.warning }
        StackLayout {
            Layout.fillWidth: true; Layout.fillHeight: true; currentIndex: settingsTabs.currentIndex
            ScrollView {
                id: workspaceScroll; objectName: "settingsWorkspaceScroll"; contentWidth: availableWidth; clip: true
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                ColumnLayout {
                    width: workspaceScroll.availableWidth; spacing: 10
                    Section { text: "Display" }
                    Copy { text: "Rendering quality" }
                    AppComboBox {
                        id: qualitySelector; objectName: "settingsRenderingQuality"; Layout.fillWidth: true
                        model: ["Low", "Balanced", "High"]; currentIndex: settings.workspaceSettings.values.quality
                        Accessible.name: "Rendering quality"
                        onActivated: settings.workspaceSettings.setValue("quality", currentIndex)
                    }
                    Copy { text: "Balanced is recommended for everyday use. This changes visual detail only."; font.pixelSize: Theme.captionSize }
                    Preference { objectName: "settingsCompactControls"; text: "Compact controls"; preferenceKey: "compactControls" }
                    Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.divider; Layout.topMargin: 6; Layout.bottomMargin: 6 }
                    Section { text: "Sandbox workspace" }
                    Preference { objectName: "settingsGridVisible"; text: "Show layout grid"; preferenceKey: "gridVisible" }
                    Preference { objectName: "settingsAllLabels"; text: "Show more equipment labels"; preferenceKey: "allLabels" }
                    Preference { objectName: "settingsNavigatorOpen"; text: "Show network navigator"; preferenceKey: "navigatorOpen" }
                    Preference { objectName: "settingsInspectorOpen"; text: "Show equipment inspector"; preferenceKey: "inspectorOpen" }
                    Preference { objectName: "settingsDockOpen"; text: "Show results panel"; preferenceKey: "dockOpen" }
                    Copy { text: "Preferences save automatically on this computer. Drag the results-panel divider to adjust its height."; font.pixelSize: Theme.captionSize }
                    ActionButton { objectName: "restoreWorkspacePreferences"; text: "Restore preferences"; iconName: "rotate-ccw"; onClicked: settings.workspaceSettings.reset() }
                    Copy { text: "Restores display and panel defaults. Projects, history and saved API keys are kept."; font.pixelSize: Theme.captionSize }
                }
            }
            ScrollView {
                id: connectionScroll; contentWidth: availableWidth; clip: true
                ColumnLayout {
                    width: connectionScroll.availableWidth; spacing: 14
                    Section { text: "NVIDIA Nemotron" }
                    Copy { text: settings.credentials.configured ? "API key available" : "No API key configured"; color: settings.credentials.configured ? Theme.success : Theme.secondary }
                    Copy { text: settings.credentials.status }
                    ActionButton {
                        id: manageKey; objectName: "settingsManageKey"; text: "Manage API key"; iconName: "key-round"
                        enabled: !settings.investigationRunning
                        onClicked: settings.manageKeyRequested(manageKey)
                    }
                    Copy { text: "Opening Settings does not contact NVIDIA. Local numerical search works without an API key."; font.pixelSize: Theme.captionSize }
                    Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.divider }
                    Section { text: "Scientific worker" }
                    Copy { objectName: "settingsWorkerStatus"; text: settings.scientificWorker.status }
                    ActionButton { objectName: "settingsCheckWorker"; text: settings.scientificWorker.busy ? "Worker busy" : "Check worker"; iconName: "activity"; enabled: !settings.scientificWorker.busy; onClicked: settings.scientificWorker.checkHealth() }
                    Copy { text: "A health check verifies the local worker connection. Scientific methods retain their separate execution and validation checks."; font.pixelSize: Theme.captionSize }
                }
            }
            ScrollView {
                id: informationScroll; contentWidth: availableWidth; clip: true
                ColumnLayout {
                    width: informationScroll.availableWidth; spacing: 16
                    Section { text: "Breakwater " + settings.buildInfo }
                    Copy { text: "A fluid-network sandbox and recorded-data advisory workspace. Numerical evidence remains separate from model explanations." }
                    ActionButton { objectName: "settingsWorkspaceHelp"; text: "Controls and shortcuts"; iconName: "mouse"; onClicked: { settings.close(); settings.helpRequested() } }
                    ActionButton { objectName: "settingsAbout"; text: "About Breakwater"; iconName: "clipboard-check"; onClicked: { settings.close(); settings.aboutRequested() } }
                    Copy { text: "Synthetic and advisory. The scene is schematic; it does not operate physical equipment."; font.pixelSize: Theme.captionSize }
                }
            }
        }
    }
}

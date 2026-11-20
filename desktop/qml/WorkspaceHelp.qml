import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: help
    objectName: "workspaceHelp"
    parent: Overlay.overlay; anchors.centerIn: parent; modal: true
    title: "Workspace controls"; padding: 20
    width: Math.min(620,parent ? parent.width-32 : 620)
    height: Math.min(510,parent ? parent.height-32 : 510)
    standardButtons: Dialog.Close
    header: DialogHeading { owner: help; closeName: "closeWorkspaceHelp" }
    footer: DialogFooter { owner: help }
    contentItem: ScrollView {
        id: scroll; contentWidth: availableWidth; clip: true
        ColumnLayout {
            width: scroll.availableWidth; spacing: 16
            Label { Layout.fillWidth: true; text: "A mouse with a wheel is recommended for orbiting and panning. Tab and Shift+Tab reach buttons, equipment and inputs; Enter or Space activates the focused action."; wrapMode: Text.WordWrap; color: Theme.secondary }
            GridLayout {
                columns: 2; columnSpacing: 20; rowSpacing: 12; Layout.fillWidth: true
                Label { text: "Right / middle drag"; font.family: Theme.monoFamily; font.pixelSize: Theme.smallSize }
                Label { text: "Orbit / pan the view"; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                Label { text: "Wheel"; font.family: Theme.monoFamily; font.pixelSize: Theme.smallSize }
                Label { text: "Zoom toward the workspace"; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                Label { text: "Ctrl+Home / Ctrl+0 / F"; font.family: Theme.monoFamily; font.pixelSize: Theme.smallSize }
                Label { text: "Restore / fit network / focus equipment or connection endpoints"; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                Label { text: "Ctrl+Shift+F"; font.family: Theme.monoFamily; font.pixelSize: Theme.smallSize }
                Label { text: "Toggle scene focus mode; Escape returns to your panels"; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                Label { text: "Ctrl-click / Shift-click"; font.family: Theme.monoFamily; font.pixelSize: Theme.smallSize }
                Label { text: "Toggle / extend equipment selection"; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                Label { text: "Ctrl+D / Delete"; font.family: Theme.monoFamily; font.pixelSize: Theme.smallSize }
                Label { text: "Duplicate / delete selected equipment"; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                Label { text: "Ctrl+Z / Ctrl+Y"; font.family: Theme.monoFamily; font.pixelSize: Theme.smallSize }
                Label { text: "Undo / redo an edit"; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                Label { text: "Escape / F1"; font.family: Theme.monoFamily; font.pixelSize: Theme.smallSize }
                Label { text: "Cancel the current gesture / open this help"; Layout.fillWidth: true; wrapMode: Text.WordWrap }
            }
            Label { Layout.fillWidth: true; text: "Dense views prioritize selected and hovered equipment labels. Use Network to filter and inspect every asset; View → More equipment labels adds labels where they fit."; textFormat: Text.PlainText; wrapMode: Text.WordWrap; color: Theme.secondary }
            Label { Layout.fillWidth: true; text: "Top view fits the network into the usable scene and adjusts when panels resize. Fit resets manual zoom and pan; Focus frames the selected equipment or connection endpoints. The footer identifies the current view. View → Scene focus mode temporarily hides panels without changing your saved panel preferences."; textFormat: Text.PlainText; wrapMode: Text.WordWrap; color: Theme.secondary }
            Label { Layout.fillWidth: true; text: "Use the Equipment / Connections icons beside Network to find equipment or routes. Connections can be filtered by ID, endpoint or pump status. Selecting a connection reveals its endpoint labels; Focus or F frames both ends. Rates and availability describe simulation settings."; textFormat: Text.PlainText; wrapMode: Text.WordWrap; color: Theme.secondary }
            Label { Layout.fillWidth: true; text: "The selected connection has a static white source-to-destination marker, including unavailable or zero-setting routes. Its scene card keeps settings and availability separate. The marker shows direction only, not measured delivery; scene positions remain schematic."; textFormat: Text.PlainText; wrapMode: Text.WordWrap; color: Theme.secondary }
            Label { Layout.fillWidth: true; text: "Scene layout is schematic. Moving equipment or changing the view does not change hydraulic inputs. Numerical edits retire earlier forecast approval."; wrapMode: Text.WordWrap; color: Theme.muted; font.pixelSize: Theme.smallSize }
        }
    }
}

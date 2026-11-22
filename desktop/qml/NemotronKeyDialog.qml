import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: keyDialog
    required property var credentials
    property bool investigationRunning: false
    property Item returnFocusItem: null
    objectName: "nemotronKeyDialog"
    parent: Overlay.overlay; anchors.centerIn: parent
    title: "NVIDIA API key"; modal: true; closePolicy: Popup.CloseOnEscape
    width: Math.min(500, parent.width - 32); height: Math.min(365, parent.height - 32)
    padding: 16
    header: DialogHeading { owner: keyDialog }
    footer: DialogFooter { owner: keyDialog }
    onOpened: apiKeyInput.forceActiveFocus()
    onClosed: { apiKeyInput.clear(); if (returnFocusItem && returnFocusItem.visible) returnFocusItem.forceActiveFocus(Qt.PopupFocusReason) }
    contentItem: ScrollView {
        id: keyScroll; contentWidth: availableWidth; clip: true
        ColumnLayout {
            width: keyScroll.availableWidth; spacing: 14
            Label { Layout.fillWidth: true; text: "The key is stored locally for this Windows account and used for future investigations."; textFormat: Text.PlainText; wrapMode: Text.WordWrap; color: Theme.secondary }
            TextField {
                id: apiKeyInput; objectName: "nemotronApiKeyInput"; Layout.fillWidth: true
                placeholderText: "NVIDIA API key"; Accessible.name: "NVIDIA API key"
                echoMode: TextInput.Password; maximumLength: 4096; selectByMouse: true
                enabled: !keyDialog.investigationRunning
                onAccepted: { if (saveKeyButton.enabled) saveKeyButton.clicked() }
            }
            Label { Layout.fillWidth: true; text: keyDialog.credentials.status; textFormat: Text.PlainText; wrapMode: Text.WordWrap; color: Theme.secondary }
            Flow {
                Layout.fillWidth: true; spacing: 8
                ActionButton {
                    id: saveKeyButton; objectName: "saveNemotronKey"; text: "Save key"; iconName: "key-round"; highlighted: true
                    enabled: apiKeyInput.text.trim().length > 0 && !keyDialog.investigationRunning
                    onClicked: { if (keyDialog.credentials.saveApiKey(apiKeyInput.text)) { apiKeyInput.clear(); keyDialog.close() } }
                }
                ActionButton {
                    objectName: "removeNemotronKey"; text: "Remove saved key"; iconName: "trash-2"
                    enabled: keyDialog.credentials.saved && !keyDialog.investigationRunning
                    onClicked: { if (keyDialog.credentials.forgetApiKey()) apiKeyInput.clear() }
                }
            }
        }
    }
}

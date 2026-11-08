import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Frame {
    id: card
    property var feedback: ({})
    property bool noticeVisible: false
    readonly property bool failed: feedback.severity === "error"
    readonly property color accent: failed ? Theme.error : feedback.severity === "warning" ? Theme.warning : Theme.secondary
    signal dismissed()
    padding: 14
    visible: noticeVisible
    implicitHeight: contentItem.implicitHeight + topPadding + bottomPadding
    Accessible.role: Accessible.AlertMessage
    Accessible.name: String(feedback.title || "Action feedback")
    Accessible.description: String(feedback.message || "")
    function showFeedback(event) {
        // Called by the dedicated action signal, including a repeated identical
        // failure. Ordinary scene/controller refreshes never reopen the notice.
        feedback = event || ({})
        noticeVisible = !!(feedback.title || feedback.message)
    }
    function dismiss() { noticeVisible = false; dismissed() }
    function recoveryText() {
        const action = feedback.recovery_action
        if (action === "show_issues" || action === "view_issues") return "Review Network issues in the properties panel."
        if (action === "select_route") return "Select a pipe route in the scene or network list."
        if (action === "select_asset") return "Select an asset to inspect its properties."
        if (action === "enter_valid_value") return "Correct the entry in the properties panel and retry."
        if (action === "retry") return "Retry the action when the current operation finishes."
        return ""
    }
    Keys.onShortcutOverride: function(event) { if (event.key === Qt.Key_Escape) event.accepted = true }
    Keys.onEscapePressed: function(event) { dismiss(); event.accepted = true }
    background: Rectangle {
        radius: Theme.cornerRadius
        color: Theme.raised
        border.color: Theme.divider
        border.width: 1
        Rectangle { anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom; width: 3; color: card.accent }
        // Only the visible card surface consumes clicks/wheels. No full-scene
        // input item or popup is created, and existing scene grabs remain intact.
        MouseArea { anchors.fill: parent; acceptedButtons: Qt.AllButtons; onWheel: function(event) { event.accepted = true } }
    }
    contentItem: ColumnLayout {
        spacing: 7
        RowLayout {
            Layout.fillWidth: true
            Image { source: Theme.icon("triangle-alert"); sourceSize.width: 19; sourceSize.height: 19; Layout.preferredWidth: 19; Layout.preferredHeight: 19; fillMode: Image.PreserveAspectFit; Accessible.ignored: true }
            Label { Layout.fillWidth: true; text: String(card.feedback.title || "Action feedback"); textFormat: Text.PlainText; wrapMode: Text.Wrap; font.family: Theme.fontFamily; font.pixelSize: Theme.bodySize; font.bold: true; color: Theme.text }
            ActionButton { objectName: "dismissActionFeedback"; iconName: "x"; text: "Dismiss feedback"; display: AbstractButton.IconOnly; implicitWidth: 30; implicitHeight: 30; tooltip: "Dismiss this notice; current network issues remain visible"; onClicked: card.dismiss() }
        }
        ScrollView {
            id: messageScroll
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(messageText.implicitHeight, 160)
            contentWidth: availableWidth
            clip: true
            Label { id: messageText; width: messageScroll.availableWidth; text: String(card.feedback.message || ""); textFormat: Text.PlainText; wrapMode: Text.Wrap; color: Theme.text; font.pixelSize: Theme.bodySize }
        }
        Label { Layout.fillWidth: true; visible: text.length > 0; text: card.recoveryText(); textFormat: Text.PlainText; wrapMode: Text.Wrap; color: Theme.secondary; font.pixelSize: Theme.smallSize }
        Label { visible: card.feedback.model_changed === false; text: "No model change"; color: Theme.muted; font.pixelSize: Theme.captionSize }
    }
}

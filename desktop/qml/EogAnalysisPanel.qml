import QtQuick
import QtQuick.Controls

// Compatibility dialog for standalone consumers. The application docks the same
// content view; tool execution and approval remain in the existing controllers.
Dialog {
    id: dialog
    required property var analysis
    required property var eog
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(1160, parent ? parent.width - 32 : 1160)
    height: Math.min(820, parent ? parent.height - 32 : 820)
    modal: true; title: "Operations investigation and retained evidence"
    padding: 0
    standardButtons: Dialog.Close
    background: Rectangle { color: Theme.surface; border.color: Theme.divider; radius: Theme.cornerRadius }
    header: DialogHeading { owner: dialog; closeName: "eogAnalysisClose" }
    footer: DialogFooter { owner: dialog }
    contentItem: EogAnalysisView { analysis: dialog.analysis; eog: dialog.eog }
    onOpened: analysis.refreshHistory(0)
}

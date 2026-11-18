import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: footer
    required property var owner
    property string label: "Close"
    implicitHeight: 60
    color: Theme.raised
    Rectangle { width: parent.width; height: 1; color: Theme.divider }
    RowLayout {
        anchors.fill: parent; anchors.margins: 12
        Item { Layout.fillWidth: true }
        ActionButton { text: footer.label; iconName: "x"; onClicked: footer.owner.close() }
    }
}

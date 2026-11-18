import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: heading
    required property var owner
    property string closeName: ""
    implicitHeight: 56
    color: Theme.raised
    Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 1; color: Theme.divider }
    RowLayout {
        anchors.fill: parent; anchors.leftMargin: 20; anchors.rightMargin: 12; spacing: 16
        Label { Layout.fillWidth: true; text: heading.owner.title; elide: Text.ElideRight; color: Theme.text; font.pixelSize: 17; font.weight: Font.DemiBold }
        ActionButton { objectName: heading.closeName; iconName: "x"; text: "Close"; display: AbstractButton.IconOnly; tooltip: "Close"; onClicked: heading.owner.close() }
    }
}

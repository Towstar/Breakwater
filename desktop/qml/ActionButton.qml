import QtQuick
import QtQuick.Controls

Button {
    id: control
    property string iconName: ""
    property string tooltip: text
    property string accessibleName: text
    implicitHeight: 36
    implicitWidth: Math.max(display === AbstractButton.IconOnly ? 36 : 64, implicitContentWidth + leftPadding + rightPadding)
    leftPadding: 10
    rightPadding: 10
    topPadding: 7
    bottomPadding: 7
    spacing: 7
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    font.family: Theme.fontFamily
    font.pixelSize: Theme.smallSize
    font.weight: highlighted ? Font.DemiBold : Font.Medium
    icon.source: Theme.icon(iconName)
    icon.width: 17
    icon.height: 17
    icon.color: !enabled ? Theme.muted : highlighted ? Theme.background : Theme.text
    palette.buttonText: !enabled ? Theme.muted : highlighted ? Theme.background : Theme.text
    palette.brightText: !enabled ? Theme.muted : highlighted ? Theme.background : Theme.text
    Accessible.name: accessibleName.length ? accessibleName : tooltip
    Accessible.description: tooltip
    ToolTip.visible: (hovered || activeFocus) && tooltip.length > 0
    ToolTip.delay: activeFocus ? 0 : 450
    ToolTip.text: tooltip
    background: Rectangle {
        implicitHeight: 36
        radius: Theme.cornerRadius
        color: !control.enabled ? Theme.surface
            : control.highlighted ? (control.down ? Theme.primaryPressed : control.hovered ? Theme.primaryHover : Theme.text)
            : control.down || control.checked ? Theme.selected : control.hovered ? Theme.hover : Theme.raised
        border.width: control.checked ? 2 : 1
        border.color: !control.enabled ? Theme.divider : control.highlighted || control.checked || control.visualFocus ? Theme.text : Theme.controlBorder
        Rectangle {
            anchors.fill: parent
            anchors.margins: -3
            radius: Theme.cornerRadius + 2
            color: "transparent"
            border.width: 1
            border.color: Theme.text
            visible: control.visualFocus
        }
    }
}

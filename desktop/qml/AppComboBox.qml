import QtQuick
import QtQuick.Controls

ComboBox {
    id: control
    implicitWidth: 180
    implicitHeight: Theme.controlHeight
    leftPadding: 12; rightPadding: 36; topPadding: 7; bottomPadding: 7
    hoverEnabled: true
    font.family: Theme.fontFamily; font.pixelSize: Theme.bodySize
    contentItem: Label {
        text: control.displayText; textFormat: Text.PlainText; elide: Text.ElideRight
        verticalAlignment: Text.AlignVCenter; font: control.font
        color: control.enabled ? Theme.text : Theme.muted
    }
    indicator: Image {
        x: control.width - width - 12; y: (control.height - height) / 2
        width: 14; height: 14; sourceSize.width: 14; sourceSize.height: 14
        source: Theme.icon("chevron-down"); opacity: control.enabled ? 1 : 0.4
        Accessible.ignored: true
    }
    background: Rectangle {
        radius: Theme.cornerRadius
        color: control.popup.visible ? Theme.selected : control.hovered ? Theme.hover : Theme.raised
        border.width: control.visualFocus ? 2 : 1
        border.color: control.visualFocus || control.popup.visible ? Theme.text : control.hovered ? Theme.controlBorder : Theme.divider
    }
    delegate: ItemDelegate {
        id: option
        required property int index
        width: control.popup.availableWidth; implicitHeight: Theme.controlHeight
        text: control.textAt(index); highlighted: control.highlightedIndex === index
        contentItem: Label {
            text: option.text; textFormat: Text.PlainText; font.family: control.font.family; font.pixelSize: control.font.pixelSize
            elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter; color: Theme.text
            font.weight: control.currentIndex === option.index ? Font.DemiBold : Font.Normal
        }
        background: Rectangle { radius: 3; color: option.highlighted ? Theme.selected : option.hovered ? Theme.hover : Theme.surface }
    }
    popup: Popup {
        y: control.height + 5; width: control.width; padding: 5
        implicitHeight: Math.min(contentItem.implicitHeight + topPadding + bottomPadding, 280)
        contentItem: ListView {
            clip: true; implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex; highlightMoveDuration: 0
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
        }
        background: Rectangle { color: Theme.surface; radius: Theme.cornerRadius; border.color: Theme.controlBorder }
    }
}

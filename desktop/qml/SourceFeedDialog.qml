import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: dialog
    required property var controller
    property string sourceId: ""
    property string expectedHash: ""
    property real availableFlow: 0
    property int totalBps: 0
    property string errorText: ""
    readonly property bool stale: controller.networkHash !== expectedHash
    parent: Overlay.overlay
    modal: true
    closePolicy: Popup.CloseOnEscape
    width: Math.min(540, parent ? parent.width - 40 : 540)
    x: parent ? (parent.width-width)/2 : 0
    y: parent ? Math.max(12,(parent.height-height)/2) : 0
    title: "Production feed split"
    header: Label { text: dialog.title; textFormat: Text.PlainText; padding: 12; font.pixelSize: 17; font.bold: true }

    ListModel { id: shares }
    function updateTotal() {
        let total = 0
        for (let i=0; i<shares.count; ++i) total += shares.get(i).shareBps
        totalBps = total
    }
    function begin(source, target) {
        controller.select(source)
        if (controller.selected.kind !== "source") return
        sourceId = source
        expectedHash = controller.networkHash
        availableFlow = controller.selected.inflow - controller.selected.curtailment
        title = "Feed split · " + controller.selected.label
        errorText = ""
        const feeds = controller.sourceFeeds(source)
        let adding = target.length > 0
        for (let i=0; i<feeds.length; ++i) if (feeds[i].targetId === target) adding = false
        shares.clear()
        for (let i=0; i<feeds.length; ++i)
            shares.append({targetId:feeds[i].targetId, label:feeds[i].label, shareBps:feeds[i].shareBps})
        if (adding) {
            const assets = controller.assets
            let label = target
            for (let i=0; i<assets.length; ++i) if (assets[i].id === target) label = assets[i].label
            shares.append({targetId:target,label:label,shareBps:0})
            // A visible proposal only. No project mutation occurs until Apply.
            const equal = Math.floor(10000/shares.count)
            const remaining = 10000-equal*shares.count
            for (let i=0; i<shares.count; ++i) shares.setProperty(i,"shareBps",equal+(i<remaining ? 1 : 0))
        }
        updateTotal()
        open()
    }
    contentItem: ColumnLayout {
        spacing: 12
        Label {
            Layout.fillWidth: true
            text: "Allocate " + dialog.availableFlow.toFixed(3) + " m³/h after production reduction. Shares must total 100%."
            wrapMode: Text.WordWrap
        }
        Label { Layout.fillWidth: true; text: "Adding a branch proposes an equal split. Review or edit every destination before applying."; wrapMode: Text.WordWrap; color: "#a7bbc7" }
        ScrollView {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(260,shareRows.implicitHeight)
            contentWidth: availableWidth
            ColumnLayout {
                id: shareRows
                width: parent.width
                spacing: 8
                Repeater {
                    model: shares
                    delegate: RowLayout {
                        id: row
                        required property int index
                        required property string targetId
                        required property string label
                        required property int shareBps
                        Layout.fillWidth: true
                        Label { Layout.fillWidth: true; text: row.label; textFormat: Text.PlainText; wrapMode: Text.WrapAnywhere }
                        SpinBox {
                            objectName: "sourceShare_" + row.targetId
                            Accessible.name: row.label + " feed percentage"
                            from: 0; to: 10000; stepSize: 1; editable: true; live: true
                            value: row.shareBps
                            validator: DoubleValidator { bottom: 0; top: 100; decimals: 2; notation: DoubleValidator.StandardNotation }
                            textFromValue: function(value,locale) { return Number(value/100).toLocaleString(locale,"f",2) }
                            valueFromText: function(text,locale) { return Math.round(Number.fromLocaleString(locale,text)*100) }
                            onValueModified: { shares.setProperty(row.index,"shareBps",value); dialog.updateTotal() }
                        }
                        Label { text: "%" }
                        Label { Layout.preferredWidth: 95; text: (dialog.availableFlow*row.shareBps/10000).toFixed(3) + " m³/h" }
                    }
                }
            }
        }
        Label { text: "Total: " + (dialog.totalBps/100).toFixed(2) + "%"; color: dialog.totalBps===10000 ? "#60d2b1" : "#ffcf70"; font.bold: true }
        Label { Layout.fillWidth: true; text: "These are configured flow shares, not a pressure/valve simulation. Changing them retires the forecast and approval."; wrapMode: Text.WordWrap; color: "#a7bbc7" }
        Label { Layout.fillWidth: true; visible: dialog.stale || dialog.errorText.length>0; text: dialog.stale ? "The network changed. Cancel and reopen the split editor." : dialog.errorText; wrapMode: Text.WordWrap; color: "#ffcf70" }
    }
    footer: RowLayout {
        Item { Layout.fillWidth: true }
        Button { text: "Cancel"; onClicked: dialog.close() }
        Button {
            text: "Apply split"; highlighted: true
            enabled: dialog.totalBps===10000 && !dialog.stale
            onClicked: {
                const allocations = {}
                for (let i=0; i<shares.count; ++i) allocations[shares.get(i).targetId] = shares.get(i).shareBps
                if (controller.setSourceFeeds(dialog.sourceId,allocations,dialog.expectedHash)) dialog.close()
                else dialog.errorText = controller.summary
            }
        }
    }
}

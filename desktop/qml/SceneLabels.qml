pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import "LabelPlacement.js" as LabelPlacement

Item {
    id: labels
    required property var rows
    required property var projector
    property var viewState: []
    property bool ready: false
    property bool allLabels: false
    property string primaryId: ""
    property string hoveredId: ""
    property var relatedIds: []
    property var draggedIds: []
    property vector3d dragDelta: Qt.vector3d(0, 0, 0)
    property real topInset: 90
    property var exclusions: []
    readonly property int visibleCount: positioned.length
    readonly property var positioned: {
        const view = viewState
        if (!ready || !projector || width <= 20 || height <= topInset + 60) return []
        const candidates = []
        for (const row of rows) {
            const related = relatedIds.indexOf(row.id) >= 0
            if (!(allLabels || row.selected || row.id === hoveredId || related || (rows.length < 12 && row.kind === "tank"))) continue
            const delta = draggedIds.indexOf(row.id) >= 0 ? dragDelta : Qt.vector3d(0, 0, 0)
            const point = projector.mapFrom3DScene(Qt.vector3d(row.x + delta.x, 108, row.z + delta.z))
            if (![point.x, point.y, point.z].every(Number.isFinite) || point.z <= 0
                    || point.x < 0 || point.x >= width || point.y < 0 || point.y >= height) continue
            candidates.push({id:row.id, text:row.label, selected:row.selected, hovered:row.id === hoveredId, related:related,
                priority:row.id === primaryId ? 0 : row.id === hoveredId ? 1 : row.selected ? 2 : related ? 2.5 : row.kind === "tank" ? 3 : 4,
                anchorX:point.x, anchorY:point.y,
                width:Math.min(220, width - 20, Math.ceil(metrics.advanceWidth(row.label)) + 14), height:Math.ceil(metrics.height) + 10})
        }
        return LabelPlacement.place(candidates, {x:10, y:topInset, width:width-20, height:height-topInset-48}, exclusions)
    }
    FontMetrics { id: metrics; font.family: Theme.fontFamily; font.pixelSize: 12 }
    Repeater {
        model: labels.positioned
        delegate: Label {
            required property var modelData
            objectName: "sceneLabel:" + modelData.id
            x:modelData.x; y:modelData.y; width:modelData.width; height:modelData.height
            text:modelData.text; textFormat:Text.PlainText; elide:Text.ElideRight
            padding:5; leftPadding:7; rightPadding:7; font.pixelSize:12
            color:modelData.selected ? Theme.text : "#DCE5E9"
            background: Rectangle { color:"#E0253139"; radius:3; border.color: modelData.selected ? Theme.text : modelData.hovered || modelData.related ? "#B7CBD4" : "#51616A" }
            Accessible.role: Accessible.StaticText
            Accessible.name: modelData.text + (modelData.selected ? ", selected" : "")
            ToolTip.visible: labelHover.hovered && truncated
            ToolTip.text: modelData.text
            HoverHandler { id: labelHover; blocking: false }
        }
    }
}

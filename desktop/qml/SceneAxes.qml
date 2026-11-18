pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls

Item {
    id: axes
    required property var projector
    required property vector3d focusPoint
    property var viewState: []
    property bool gridVisible: false
    property real gridStep: 20
    property var projectedAxes: []
    width: 84; height: 86
    objectName: "sceneOrientation"
    Accessible.role: Accessible.StaticText
    Accessible.name: "Scene orientation axes. Layout coordinates, not geographic directions."

    function requestRefresh() {
        if (visible && refreshTimer) refreshTimer.restart()
    }
    function refreshAxes() {
        if (!projector || projector.width <= 0 || projector.height <= 0) return
        const origin = projector.mapFrom3DScene(focusPoint)
        const directions = [{name:"X",x:50,y:0,z:0,color:"#F3F3F3"},
            {name:"Y",x:0,y:50,z:0,color:"#B7CBD4"}, {name:"Z",x:0,y:0,z:50,color:"#C3D5C8"}]
        const vectors = directions.map(axis => {
            const p = projector.mapFrom3DScene(Qt.vector3d(focusPoint.x + axis.x, focusPoint.y + axis.y, focusPoint.z + axis.z))
            return {name:axis.name,x:p.x-origin.x,y:p.y-origin.y,color:axis.color}
        })
        const span = Math.max(1, ...vectors.map(axis => Math.sqrt(axis.x*axis.x + axis.y*axis.y)))
        projectedAxes = vectors.filter(axis => isFinite(axis.x) && isFinite(axis.y) && Math.sqrt(axis.x*axis.x + axis.y*axis.y) > span * 0.16)
            .map(axis => ({name:axis.name,x:40+axis.x*24/span,y:35+axis.y*24/span,color:axis.color}))
        axesCanvas.requestPaint()
    }
    onViewStateChanged: requestRefresh()
    onVisibleChanged: requestRefresh()
    // Coalesce camera bindings before drawing. Updating after frameSwapped
    // would schedule a second full scene render for every camera movement.
    Timer { id: refreshTimer; interval: 0; repeat: false; onTriggered: { if (axes.visible) axes.refreshAxes() } }
    Component.onCompleted: requestRefresh()
    Rectangle { anchors.fill: parent; color: "#B315191B"; radius: 6; border.color: "#26FFFFFF" }
    Canvas {
        id: axesCanvas; anchors.fill: parent
        onPaint: {
            const ctx = getContext("2d"); ctx.clearRect(0, 0, width, height)
            ctx.lineWidth = 1.5
            for (const axis of axes.projectedAxes) {
                ctx.strokeStyle = axis.color; ctx.beginPath(); ctx.moveTo(40,35); ctx.lineTo(axis.x,axis.y); ctx.stroke()
            }
            ctx.fillStyle = "#F3F3F3"; ctx.beginPath(); ctx.arc(40,35,2,0,Math.PI*2); ctx.fill()
        }
    }
    Repeater {
        model: axes.projectedAxes
        delegate: Label {
            required property var modelData
            x: modelData.x - width / 2; y: modelData.y - height / 2
            text: modelData.name; color: modelData.color; font.pixelSize: 10; font.bold: true
            padding: 1; background: Rectangle { color: "#DA15191B"; radius: 2 }
            Accessible.ignored: true
        }
    }
    Label { anchors.horizontalCenter: parent.horizontalCenter; anchors.bottom: parent.bottom; anchors.bottomMargin: 7; text: axes.gridVisible ? "GRID " + axes.gridStep : "LAYOUT"; color: Theme.secondary; font.pixelSize: 9; font.letterSpacing: 0.8 }
}

import QtQuick
import QtQuick3D

Node {
    id: route
    required property var routeData
    property string draggedId: ""
    property vector3d draggedPosition: Qt.vector3d(0, 0, 0)
    property var draggedIds: []
    property vector3d dragDelta: Qt.vector3d(0, 0, 0)
    property bool detailed: true
    readonly property real sx: draggedIds.indexOf(routeData.source) >= 0 ? routeData.sx + dragDelta.x : draggedId === routeData.source ? draggedPosition.x : routeData.sx
    readonly property real sz: draggedIds.indexOf(routeData.source) >= 0 ? routeData.sz + dragDelta.z : draggedId === routeData.source ? draggedPosition.z : routeData.sz
    readonly property real tx: draggedIds.indexOf(routeData.target) >= 0 ? routeData.tx + dragDelta.x : draggedId === routeData.target ? draggedPosition.x : routeData.tx
    readonly property real tz: draggedIds.indexOf(routeData.target) >= 0 ? routeData.tz + dragDelta.z : draggedId === routeData.target ? draggedPosition.z : routeData.tz
    readonly property real dx: tx - sx
    readonly property real dz: tz - sz
    readonly property real length: Math.sqrt(dx * dx + dz * dz)
    readonly property quaternion direction: length > 0.01 ? Qt.quaternion(Math.SQRT1_2, Math.SQRT1_2 * dz / length, 0, -Math.SQRT1_2 * dx / length) : Qt.quaternion(1, 0, 0, 0)
    readonly property color pipeColor: !routeData.available ? "#eb8585" : routeData.selected ? "#edf1f3" : (routeData.flow > 0 ? "#a7bdc5" : "#677780")
    IndustrialPipe {
        visible: route.length > 0.01
        pickName: "route:" + route.routeData.id
        span: route.length
        tint: route.pipeColor
        fittings: route.detailed
        position: Qt.vector3d((route.sx + route.tx) / 2, 8, (route.sz + route.tz) / 2)
        rotation: route.direction
    }
    Repeater3D {
        model: route.detailed && route.length > 90 ? [0.25, 0.75] : []
        delegate: Node {
            required property real modelData
            position: Qt.vector3d(route.sx + route.dx * modelData, 0, route.sz + route.dz * modelData)
            Model { source: "#Cube"; y: 1; scale: Qt.vector3d(0.14, 0.025, 0.14); materials: PrincipledMaterial { baseColor: "#A9ABA2"; roughness: 0.95 } }
            Model { source: "#Cube"; y: 4; scale: Qt.vector3d(0.025, 0.07, 0.025); materials: PrincipledMaterial { baseColor: "#74828A"; metalness: 0.25; roughness: 0.6 } }
        }
    }
    Model {
        visible: route.length > 0.01 && route.routeData.flow > 0
        objectName: "route:" + route.routeData.id
        source: "#Cone"
        position: Qt.vector3d(route.sx + route.dx * 0.62, 8, route.sz + route.dz * 0.62)
        rotation: route.direction
        scale: Qt.vector3d(0.16, 0.22, 0.16)
        pickable: true
        materials: PrincipledMaterial { baseColor: route.pipeColor; roughness: 0.7 }
    }
    IndustrialEquipment {
        visible: route.routeData.pump
        kind: "pump"; pickName: "route:" + route.routeData.id
        position: Qt.vector3d((route.sx + route.tx) / 2, 0, (route.sz + route.tz) / 2)
        eulerRotation.y: Math.atan2(-route.dz, route.dx) * 180 / Math.PI
        scale: Qt.vector3d(0.8, 0.8, 0.8)
        selected: route.routeData.selected; abnormal: !route.routeData.available
        detailed: route.detailed || route.routeData.selected
        showMarker: false
    }
}

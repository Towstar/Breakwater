import QtQuick
import QtQuick3D

Node {
    id: route
    required property var routeData
    property string draggedId: ""
    property vector3d draggedPosition: Qt.vector3d(0, 0, 0)
    readonly property real sx: draggedId === routeData.source ? draggedPosition.x : routeData.sx
    readonly property real sz: draggedId === routeData.source ? draggedPosition.z : routeData.sz
    readonly property real tx: draggedId === routeData.target ? draggedPosition.x : routeData.tx
    readonly property real tz: draggedId === routeData.target ? draggedPosition.z : routeData.tz
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
        position: Qt.vector3d((route.sx + route.tx) / 2, 8, (route.sz + route.tz) / 2)
        rotation: route.direction
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
        showMarker: false
    }
}

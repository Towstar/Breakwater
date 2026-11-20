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
            eulerRotation.y: Math.atan2(-route.dz, route.dx) * 180 / Math.PI
            PrincipledMaterial { id: supportSteel; baseColor: "#677982"; metalness: 0.3; roughness: 0.65 }
            Model { source: "#Cube"; y: -1; scale: Qt.vector3d(0.24, 0.04, 0.26); materials: PrincipledMaterial { baseColor: "#AEBBBC"; roughness: 0.95 } }
            Model { source: "#Cube"; y: 2.8; scale: Qt.vector3d(0.06, 0.036, 0.06); materials: supportSteel }
            Model { source: "#Cube"; y: 4.5; scale: Qt.vector3d(0.09, 0.012, 0.15); materials: supportSteel }
            Repeater3D {
                model: [-1, 1]
                delegate: Node {
                    required property real modelData
                    Model { source: "#Cube"; y: 7; z: modelData * 3.9; scale: Qt.vector3d(0.08, 0.05, 0.012); materials: supportSteel }
                    Model { source: "#Cylinder"; y: 5.5; z: modelData * 6; scale: Qt.vector3d(0.018, 0.025, 0.018); materials: supportSteel }
                }
            }
        }
    }
    Model {
        visible: route.length > 0.01 && route.routeData.available && route.routeData.flow > 0
        objectName: "route:" + route.routeData.id
        source: "#Cone"
        position: Qt.vector3d(route.sx + route.dx * 0.62, 8, route.sz + route.dz * 0.62)
        rotation: route.direction
        scale: Qt.vector3d(0.16, 0.22, 0.16)
        pickable: true
        materials: PrincipledMaterial { baseColor: route.pipeColor; roughness: 0.7 }
    }
    SitePad {
        // Primary availability colour and picking remain separate from the cue.
        visible: route.routeData.pump && (route.detailed || route.routeData.selected)
        kind: "pump"; selected: route.routeData.selected; detailed: route.detailed
        position: Qt.vector3d((route.sx + route.tx) / 2, 0, (route.sz + route.tz) / 2)
        eulerRotation.y: Math.atan2(-route.dz, route.dx) * 180 / Math.PI
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
    // Selected-only source -> target cue, including failed/zero-setting routes.
    // This is static schematic direction, never measured or simulated delivery.
    Loader3D {
        objectName: "selectedRouteCue:" + route.routeData.id
        active: route.routeData.selected && Number.isFinite(route.length) && route.length > 0.01
        visible: active
        position: Qt.vector3d(route.sx + route.dx * 0.62, 19, route.sz + route.dz * 0.62)
        rotation: route.direction
        scale: Qt.vector3d(Math.min(1, route.length / 80), Math.min(1, route.length / 80), Math.min(1, route.length / 80))
        sourceComponent: Node {
            PrincipledMaterial { id: directionPaint; baseColor: "#EDF1F3"; lighting: PrincipledMaterial.NoLighting }
            Model { objectName: "selectedRouteCueShaft:" + route.routeData.id; source: "#Cube"; y: -7; scale: Qt.vector3d(0.055, 0.22, 0.025); materials: directionPaint; pickable: false; castsShadows: false }
            Model { objectName: "selectedRouteCueHead:" + route.routeData.id; source: "#Cone"; y: 10; scale: Qt.vector3d(0.15, 0.16, 0.10); materials: directionPaint; pickable: false; castsShadows: false }
        }
    }
}

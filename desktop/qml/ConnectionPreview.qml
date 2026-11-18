import QtQuick
import QtQuick3D

Node {
    id: preview
    property vector3d start: Qt.vector3d(0, 8, 0)
    property vector3d end: Qt.vector3d(0, 8, 0)
    property bool allowed: false
    readonly property real dx: end.x - start.x
    readonly property real dz: end.z - start.z
    readonly property real span: Math.sqrt(dx * dx + dz * dz)
    Model {
        visible: preview.span > 1; source: "#Cylinder"; pickable: false; castsShadows: false
        position: Qt.vector3d((preview.start.x + preview.end.x) / 2, 13, (preview.start.z + preview.end.z) / 2)
        rotation: preview.span > 0.01 ? Qt.quaternion(Math.SQRT1_2, Math.SQRT1_2 * preview.dz / preview.span, 0, -Math.SQRT1_2 * preview.dx / preview.span) : Qt.quaternion(1, 0, 0, 0)
        scale: Qt.vector3d(0.035, preview.span / 100, 0.035)
        materials: PrincipledMaterial { baseColor: preview.allowed ? "#61896F" : "#BF685E"; emissiveFactor: Qt.vector3d(0.1, 0.1, 0.1); roughness: 0.8 }
    }
}

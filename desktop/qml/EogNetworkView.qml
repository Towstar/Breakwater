import QtQuick
import QtQuick.Controls
import QtQuick3D

Rectangle {
    id: network
    required property var eog
    property bool sceneReady: false
    property real yaw: 24
    property real pitch: -54
    property real distance: 1050
    color: "#101d28"
    radius: 6
    clip: true
    border.color: "#365261"
    implicitHeight: 300
    onVisibleChanged: { sceneReady = false; if (visible) settle.restart() }
    Connections {
        target: network.eog
        function onChanged() { if (network.visible && !network.sceneReady) settle.restart() }
    }
    Timer { id: settle; interval: 300; onTriggered: network.sceneReady = network.visible && network.eog.loaded }

    View3D {
        id: view
        anchors.fill: parent
        environment: SceneEnvironment {
            backgroundMode: SceneEnvironment.Color; clearColor: "#101d28"
            antialiasingMode: SceneEnvironment.MSAA
            antialiasingQuality: SceneEnvironment.Medium
        }
        Node {
            id: rig
            eulerRotation: Qt.vector3d(network.pitch, network.yaw, 0)
            PerspectiveCamera { id: camera; z: network.distance; clipNear: 1; clipFar: 8000 }
        }
        camera: camera
        DirectionalLight { eulerRotation: Qt.vector3d(-50, -30, 0); brightness: 1.3; ambientColor: "#8295a5" }
        Model {
            source: "#Rectangle"; eulerRotation.x: -90; scale: Qt.vector3d(15, 12, 1)
            materials: PrincipledMaterial { baseColor: "#1c3040"; roughness: 1 }
        }
        Repeater3D {
            model: network.eog.routes
            delegate: Node {
                id: route
                required property var modelData
                readonly property real dx: modelData.tx - modelData.sx
                readonly property real dz: modelData.tz - modelData.sz
                readonly property real length: Math.sqrt(dx * dx + dz * dz)
                readonly property quaternion direction: length > 0.001 ? Qt.quaternion(Math.SQRT1_2, Math.SQRT1_2 * dz / length, 0, -Math.SQRT1_2 * dx / length) : Qt.quaternion(1, 0, 0, 0)
                Model {
                    visible: route.length > 0.001
                    objectName: "eog-asset:" + route.modelData.target
                    source: "#Cylinder"; pickable: true
                    position: Qt.vector3d((route.modelData.sx + route.modelData.tx) / 2, 12, (route.modelData.sz + route.modelData.tz) / 2)
                    rotation: route.direction; scale: Qt.vector3d(0.055, route.length / 100, 0.055)
                    materials: PrincipledMaterial { baseColor: "#568995"; roughness: 0.8 }
                }
                Model {
                    visible: route.length > 0.001
                    source: "#Cone"
                    position: Qt.vector3d(route.modelData.sx + route.dx * 0.65, 12, route.modelData.sz + route.dz * 0.65)
                    rotation: route.direction; scale: Qt.vector3d(0.13, 0.20, 0.13)
                    materials: PrincipledMaterial { baseColor: "#8fc0c5"; roughness: 0.8 }
                }
            }
        }
        Repeater3D {
            model: network.eog.nodes
            delegate: Node {
                id: equipment
                required property var modelData
                readonly property string kind: modelData.kind
                readonly property bool tank: kind === "tank"
                readonly property real fraction: tank && !modelData.unknown && modelData.capacity > 0 ? modelData.volume / modelData.capacity : -1
                readonly property real displayFill: isFinite(fraction) && fraction >= 0 ? Math.min(1, fraction) : 0
                readonly property color tint: modelData.selected ? "#ffcf70" : kind === "well" ? "#cfa66e" : kind === "tank" ? "#90a7b7" : kind === "pump" ? "#7fbba5" : kind === "disposal" ? "#7295c4" : "#74a9b6"
                position: Qt.vector3d(modelData.x, 0, modelData.z)
                visible: kind !== "facility"
                Model {
                    objectName: "eog-asset:" + equipment.modelData.id
                    source: equipment.tank || equipment.kind === "disposal" ? "#Cylinder" : equipment.kind === "well" ? "#Cone" : "#Cube"
                    position: Qt.vector3d(0, equipment.tank ? 36 : 24, 0)
                    scale: equipment.tank ? Qt.vector3d(0.55, 0.72, 0.55) : equipment.kind === "pipeline" ? Qt.vector3d(0.85, 0.14, 0.20) : Qt.vector3d(0.40, 0.48, 0.40)
                    pickable: true
                    materials: PrincipledMaterial { baseColor: equipment.tint; roughness: 0.8 }
                }
                Model {
                    visible: equipment.tank && equipment.displayFill > 0
                    objectName: "eog-asset:" + equipment.modelData.id
                    source: "#Cylinder"; pickable: true
                    position: Qt.vector3d(0, 36 * equipment.displayFill, 0)
                    scale: Qt.vector3d(0.558, 0.72 * equipment.displayFill, 0.558)
                    materials: PrincipledMaterial { baseColor: equipment.fraction > 1 ? "#ee9d70" : "#36bec8"; roughness: 0.6 }
                }
                Model {
                    source: "#Cylinder"; position: Qt.vector3d(0, 1, 0)
                    scale: Qt.vector3d(equipment.modelData.selected ? 0.75 : 0.62, 0.025, equipment.modelData.selected ? 0.75 : 0.62)
                    materials: PrincipledMaterial { baseColor: equipment.modelData.selected ? "#ffc96b" : "#344c5d"; roughness: 1 }
                }
            }
        }
    }
    Repeater {
        model: network.eog.nodes
        delegate: Label {
            required property var modelData
            property vector3d point: {
                if (!network.sceneReady) return Qt.vector3d(0, 0, 0)
                const watchRotation = rig.eulerRotation
                const watchDistance = camera.z
                const watchSize = view.width + view.height
                return view.mapFrom3DScene(Qt.vector3d(modelData.x, modelData.kind === "tank" ? 105 : 80, modelData.z))
            }
            visible: network.sceneReady && modelData.kind !== "facility" && point.z > 0
            x: point.x - width / 2; y: point.y; width: Math.min(160, implicitWidth)
            text: modelData.label || modelData.id; textFormat: Text.PlainText
            elide: Text.ElideMiddle; horizontalAlignment: Text.AlignHCenter
            color: modelData.selected ? "#ffcf70" : "#dceaf0"; font.pixelSize: 10
        }
    }
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        property point previous
        property point pressPoint
        onPressed: function(mouse) { previous = Qt.point(mouse.x, mouse.y); pressPoint = previous }
        onPositionChanged: function(mouse) {
            if (pressedButtons & Qt.RightButton) {
                network.yaw -= (mouse.x - previous.x) * 0.4
                network.pitch = Math.max(-85, Math.min(-20, network.pitch - (mouse.y - previous.y) * 0.3))
            }
            previous = Qt.point(mouse.x, mouse.y)
        }
        onReleased: function(mouse) {
            if (mouse.button !== Qt.LeftButton || Math.abs(mouse.x - pressPoint.x) + Math.abs(mouse.y - pressPoint.y) > 6) return
            const hit = view.pick(mouse.x, mouse.y)
            if (hit.objectHit && hit.objectHit.objectName.indexOf("eog-asset:") === 0)
                network.eog.selectAsset(hit.objectHit.objectName.substring(10))
        }
        onWheel: function(wheel) { network.distance = Math.max(400, Math.min(2600, network.distance - wheel.angleDelta.y * 0.8)) }
    }
    Label {
        anchors.left: parent.left; anchors.top: parent.top; anchors.margins: 10
        text: "FACILITY VIEW · select equipment · right-drag to orbit · scroll to zoom"
        color: "#b7cbd5"; font.pixelSize: 10
    }
    Label {
        anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: 10
        text: "Display layout only · recorded values stay unchanged"
        color: "#94aebb"; font.pixelSize: 10
    }
}

pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Window
import QtQuick3D

Rectangle {
    id: network
    required property var eog
    readonly property bool overview: eog.sceneMode === "overview"
    readonly property var sceneNodes: eog.sceneNodes
    readonly property var sceneRoutes: eog.sceneRoutes
    readonly property var sceneBounds: eog.sceneBounds
    property bool sceneReady: false
    property bool projectionReady: false
    property int sceneRevision: 0
    property int renderedRevision: -1
    property int renderedFrames: 0
    property real yaw: 24
    property real pitch: -54
    property real zoomFactor: 1
    readonly property real fitDistance: Math.max(420, (sceneBounds.radius || 220) * 2.2)
    readonly property real distance: fitDistance * zoomFactor
    property int visibleSceneLabels: 0
    color: "#11171c"; radius: 6; clip: true; border.color: "#41494f"
    implicitHeight: 320
    objectName: "eogNetworkView"

    function invalidateScene() {
        sceneRevision += 1; renderedFrames = 0; sceneReady = false; projectionReady = false; visibleSceneLabels = 0
        if (visible && network.Window.window) network.Window.window.update()
    }
    function visibleNode(row) { return overview || row.kind !== "facility" }
    function withinVisibleClip(label: Item): bool {
        if (!label.visible) return false
        let ancestor = label.parent
        while (ancestor) {
            if (ancestor.clip) {
                const p = label.mapToItem(ancestor, label.width / 2, label.height / 2)
                if (p.x < 0 || p.y < 0 || p.x > ancestor.width || p.y > ancestor.height) return false
            }
            ancestor = ancestor.parent
        }
        const host = network.Window.window
        const point = label.mapToItem(null, label.width / 2, label.height / 2)
        return host && point.x >= 0 && point.y >= 0 && point.x <= host.width && point.y <= host.height
    }
    function countVisibleLabels() {
        let count = 0
        for (let i = 0; i < nodeLabels.count; ++i) {
            const label = nodeLabels.itemAt(i) as SceneLabel
            if (!label || !visibleNode(label.modelData)) continue
            const p = label.projectedPoint
            if (label.text.length > 0 && label.width > 0 && label.height > 0
                    && isFinite(p.x) && isFinite(p.y) && p.z > 0
                    && p.x >= 0 && p.x <= view.width && p.y >= 0 && p.y <= view.height
                    && withinVisibleClip(label)) count += 1
        }
        return count
    }
    function observeFrame() {
        if (!visible || !eog.loaded || sceneNodes.length === 0 || width <= 0 || height <= 0) return
        if (renderedRevision !== sceneRevision) { renderedRevision = sceneRevision; renderedFrames = 0 }
        renderedFrames += 1
        if (!projectionReady) {
            projectionReady = true
            if (network.Window.window) network.Window.window.update()
            return
        }
        // Projection becomes available after a rendered frame; the next frame
        // must include actual positioned labels before acceptance reports ready.
        visibleSceneLabels = countVisibleLabels()
        sceneReady = renderedFrames >= 2 && visibleSceneLabels > 0
    }
    onVisibleChanged: invalidateScene()
    onWidthChanged: invalidateScene()
    onHeightChanged: invalidateScene()
    onYawChanged: invalidateScene()
    onPitchChanged: invalidateScene()
    onZoomFactorChanged: invalidateScene()
    onOverviewChanged: { zoomFactor = 1; invalidateScene() }
    Connections { target: network.eog; function onChanged() { network.invalidateScene() } }
    Connections {
        target: network.Window.window
        function onFrameSwapped() { Qt.callLater(network.observeFrame) }
    }

    View3D {
        id: view; anchors.fill: parent
        environment: SceneEnvironment {
            backgroundMode: SceneEnvironment.Color; clearColor: "#11171c"
            antialiasingMode: SceneEnvironment.MSAA; antialiasingQuality: SceneEnvironment.Medium
        }
        Node {
            id: rig
            position: Qt.vector3d(network.sceneBounds.center_x || 0, 0, network.sceneBounds.center_z || 0)
            eulerRotation: Qt.vector3d(network.pitch, network.yaw, 0)
            PerspectiveCamera { id: eogCamera; z: network.distance; clipNear: 1; clipFar: Math.max(8000, network.fitDistance * 8) }
        }
        camera: eogCamera
        DirectionalLight { eulerRotation: Qt.vector3d(-50, -30, 0); brightness: 1.3; ambientColor: "#9099a1" }
        Model {
            source: "#Rectangle"
            position: Qt.vector3d(network.sceneBounds.center_x || 0, -1, network.sceneBounds.center_z || 0)
            eulerRotation.x: -90
            scale: Qt.vector3d(Math.max(12, (network.sceneBounds.width || 600) / 80), Math.max(8, (network.sceneBounds.depth || 400) / 80), 1)
            materials: PrincipledMaterial { baseColor: "#263139"; roughness: 1 }
        }
        Repeater3D {
            model: network.sceneRoutes
            delegate: Node {
                id: route
                required property var modelData
                readonly property real dx: modelData.tx - modelData.sx
                readonly property real dz: modelData.tz - modelData.sz
                readonly property bool truckAccess: modelData.route_kind === "truck_access"
                readonly property real length: Math.sqrt(dx * dx + dz * dz)
                readonly property quaternion direction: length > 0.001 ? Qt.quaternion(Math.SQRT1_2, Math.SQRT1_2 * dz / length, 0, -Math.SQRT1_2 * dx / length) : Qt.quaternion(1, 0, 0, 0)
                IndustrialPipe {
                    visible: route.length > 0.001 && !route.truckAccess
                    pickName: "eog-asset:" + route.modelData.target
                    span: route.length; tint: "#90a8b0"
                    position: Qt.vector3d((route.modelData.sx + route.modelData.tx) / 2, 12, (route.modelData.sz + route.modelData.tz) / 2)
                    rotation: route.direction
                }
                Model {
                    visible: route.length > 0.001 && route.truckAccess
                    objectName: "eog-asset:" + route.modelData.target
                    source: "#Cube"; pickable: true
                    position: Qt.vector3d((route.modelData.sx + route.modelData.tx) / 2, 0.5, (route.modelData.sz + route.modelData.tz) / 2)
                    eulerRotation.y: Math.atan2(-route.dz, route.dx) * 180 / Math.PI
                    scale: Qt.vector3d(route.length / 100, 0.008, 0.075)
                    materials: PrincipledMaterial { baseColor: "#45414b"; roughness: 1 }
                }
                Repeater3D {
                    model: route.truckAccess && route.length > 20 ? [0.2, 0.4, 0.6, 0.8] : []
                    delegate: Model {
                        required property var modelData
                        source: "#Cube"; pickable: false
                        position: Qt.vector3d(route.modelData.sx + route.dx * modelData, 1.1, route.modelData.sz + route.dz * modelData)
                        eulerRotation.y: Math.atan2(-route.dz, route.dx) * 180 / Math.PI
                        scale: Qt.vector3d(Math.min(10, route.length * 0.08) / 100, 0.006, 0.012)
                        materials: PrincipledMaterial { baseColor: "#b7aeba"; roughness: 1 }
                    }
                }
                Model {
                    // Static topology direction; acquisition supplies no live
                    // flow for these schematic links, so nothing is animated.
                    visible: route.length > 0.001 && !route.truckAccess; source: "#Cone"; pickable: false
                    position: Qt.vector3d(route.modelData.sx + route.dx * 0.65, 12, route.modelData.sz + route.dz * 0.65)
                    rotation: route.direction; scale: Qt.vector3d(0.13, 0.20, 0.13)
                    materials: PrincipledMaterial { baseColor: "#b5c9d1"; roughness: 0.65 }
                }
            }
        }
        Repeater3D {
            model: network.sceneNodes
            delegate: Node {
                id: equipment
                required property var modelData
                readonly property string kind: modelData.kind
                readonly property bool tank: kind === "tank"
                readonly property real fraction: tank && typeof modelData.volume === "number" && modelData.capacity > 0 ? modelData.volume / modelData.capacity : NaN
                // The shell is finite geometry. Raw negative/overflow values
                // remain unmodified in details, charts and warning colour.
                readonly property real displayFill: isFinite(fraction) ? Math.max(0, Math.min(1, fraction)) : 0
                readonly property bool abnormal: isFinite(fraction) && (fraction < 0 || fraction > 1)
                position: Qt.vector3d(modelData.x, 0, modelData.z)
                visible: network.visibleNode(modelData)
                IndustrialEquipment {
                    kind: equipment.kind
                    pickName: "eog-asset:" + equipment.modelData.id
                    selected: equipment.modelData.selected
                    abnormal: equipment.abnormal
                    fillFraction: equipment.fraction
                }
            }
        }
    }
    component SceneLabel: Label {
            required property var modelData
            readonly property vector3d projectedPoint: {
                if (!network.projectionReady) return Qt.vector3d(0, 0, -1)
                const watchRotation = rig.eulerRotation, watchDistance = eogCamera.z
                const watchCenter = rig.position, watchSize = view.width + view.height
                return view.mapFrom3DScene(Qt.vector3d(modelData.x, modelData.kind === "tank" ? 105 : 80, modelData.z))
            }
            objectName: "eogLabel:" + modelData.id
            visible: network.projectionReady && network.visibleNode(modelData) && projectedPoint.z > 0
            x: projectedPoint.x - width / 2; y: projectedPoint.y; width: Math.min(180, implicitWidth)
            text: modelData.label || modelData.id
            textFormat: Text.PlainText; elide: Text.ElideMiddle; horizontalAlignment: Text.AlignHCenter
            color: modelData.selected ? "#edf1f3" : "#c8d1d8"; font.pixelSize: 12
    }
    Repeater {
        id: nodeLabels
        model: network.sceneNodes
        delegate: SceneLabel {}
    }
    MouseArea {
        anchors.fill: parent; acceptedButtons: Qt.LeftButton | Qt.RightButton
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
                network.eog.pickSceneNode(hit.objectHit.objectName.substring(10))
        }
        onWheel: function(wheel) { network.zoomFactor = Math.max(0.45, Math.min(3, network.zoomFactor - wheel.angleDelta.y / 120 * 0.10)) }
    }
    Label {
        anchors.left: parent.left; anchors.top: parent.top; anchors.margins: 10; width: Math.max(0, parent.width - 20)
        text: network.overview ? "SYSTEMS · select a facility to inspect equipment · right-drag to orbit"
                               : "FACILITY · select equipment · right-drag to orbit · scroll to zoom"
        wrapMode: Text.Wrap; color: "#b7cbd5"; font.pixelSize: 10
    }
    Label {
        anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: 10; width: Math.max(0, parent.width - 20)
        text: network.overview ? "Pipe: water connection · dashed: truck access · travel times from ops.json"
                               : "Schematic equipment and topology · raw observations remain unchanged"
        horizontalAlignment: Text.AlignRight; wrapMode: Text.Wrap; color: "#94aebb"; font.pixelSize: 10
    }
}

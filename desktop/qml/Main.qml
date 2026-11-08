import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtQuick3D

ApplicationWindow {
    id: app
    required property var controller
    required property var investigator
    required property var history
    required property var scientificWorker
    required property var pipeTransport
    required property var storageScience
    readonly property bool storageScienceVisible: showAgent && agentPanel.scienceVisible
    readonly property bool pipeTransportVisible: pipeTransportDialog.visible
    readonly property bool compactUi: Qt.application.arguments.indexOf("--compact-ui") >= 0
    property bool showAgent: false
    width: compactUi ? 1100 : 1440
    height: compactUi ? 700 : 900
    minimumWidth: 1100
    minimumHeight: 700
    visible: true
    title: "BREAKWATER | Synthetic fluid network workbench"
    color: "#101a24"
    palette.window: "#101a24"
    palette.base: "#172735"
    palette.button: "#243b4a"
    palette.buttonText: "#e3edf2"
    palette.text: "#e3edf2"
    palette.windowText: "#e3edf2"
    palette.highlight: "#3db9bc"
    property bool connectMode: false
    property string connectionSource: ""
    property real yaw: 35
    property real pitch: -55
    property real distance: 720
    property bool sceneReady: false
    readonly property int visibleAssetLabels: {
        let count = 0
        for (let i = 0; i < labelRepeater.count; ++i) {
            const item = labelRepeater.itemAt(i)
            if (item && item.visible) ++count
        }
        return count
    }
    onFrameSwapped: { if (!sceneReady) sceneReady = true }

    function groundPoint(x, y) {
        const a = view.mapTo3DScene(Qt.vector3d(x, y, 10))
        const b = view.mapTo3DScene(Qt.vector3d(x, y, 1500))
        const denominator = b.y - a.y
        if (Math.abs(denominator) < 0.0001) return Qt.vector3d(0, 0, 0)
        const t = -a.y / denominator
        return Qt.vector3d(a.x + t * (b.x - a.x), 0, a.z + t * (b.z - a.z))
    }
    function openPipeTransport() { pipeTransportDialog.open() }
    function openStorageScience() { showAgent = true; agentPanel.showScience() }
    function clickAsset(id) {
        if (connectMode) {
            if (connectionSource.length === 0) connectionSource = id
            else { controller.connectAssets(connectionSource, id); connectionSource = ""; connectMode = false }
        } else controller.select(id)
    }
    Component.onCompleted: { yaw = controller.camera.yaw; pitch = controller.camera.pitch; distance = controller.camera.distance; cameraRig.x = controller.camera.x; cameraRig.z = controller.camera.z }
    Connections {
        target: app.controller
        function onCameraChanged() { app.yaw = app.controller.camera.yaw; app.pitch = app.controller.camera.pitch; app.distance = app.controller.camera.distance; cameraRig.x = app.controller.camera.x; cameraRig.z = app.controller.camera.z }
    }
    Shortcut { sequence: StandardKey.Undo; onActivated: controller.undo() }
    Shortcut { sequence: StandardKey.Redo; onActivated: controller.redo() }
    Shortcut { sequence: "Escape"; onActivated: { app.connectMode = false; app.connectionSource = "" } }

    FileDialog { id: openDialog; nameFilters: ["BREAKWATER project (*.json)"]; onAccepted: controller.load(selectedFile) }
    FileDialog { id: saveDialog; fileMode: FileDialog.SaveFile; defaultSuffix: "json"; nameFilters: ["BREAKWATER project (*.json)"]; onAccepted: controller.save(selectedFile) }
    PipeTransportPanel { id: pipeTransportDialog; pipeTransport: app.pipeTransport }

    header: ToolBar {
        height: 64
        background: Rectangle { color: "#172735" }
        RowLayout {
            anchors.fill: parent; anchors.margins: 12; spacing: 8
            Label { text: "BREAKWATER"; font.pixelSize: 21; font.bold: true; color: "#60d2d1"; Layout.rightMargin: 12 }
            Button { Layout.preferredWidth: 72; text: "Reset"; onClicked: controller.reset() }
            Button { text: "Pump failure"; onClicked: controller.loadExample("baseline") }
            Button { text: "Bad reroute"; onClicked: controller.loadExample("bad") }
            Button { text: "Mixed plan"; onClicked: controller.loadExample("mixed") }
            Item { Layout.fillWidth: true }
            Button { Layout.preferredWidth: 64; text: "Undo"; enabled: controller.canUndo; onClicked: controller.undo() }
            Button { Layout.preferredWidth: 64; text: "Redo"; enabled: controller.canRedo; onClicked: controller.redo() }
            Button { Layout.preferredWidth: 64; text: "Open"; onClicked: openDialog.open() }
            Button { Layout.preferredWidth: 64; text: "Save"; onClicked: saveDialog.open() }
            Button { text: controller.busy ? "Calculating..." : "Run forecast"; enabled: !controller.busy && controller.draftIssues.length === 0; highlighted: true; onClicked: controller.forecast() }
        }
    }

    ColumnLayout {
        anchors.fill: parent; spacing: 0
        RowLayout {
            Layout.fillWidth: true; Layout.fillHeight: true; spacing: 1
            Rectangle {
                Layout.preferredWidth: 176; Layout.fillHeight: true; color: "#142330"
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 12; spacing: 8
                    ScrollView {
                        id: equipmentScroll
                        Layout.fillWidth: true; Layout.fillHeight: true
                        contentWidth: availableWidth; clip: true
                        ColumnLayout {
                            width: equipmentScroll.availableWidth; spacing: 8
                    Label { text: "EQUIPMENT"; font.pixelSize: 11; font.letterSpacing: 2; color: "#9eb5c2" }
                    Label { text: "Drag into the scene\nor click to add."; font.pixelSize: 11; color: "#8ca5b4" }
                    Repeater {
                        model: [{kind:"tank", label:"Storage tank"}, {kind:"source",label:"Production source"}, {kind:"treatment",label:"Treatment"}, {kind:"trucking",label:"Tanker outlet"}, {kind:"pump",label:"Pump on route"}]
                        delegate: Rectangle {
                            required property var modelData
                            Layout.fillWidth: true; implicitHeight: 34; radius: 4; color: paletteMouse.containsMouse ? "#315164" : "#223b4c"
                            Label { anchors.centerIn: parent; text: modelData.label; textFormat: Text.PlainText; font.pixelSize: 12 }
                            MouseArea {
                                id: paletteMouse
                                anchors.fill: parent; hoverEnabled: true; preventStealing: true
                                property point start
                                onPressed: function(mouse) {
                                    start = Qt.point(mouse.x, mouse.y)
                                    const p = mapToItem(app.contentItem, mouse.x, mouse.y)
                                    ghost.kind = modelData.kind; ghost.label = modelData.label; ghost.x = p.x - 60; ghost.y = p.y - 16
                                }
                                onPositionChanged: function(mouse) {
                                    if (!pressed) return
                                    if (Math.abs(mouse.x - start.x) + Math.abs(mouse.y - start.y) > 8) {
                                        const p = mapToItem(app.contentItem, mouse.x, mouse.y)
                                        ghost.visible = true; ghost.x = p.x - 60; ghost.y = p.y - 16; ghost.Drag.active = true
                                    }
                                }
                                onReleased: {
                                    if (ghost.Drag.active) ghost.Drag.drop()
                                    else if (modelData.kind === "pump") controller.attachPump(controller.selectedId)
                                    else controller.addAsset(modelData.kind, 0, 0)
                                    ghost.Drag.active = false; ghost.visible = false
                                }
                                onCanceled: { ghost.Drag.active = false; ghost.visible = false }
                            }
                        }
                    }
                    Button {
                        Layout.fillWidth: true; text: app.connectMode ? "Cancel connection" : "Connect equipment"
                        onClicked: { app.connectMode = !app.connectMode; app.connectionSource = "" }
                    }
                    Label { text: "NETWORK"; font.pixelSize: 11; font.letterSpacing: 2; color: "#9eb5c2"; Layout.topMargin: 10 }
                    ListView {
                        Layout.fillWidth: true; Layout.preferredHeight: 160; clip: true; spacing: 4
                        model: controller.assets
                        delegate: ItemDelegate {
                            required property var modelData
                            width: ListView.view.width
                            text: modelData.label; highlighted: modelData.selected
                            contentItem: Label { text: parent.text; textFormat: Text.PlainText; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter }
                            onClicked: app.clickAsset(modelData.id)
                        }
                    }
                        }
                    }
                    Button { Layout.fillWidth: true; text: "Nemotron investigator"; onClicked: app.showAgent = !app.showAgent }
                    Button { objectName: "openPipeEvidence"; Layout.fillWidth: true; text: "Pipe transport"; onClicked: pipeTransportDialog.open() }
                    Label { text: "PhysicsNeMo: not connected\nLean proofs: standalone"; color: "#8ca5b4"; font.pixelSize: 11; lineHeight: 1.3 }
                }
            }

            Item {
                id: scenePanel
                Layout.fillWidth: true; Layout.fillHeight: true
                View3D {
                    id: view; anchors.fill: parent
                    environment: SceneEnvironment { backgroundMode: SceneEnvironment.Color; clearColor: "#101d28"; antialiasingMode: SceneEnvironment.MSAA; antialiasingQuality: SceneEnvironment.Medium }
                    Node {
                        id: cameraRig
                        eulerRotation: Qt.vector3d(app.pitch, app.yaw, 0)
                        PerspectiveCamera { id: sceneCamera; z: app.distance; clipNear: 1; clipFar: 5000 }
                    }
                    camera: sceneCamera
                    DirectionalLight { eulerRotation: Qt.vector3d(-45, -35, 0); brightness: 1.3; ambientColor: "#8295a5" }
                    Model {
                        source: "#Rectangle"; eulerRotation.x: -90; scale: Qt.vector3d(16, 16, 1)
                        materials: PrincipledMaterial { baseColor: "#203442"; roughness: 1 }
                    }
                    Repeater3D {
                        model: controller.routes
                        delegate: PipeRoute {
                            required property var modelData
                            routeData: modelData; draggedId: sceneMouse.draggedId; draggedPosition: sceneMouse.dragPosition
                        }
                    }
                    Repeater3D { model: controller.assets; delegate: Equipment { required property var modelData; assetData: modelData } }
                }
                Repeater {
                    id: labelRepeater
                    model: controller.assets
                    delegate: Label {
                        required property var modelData
                        property vector3d point: {
                            if (!app.sceneReady) return Qt.vector3d(0, 0, 0)
                            const watchRotation = cameraRig.eulerRotation
                            const watchPosition = cameraRig.position
                            const watchDistance = sceneCamera.z
                            const watchSize = view.width + view.height
                            return view.mapFrom3DScene(Qt.vector3d(modelData.x, 108, modelData.z))
                        }
                        x: point.x - width / 2; y: point.y; visible: point.z > 0
                        text: modelData.label; textFormat: Text.PlainText; font.pixelSize: 12; color: modelData.selected ? "#ffcf70" : "#d5e5eb"
                    }
                }
                MouseArea {
                    id: sceneMouse
                    anchors.fill: parent; acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
                    property string draggedId: ""
                    property var draggedNode: null
                    property vector3d dragPosition: Qt.vector3d(0, 0, 0)
                    property vector3d startWorld: Qt.vector3d(0, 0, 0)
                    property vector3d original: Qt.vector3d(0, 0, 0)
                    property point previous
                    property bool moved: false
                    property string pickedName: ""
                    onPressed: function(mouse) {
                        previous = Qt.point(mouse.x, mouse.y); moved = false
                        const hit = view.pick(mouse.x, mouse.y)
                        pickedName = hit.objectHit ? hit.objectHit.objectName : ""
                        if (mouse.button === Qt.LeftButton && pickedName.indexOf("asset:") === 0 && !app.connectMode) {
                            draggedId = pickedName.substring(6)
                            let node = hit.objectHit
                            while (node && node.objectName.indexOf("equipment:") !== 0) node = node.parent
                            draggedNode = node
                            if (node) { original = node.position; dragPosition = original; startWorld = app.groundPoint(mouse.x, mouse.y) }
                        }
                    }
                    onPositionChanged: function(mouse) {
                        const dx = mouse.x - previous.x, dy = mouse.y - previous.y
                        if (pressedButtons & Qt.RightButton) { app.yaw -= dx * 0.4; app.pitch = Math.max(-85, Math.min(-15, app.pitch - dy * 0.3)) }
                        if (pressedButtons & Qt.MiddleButton) { cameraRig.x -= dx * app.distance / 900; cameraRig.z -= dy * app.distance / 900 }
                        if ((pressedButtons & Qt.LeftButton) && draggedNode) {
                            const world = app.groundPoint(mouse.x, mouse.y)
                            dragPosition = Qt.vector3d(Math.round((original.x + world.x - startWorld.x) / 20) * 20, 0, Math.round((original.z + world.z - startWorld.z) / 20) * 20)
                            draggedNode.position = dragPosition; moved = moved || Math.abs(world.x - startWorld.x) + Math.abs(world.z - startWorld.z) > 3
                        }
                        previous = Qt.point(mouse.x, mouse.y)
                    }
                    onReleased: function(mouse) {
                        const id = draggedId, position = dragPosition
                        draggedNode = null; draggedId = ""
                        if (mouse.button === Qt.LeftButton) {
                            if (moved && id.length > 0) controller.moveAsset(id, position.x, position.z)
                            else if (pickedName.indexOf("asset:") === 0) app.clickAsset(pickedName.substring(6))
                            else if (pickedName.indexOf("route:") === 0) controller.select(pickedName.substring(6))
                            else controller.select("")
                        }
                        controller.setCamera(app.yaw, app.pitch, app.distance, cameraRig.x, cameraRig.z)
                    }
                    onCanceled: { if (draggedNode) draggedNode.position = original; draggedNode = null; draggedId = "" }
                    onWheel: function(wheel) { app.distance = Math.max(250, Math.min(1600, app.distance - wheel.angleDelta.y * 0.6)); controller.setCamera(app.yaw, app.pitch, app.distance, cameraRig.x, cameraRig.z) }
                }
                DropArea {
                    anchors.fill: parent; keys: ["equipment"]
                    onDropped: function(drop) {
                        if (drop.source.kind === "pump") {
                            const hit = view.pick(drop.x, drop.y)
                            if (hit.objectHit && hit.objectHit.objectName.indexOf("route:") === 0) controller.attachPump(hit.objectHit.objectName.substring(6))
                        } else {
                            const point = app.groundPoint(drop.x, drop.y)
                            controller.addAsset(drop.source.kind, point.x, point.z)
                        }
                        drop.acceptProposedAction()
                    }
                }
                Label {
                    anchors.left: parent.left; anchors.top: parent.top; anchors.margins: 14
                    text: app.connectMode ? (app.connectionSource.length > 0 ? "Choose destination equipment" : "Choose source equipment") : "Drag to move · Right-drag to orbit · Middle-drag to pan · Scroll to zoom"
                    color: app.connectMode ? "#ffcf70" : "#a1b6c5"; font.pixelSize: 12
                }
                Label { anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: 14; text: "SYNTHETIC · Layout does not determine hydraulic flow"; color: "#88a0b0"; font.pixelSize: 11 }
            }

            Rectangle {
                Layout.preferredWidth: 272; Layout.fillHeight: true; color: "#142330"
                ScrollView {
                    anchors.fill: parent; anchors.margins: 14; contentWidth: availableWidth
                    ColumnLayout {
                        width: parent.width; spacing: 10
                        Label { text: "PROPERTIES"; font.pixelSize: 11; font.letterSpacing: 2; color: "#9eb5c2" }
                        Label { Layout.fillWidth: true; text: controller.selected.label || "Select equipment or a pipe"; textFormat: Text.PlainText; font.pixelSize: 17; wrapMode: Text.WrapAnywhere }
                        Label { Layout.fillWidth: true; text: controller.selected.kind || ""; textFormat: Text.PlainText; color: "#60c6c9" }
                        Button { objectName: "inspectPipeTransport"; Layout.fillWidth: true; visible: controller.selected.kind === "route"; enabled: app.pipeTransport.routeId.length > 0; text: "Inspect pipe transport"; onClicked: pipeTransportDialog.open() }
                        TextField { Layout.fillWidth: true; visible: !!controller.selected.kind && controller.selected.kind !== "route"; text: controller.selected.label || ""; onEditingFinished: controller.editSelected("label", text) }
                        Repeater {
                            model: {
                                const k = controller.selected.kind
                                if (k === "tank") return [{key:"capacity",name:"Capacity (m³)"},{key:"initial",name:"Initial inventory (m³)"}]
                                if (k === "source") return [{key:"inflow",name:"Production (m³/h)"},{key:"curtailment_max",name:"Max reduction (m³/h)"},{key:"curtailment",name:"Reduction (m³/h)"},{key:"cost",name:"Reduction cost / m³"}]
                                if (k === "route") return controller.selected.sourceDriven ? [{key:"capacity",name:"Route capacity (m³/h)"}] : [{key:"capacity",name:"Route capacity (m³/h)"},{key:"flow",name:"Setpoint (m³/h)"},{key:"cost",name:"Route cost / m³"}]
                                if (k === "treatment" || k === "trucking") return [{key:"capacity",name:"Throughput (m³/h)"},{key:"cost",name:"Cost / m³"}]
                                return []
                            }
                            delegate: ColumnLayout {
                                required property var modelData
                                Layout.fillWidth: true; spacing: 3
                                Label { text: modelData.name; color: "#a7bbc7"; font.pixelSize: 12 }
                                TextField { Layout.fillWidth: true; text: String(controller.selected[modelData.key] ?? ""); inputMethodHints: Qt.ImhFormattedNumbersOnly; onEditingFinished: controller.editSelected(modelData.key, text) }
                            }
                        }
                        Label { visible: controller.selected.kind === "route" && !!controller.selected.sourceDriven; Layout.fillWidth: true; text: "Source-route flow follows production minus reduction."; wrapMode: Text.WordWrap; color: "#93aeba" }
                        Button { Layout.fillWidth: true; visible: controller.selected.kind === "route" && !controller.selected.sourceDriven && !controller.selected.pump; text: "Attach pump"; onClicked: controller.attachPump(controller.selectedId) }
                        CheckBox { visible: !!controller.selected.pump; checked: !!controller.selected.available; text: "Pump available"; onClicked: controller.editSelected("available", checked ? "true" : "false") }
                        Button { Layout.fillWidth: true; visible: !!controller.selected.pump; enabled: !!controller.selected.available; text: "Fail selected pump"; onClicked: controller.failSelectedPump() }
                        Button { Layout.fillWidth: true; visible: !!controller.selected.kind; text: "Delete selected"; onClicked: controller.deleteSelected() }
                        Rectangle { Layout.fillWidth: true; height: 1; color: "#304452" }
                        Label { text: controller.draftIssues.length ? "DRAFT NEEDS ATTENTION" : "NETWORK READY"; color: controller.draftIssues.length ? "#ffcf70" : "#60d2b1"; font.pixelSize: 11 }
                        Repeater { model: controller.draftIssues; delegate: Label { required property string modelData; Layout.fillWidth: true; text: modelData; textFormat: Text.PlainText; wrapMode: Text.WordWrap; font.pixelSize: 11; color: "#e1b879" } }
                    }
                }
            }
        }
        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: app.showAgent ? Math.min(420, Math.round(app.height * 0.52)) : 196; color: "#172735"
            AgentPanel { id: agentPanel; anchors.fill: parent; anchors.margins: 14; visible: app.showAgent; investigator: app.investigator; history: app.history; scientificWorker: app.scientificWorker; storageScience: app.storageScience }
            ColumnLayout {
                visible: !app.showAgent
                anchors.fill: parent; anchors.margins: 14; spacing: 5
                RowLayout {
                    Layout.fillWidth: true
                    Label { text: "FORECAST / VALIDATION"; color: "#60d2d1"; font.pixelSize: 11; font.letterSpacing: 2 }
                    Item { Layout.fillWidth: true }
                    Label { text: controller.runId.length > 0 ? "Run " + controller.runId.substring(0, 8) : "No executed forecast"; color: "#91aab9"; font.pixelSize: 11 }
                    Button { text: "Approve simulated preview"; enabled: controller.canApprove; onClicked: controller.approve() }
                }
                Label { Layout.fillWidth: true; Layout.fillHeight: true; text: controller.summary; textFormat: Text.PlainText; wrapMode: Text.WordWrap; font.pixelSize: 13; color: "#d2e1e9"; elide: Text.ElideRight }
                RowLayout {
                    Layout.fillWidth: true
                    Repeater {
                        model: controller.assets
                        delegate: Label { required property var modelData; visible: modelData.kind === "tank"; text: modelData.label + ": " + modelData.volume.toFixed(2) + " / " + modelData.capacity.toFixed(0) + " m³"; textFormat: Text.PlainText; color: modelData.violated ? "#f58c8d" : "#a8d9df"; Layout.rightMargin: 16; font.pixelSize: 12 }
                    }
                    Item { Layout.fillWidth: true }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Label { text: "t = " + controller.forecastTimeH.toFixed(3) + " h"; Layout.preferredWidth: 95 }
                    Slider { Layout.fillWidth: true; from: 0; to: 1; enabled: controller.runId.length > 0; value: controller.forecastFraction; onMoved: controller.forecastFraction = value }
                    Label { text: "3.000 h" }
                }
            }
        }
    }
    Rectangle {
        id: ghost; parent: app.contentItem; z: 1000; visible: false; width: 120; height: 34; radius: 5; color: "#357a88"
        property string kind: ""; property string label: ""
        Drag.source: ghost; Drag.keys: ["equipment"]; Drag.hotSpot.x: 60; Drag.hotSpot.y: 16
        Label { anchors.centerIn: parent; text: ghost.label; textFormat: Text.PlainText }
    }
}

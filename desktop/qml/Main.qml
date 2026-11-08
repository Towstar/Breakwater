import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtQuick3D

ApplicationWindow {
    id: app
    required property var controller
    required property var investigator
    required property var credentials
    required property var history
    required property var scientificWorker
    required property var pipeTransport
    required property var storageScience
    required property var telemetry
    required property var eog
    required property var eogAnalysis
    required property string initialWorkspace
    property string workspace: initialWorkspace
    readonly property bool eogSceneReady: workspace === "eog" && eogPanel.datasetSceneReady
    readonly property int eogVisibleLabels: workspace === "eog" ? eogPanel.visibleSceneLabels : 0
    readonly property bool eogTrendReady: workspace === "eog" && eogPanel.datasetTrendReady
    readonly property int eogTrendSamples: workspace === "eog" ? eogPanel.renderedTrendSamples : 0
    readonly property int eogTrendGuides: workspace === "eog" ? eogPanel.renderedTrendGuides : 0
    readonly property bool eogAnalysisVisible: eogAnalysisDialog.visible
    readonly property bool sensorEditorVisible: sensorDialog.visible
    readonly property bool telemetryVisible: telemetryDialog.visible
    readonly property int telemetryTraceSamples: telemetryDialog.renderedNumericSamples
    readonly property bool storageScienceVisible: showAgent && agentPanel.scienceVisible
    readonly property bool pipeTransportVisible: pipeTransportDialog.visible
    readonly property bool compactUi: Qt.application.arguments.indexOf("--compact-ui") >= 0
    property bool showAgent: false
    width: compactUi ? 1100 : 1440
    height: compactUi ? 700 : 900
    minimumWidth: 1100
    minimumHeight: 700
    visible: true
    title: workspace === "eog" ? "BREAKWATER | EOG operator console" : "BREAKWATER | Synthetic fluid network workbench"
    color: Theme.background
    font.family: Theme.fontFamily
    font.pixelSize: Theme.bodySize
    palette.window: Theme.background
    palette.base: Theme.raised
    palette.alternateBase: Theme.surface
    palette.button: Theme.raised
    palette.buttonText: Theme.text
    palette.text: Theme.text
    palette.windowText: Theme.text
    palette.placeholderText: Theme.muted
    palette.highlight: Theme.text
    palette.highlightedText: Theme.background
    palette.mid: Theme.controlBorder
    palette.dark: Theme.selected
    palette.light: Theme.raised
    palette.brightText: Theme.text
    property bool connectMode: false
    property string connectionSource: ""
    property string inspectedAssetId: ""
    property var rejectedNumericFields: ({})
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
    function closeWorkbenchPanels() {
        showAgent = false
        pipeTransportDialog.close()
        sensorDialog.close()
        telemetryDialog.close()
    }
    function openPipeTransport() { closeWorkbenchPanels(); pipeTransportDialog.open() }
    function openStorageScience() { closeWorkbenchPanels(); showAgent = true; agentPanel.showScience() }
    function openSensors() { closeWorkbenchPanels(); sensorDialog.open() }
    function openTelemetry() { closeWorkbenchPanels(); telemetryDialog.open() }
    function toggleWorkbenchPanel(name) {
        const wasVisible = name === "investigator" ? showAgent
            : name === "sensors" ? sensorDialog.visible
            : name === "telemetry" ? telemetryDialog.visible : pipeTransportDialog.visible
        closeWorkbenchPanels()
        if (wasVisible) return
        if (name === "investigator") showAgent = true
        else if (name === "sensors") sensorDialog.open()
        else if (name === "telemetry") telemetryDialog.open()
        else if (name === "pipe") pipeTransportDialog.open()
    }
    function openEogAnalysis() { eogAnalysisDialog.open() }
    function commitNumericField(field, value) {
        const assetId = controller.selectedId
        const previousFeedbackId = controller.actionFeedback.id
        controller.editSelected(field, value)
        const outcome = controller.actionFeedback
        const next = Object.assign({}, rejectedNumericFields)
        // Retain only the actual rejection from this deliberate commit. This
        // presents the controller's result without a second numeric validator.
        if (outcome.id && outcome.id !== previousFeedbackId && outcome.model_changed === false
                && (outcome.asset_ids || []).indexOf(assetId) >= 0
                && (outcome.field === field || outcome.code === "edit.rejected"))
            next[field] = { input: value, message: String(outcome.message || "The entry was not applied.") }
        else delete next[field]
        if (controller.selectedId === assetId) rejectedNumericFields = next
    }
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
        function onActionFeedbackChanged() { actionNotice.showFeedback(app.controller.actionFeedback) }
        function onChanged() {
            if (app.inspectedAssetId !== app.controller.selectedId) {
                app.inspectedAssetId = app.controller.selectedId
                app.rejectedNumericFields = ({})
            }
        }
    }
    Shortcut { sequence: StandardKey.Undo; enabled: app.workspace === "workbench"; onActivated: controller.undo() }
    Shortcut { sequence: StandardKey.Redo; enabled: app.workspace === "workbench"; onActivated: controller.redo() }
    Shortcut {
        sequence: "Escape"
        onActivated: {
            if (actionNotice.visible && actionNotice.ownsFocus(app.activeFocusItem)) actionNotice.dismiss()
            else {
                app.connectMode = false; app.connectionSource = ""
                if (actionNotice.visible) actionNotice.dismiss()
            }
        }
    }

    FileDialog { id: openDialog; nameFilters: ["BREAKWATER project (*.json)"]; onAccepted: controller.load(selectedFile) }
    FileDialog { id: saveDialog; fileMode: FileDialog.SaveFile; defaultSuffix: "json"; nameFilters: ["BREAKWATER project (*.json)"]; onAccepted: controller.save(selectedFile) }
    PipeTransportPanel { id: pipeTransportDialog; pipeTransport: app.pipeTransport }
    SensorPanel { id: sensorDialog; controller: app.controller }
    TelemetryPanel { id: telemetryDialog; telemetry: app.telemetry }
    EogAnalysisPanel { id: eogAnalysisDialog; analysis: app.eogAnalysis; eog: app.eog }

    header: ToolBar {
        height: 64
        background: Rectangle {
            color: Theme.surface
            Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 1; color: Theme.divider }
        }
        RowLayout {
            visible: app.workspace === "workbench"
            anchors.fill: parent; anchors.leftMargin: 18; anchors.rightMargin: 18; spacing: 10
            Image { source: "qrc:/breakwater/assets/breakwater-logo.svg"; fillMode: Image.PreserveAspectFit; Layout.preferredWidth: 156; Layout.preferredHeight: 44; Accessible.name: "Breakwater"; Accessible.role: Accessible.StaticText }
            Rectangle { Layout.preferredWidth: 1; Layout.preferredHeight: 28; color: Theme.divider; Layout.leftMargin: 4; Layout.rightMargin: 4 }
            Label { text: "Simulation workbench"; visible: app.width > 1220; color: Theme.secondary; font.pixelSize: Theme.smallSize }
            ActionButton { iconName: "network"; text: "EOG console"; onClicked: { pipeTransportDialog.close(); sensorDialog.close(); telemetryDialog.close(); app.workspace = "eog" } }
            ActionButton {
                id: demoChoice; iconName: "layout-template"; text: "Demo scenarios"; tooltip: "Load a complete example configuration"
                onClicked: demoMenu.open()
                Menu {
                    id: demoMenu; y: demoChoice.height + 5
                    MenuItem { text: "Pump failure"; onTriggered: controller.loadExample("baseline") }
                    MenuItem { text: "Bad reroute"; onTriggered: controller.loadExample("bad") }
                    MenuItem { text: "Mixed plan"; onTriggered: controller.loadExample("mixed") }
                }
            }
            ActionButton { iconName: "rotate-ccw"; text: "Reset"; tooltip: "Reset the synthetic network to its initial example"; onClicked: controller.reset() }
            Item { Layout.fillWidth: true; Layout.minimumWidth: 0 }
            RowLayout {
                spacing: 4
                ActionButton { text: "Undo"; iconName: "undo-2"; display: AbstractButton.IconOnly; tooltip: "Undo"; enabled: controller.canUndo; onClicked: controller.undo() }
                ActionButton { text: "Redo"; iconName: "redo-2"; display: AbstractButton.IconOnly; tooltip: "Redo"; enabled: controller.canRedo; onClicked: controller.redo() }
                ActionButton { text: "Open"; iconName: "folder-open"; display: AbstractButton.IconOnly; tooltip: "Open project"; onClicked: openDialog.open() }
                ActionButton { text: "Save"; iconName: "save"; display: AbstractButton.IconOnly; tooltip: "Save project"; onClicked: saveDialog.open() }
            }
            ActionButton { iconName: "play"; text: controller.busy ? "Calculating..." : "Run forecast"; enabled: !controller.busy && controller.draftIssues.length === 0; highlighted: true; tooltip: controller.draftIssues.length ? "Resolve the current Network issues before forecasting" : "Run the native forecast for this synthetic network"; onClicked: controller.forecast() }
        }
        RowLayout {
            visible: app.workspace === "eog"
            anchors.fill: parent; anchors.leftMargin: 18; anchors.rightMargin: 18; spacing: 12
            Image { source: "qrc:/breakwater/assets/breakwater-logo.svg"; fillMode: Image.PreserveAspectFit; Layout.preferredWidth: 156; Layout.preferredHeight: 44; Accessible.name: "Breakwater"; Accessible.role: Accessible.StaticText }
            Rectangle { Layout.preferredWidth: 1; Layout.preferredHeight: 28; color: Theme.divider; Layout.leftMargin: 4; Layout.rightMargin: 4 }
            Label { text: "EOG operator console"; color: Theme.secondary; font.pixelSize: Theme.smallSize }
            Item { Layout.fillWidth: true }
            Label { text: "Recorded / advisory"; visible: app.width > 1220; color: Theme.muted; Layout.maximumWidth: 200; elide: Text.ElideRight; font.pixelSize: Theme.smallSize }
            ActionButton { objectName: "openEogAnalysis"; iconName: "workflow"; highlighted: true; text: "Investigate / history"; onClicked: eogAnalysisDialog.open() }
            ActionButton { objectName: "switchToWorkbench"; iconName: "boxes"; text: "Simulation workbench"; onClicked: app.workspace = "workbench" }
        }
    }

    EogDatasetPanel { id: eogPanel; eog: app.eog; anchors.fill: parent; visible: app.workspace === "eog" }
    RowLayout {
        visible: app.workspace === "workbench"
        anchors.fill: parent; spacing: 1
            Rectangle {
                Layout.preferredWidth: 208; Layout.minimumWidth: 208; Layout.maximumWidth: 208
                Layout.fillHeight: true; color: Theme.surface
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 12; spacing: 6
                    RowLayout {
                        Layout.fillWidth: true; spacing: 6
                        Label { text: "EQUIPMENT"; font.pixelSize: Theme.captionSize; font.letterSpacing: 1.2; font.bold: true; color: Theme.secondary }
                        Label { Layout.fillWidth: true; text: "click / drag"; horizontalAlignment: Text.AlignRight; font.pixelSize: Theme.captionSize; color: Theme.muted }
                    }
                    GridLayout {
                        objectName: "equipmentGrid"
                        Layout.fillWidth: true; columns: 3; columnSpacing: 6; rowSpacing: 6
                        Repeater {
                        model: [{kind:"tank", label:"Storage tank"}, {kind:"source",label:"Production source"}, {kind:"treatment",label:"Treatment"}, {kind:"trucking",label:"Tanker outlet"}, {kind:"pump",label:"Pump on route"}]
                        delegate: Rectangle {
                            required property var modelData
                            objectName: "equipmentTool_" + modelData.kind
                            Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.preferredWidth: 54; Layout.preferredHeight: 44
                            implicitWidth: 54; implicitHeight: 44; radius: Theme.cornerRadius; color: paletteMouse.containsMouse ? Theme.hover : Theme.raised
                            border.color: activeFocus ? Theme.text : paletteMouse.containsMouse ? Theme.controlBorder : Theme.divider
                            border.width: 1
                            activeFocusOnTab: true
                            Accessible.role: Accessible.Button
                            Accessible.name: modelData.label
                            Accessible.description: modelData.kind === "pump" ? "Attach to the selected route, or drag onto a route." : "Click or press Space or Return to add, or drag into the scene."
                            ToolTip.visible: paletteMouse.containsMouse || activeFocus
                            ToolTip.delay: activeFocus ? 0 : 450
                            ToolTip.text: modelData.label
                            Keys.onSpacePressed: { if (modelData.kind === "pump") controller.attachPump(controller.selectedId); else controller.addAsset(modelData.kind, 0, 0) }
                            Keys.onReturnPressed: { if (modelData.kind === "pump") controller.attachPump(controller.selectedId); else controller.addAsset(modelData.kind, 0, 0) }
                            Image {
                                anchors.centerIn: parent; width: 22; height: 22
                                source: Theme.icon(({tank:"cylinder",source:"waves",treatment:"factory",trucking:"truck",pump:"gauge"})[modelData.kind])
                                sourceSize.width: 22; sourceSize.height: 22; fillMode: Image.PreserveAspectFit; Accessible.ignored: true
                            }
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
                        ActionButton {
                            objectName: "equipmentTool_connect"
                            iconName: app.connectMode ? "x" : "link-2"
                            Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.preferredWidth: 54; Layout.preferredHeight: 44
                            display: AbstractButton.IconOnly; icon.width: 22; icon.height: 22
                            text: app.connectMode ? "Cancel connection" : "Connect equipment"
                            tooltip: text; checked: app.connectMode
                            onClicked: { app.connectMode = !app.connectMode; app.connectionSource = "" }
                        }
                    }
                    Label { text: "NETWORK"; font.pixelSize: Theme.captionSize; font.letterSpacing: 1.4; font.bold: true; color: Theme.secondary }
                    ListView {
                        id: networkAssetList
                        objectName: "networkAssetList"
                        Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumHeight: 48
                        clip: true; spacing: 4; boundsBehavior: Flickable.StopAtBounds
                        flickableDirection: Flickable.VerticalFlick
                        model: controller.assets
                        delegate: ItemDelegate {
                            required property var modelData
                            width: ListView.view.width - networkScrollBar.width - 4
                            text: modelData.label; highlighted: modelData.selected
                            background: Rectangle { color: modelData.selected ? Theme.selected : parent.hovered ? Theme.hover : "transparent"; Rectangle { anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom; width: 3; color: Theme.text; visible: modelData.selected } }
                            contentItem: Label { text: parent.text; textFormat: Text.PlainText; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter; color: Theme.text; font.pixelSize: Theme.smallSize }
                            onClicked: app.clickAsset(modelData.id)
                        }
                        ScrollBar.vertical: ScrollBar {
                            id: networkScrollBar
                            objectName: "networkScrollBar"
                            policy: ScrollBar.AlwaysOn; active: true; interactive: true
                            width: 12; padding: 2; minimumSize: 0.12
                            background: Rectangle { color: Theme.background; border.color: Theme.divider; radius: 4 }
                            contentItem: Rectangle {
                                implicitWidth: 8; implicitHeight: 32; radius: 3
                                color: networkScrollBar.pressed ? Theme.text : networkScrollBar.hovered ? Theme.secondary : Theme.controlBorder
                            }
                        }
                    }
                    ActionButton { objectName: "toggleInvestigator"; Layout.fillWidth: true; iconName: "workflow"; text: "Nemotron investigator"; checked: app.showAgent; onClicked: app.toggleWorkbenchPanel("investigator") }
                    ActionButton { objectName: "openSensorEditor"; Layout.fillWidth: true; iconName: "gauge"; text: "Sensors"; checked: sensorDialog.visible; onClicked: app.toggleWorkbenchPanel("sensors") }
                    ActionButton { objectName: "openTelemetry"; Layout.fillWidth: true; iconName: "activity"; text: "Local monitoring"; checked: telemetryDialog.visible; onClicked: app.toggleWorkbenchPanel("telemetry") }
                    ActionButton { objectName: "openPipeEvidence"; Layout.fillWidth: true; iconName: "waves"; text: "Pipe transport"; checked: pipeTransportDialog.visible; onClicked: app.toggleWorkbenchPanel("pipe") }
                    Label { text: "PhysicsNeMo: not connected\nLean proofs: standalone"; color: Theme.muted; font.pixelSize: Theme.captionSize; lineHeight: 1.3 }
                }
            }

        ColumnLayout {
            Layout.fillWidth: true; Layout.fillHeight: true; spacing: 0
            RowLayout {
                Layout.fillWidth: true; Layout.fillHeight: true; spacing: 1
            Item {
                id: scenePanel
                Layout.fillWidth: true; Layout.fillHeight: true
                View3D {
                    id: view; anchors.fill: parent
                    environment: SceneEnvironment { backgroundMode: SceneEnvironment.Color; clearColor: "#0D0F11"; antialiasingMode: SceneEnvironment.MSAA; antialiasingQuality: SceneEnvironment.Medium }
                    Node {
                        id: cameraRig
                        eulerRotation: Qt.vector3d(app.pitch, app.yaw, 0)
                        PerspectiveCamera { id: sceneCamera; z: app.distance; clipNear: 1; clipFar: 5000 }
                    }
                    camera: sceneCamera
                    DirectionalLight { eulerRotation: Qt.vector3d(-45, -35, 0); brightness: 1.3; ambientColor: "#8295a5" }
                    Model {
                        source: "#Rectangle"; eulerRotation.x: -90; scale: Qt.vector3d(16, 16, 1)
                        materials: PrincipledMaterial { baseColor: "#20262B"; roughness: 1 }
                    }
                    Repeater3D {
                        model: controller.routes
                        delegate: PipeRoute {
                            required property var modelData
                            routeData: modelData; draggedId: sceneMouse.draggedId; draggedPosition: sceneMouse.dragPosition
                        }
                    }
                    Repeater3D { model: controller.assets; delegate: Equipment { required property var modelData; assetData: modelData } }
                    Repeater3D {
                        model: controller.sensorMarkers
                        delegate: Model {
                            required property var modelData
                            objectName: "sensor:" + modelData.id
                            source: "#Sphere"; pickable: true
                            position: Qt.vector3d(modelData.x, modelData.y, modelData.z)
                            scale: Qt.vector3d(0.14, 0.14, 0.14)
                            materials: PrincipledMaterial { baseColor: modelData.selected ? "#F3F3F3" : "#929CA6"; roughness: 0.55 }
                        }
                    }
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
                            else if (pickedName.indexOf("sensor:") === 0) { controller.selectSensor(pickedName.substring(7)); app.openSensors() }
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
                            const name = hit.objectHit ? hit.objectHit.objectName : ""
                            controller.attachPump(name.indexOf("route:") === 0 ? name.substring(6) : name.indexOf("asset:") === 0 ? name.substring(6) : "")
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
                    color: app.connectMode ? Theme.warning : Theme.secondary; font.pixelSize: Theme.smallSize
                }
                Label { anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: 14; text: "SYNTHETIC · Layout does not determine hydraulic flow"; color: Theme.muted; font.pixelSize: Theme.captionSize }
                ActionFeedbackCard {
                    id: actionNotice
                    objectName: "actionFeedbackCard"
                    anchors.left: parent.left; anchors.bottom: parent.bottom; anchors.leftMargin: 14; anchors.bottomMargin: 38
                    width: Math.min(360, parent.width - 28)
                    z: 20
                }
            }

            Rectangle {
                Layout.preferredWidth: 286; Layout.fillHeight: true; color: Theme.surface
                ScrollView {
                    anchors.fill: parent; anchors.margins: 14; contentWidth: availableWidth
                    ColumnLayout {
                        width: parent.width; spacing: 10
                        Label { text: "ASSET PROPERTIES"; font.pixelSize: Theme.captionSize; font.letterSpacing: 1.4; font.bold: true; color: Theme.secondary }
                        Label { Layout.fillWidth: true; text: controller.selected.label || "Select equipment or a pipe"; textFormat: Text.PlainText; font.pixelSize: 19; font.weight: Font.DemiBold; wrapMode: Text.WrapAnywhere }
                        Label { Layout.fillWidth: true; text: controller.selected.kind || ""; textFormat: Text.PlainText; color: Theme.secondary }
                        ActionButton { objectName: "inspectPipeTransport"; iconName: "waves"; Layout.fillWidth: true; visible: controller.selected.kind === "route"; enabled: app.pipeTransport.routeId.length > 0; text: "Inspect pipe transport"; onClicked: pipeTransportDialog.open() }
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
                                id: numericRow
                                required property var modelData
                                readonly property var rejection: app.rejectedNumericFields[modelData.key] || ({})
                                Layout.fillWidth: true; spacing: 3
                                Label { text: modelData.name; color: numericRow.rejection.message ? Theme.error : Theme.secondary; font.pixelSize: Theme.smallSize }
                                TextField {
                                    objectName: "numericProperty_" + numericRow.modelData.key
                                    property bool pendingUserCommit: false
                                    Layout.fillWidth: true
                                    text: String(controller.selected[numericRow.modelData.key] ?? "")
                                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                                    Accessible.name: numericRow.modelData.name
                                    Accessible.description: numericRow.rejection.message || ""
                                    onTextEdited: pendingUserCommit = true
                                    // Preserve native Return handling, but distinguish an
                                    // explicit retry from focus leaving after that commit.
                                    Keys.onReturnPressed: function(event) { pendingUserCommit = true; event.accepted = false }
                                    Keys.onEnterPressed: function(event) { pendingUserCommit = true; event.accepted = false }
                                    onEditingFinished: {
                                        if (!pendingUserCommit) return
                                        pendingUserCommit = false
                                        app.commitNumericField(numericRow.modelData.key, text)
                                    }
                                }
                                Label {
                                    objectName: "numericRejection_" + numericRow.modelData.key
                                    Layout.fillWidth: true; visible: !!numericRow.rejection.message
                                    text: "Not applied: “" + String(numericRow.rejection.input || "") + "”. "
                                        + numericRow.rejection.message + "\nAccepted value: "
                                        + String(controller.selected[numericRow.modelData.key] ?? "unavailable")
                                    textFormat: Text.PlainText; wrapMode: Text.WrapAnywhere
                                    color: Theme.error; font.pixelSize: Theme.bodySize
                                }
                            }
                        }
                        Label { visible: controller.selected.kind === "route" && !!controller.selected.sourceDriven; Layout.fillWidth: true; text: "Source-route flow follows production minus reduction."; wrapMode: Text.WordWrap; color: Theme.secondary }
                        ActionButton { iconName: "gauge"; Layout.fillWidth: true; visible: controller.selected.kind === "route" && !controller.selected.sourceDriven && !controller.selected.pump; text: "Attach pump"; onClicked: controller.attachPump(controller.selectedId) }
                        CheckBox { visible: !!controller.selected.pump; checked: !!controller.selected.available; text: "Pump available"; onClicked: controller.editSelected("available", checked ? "true" : "false") }
                        ActionButton { iconName: "triangle-alert"; Layout.fillWidth: true; visible: !!controller.selected.pump; enabled: !!controller.selected.available; text: "Fail selected pump"; onClicked: controller.failSelectedPump() }
                        ActionButton { iconName: "trash-2"; Layout.fillWidth: true; visible: !!controller.selected.kind; text: "Delete selected"; onClicked: controller.deleteSelected() }
                        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.divider }
                        Label { text: controller.draftIssues.length ? "NETWORK ISSUES · " + controller.draftIssues.length : "NETWORK READY TO FORECAST"; color: controller.draftIssues.length ? Theme.warning : Theme.secondary; font.pixelSize: Theme.captionSize; font.bold: true }
                        Repeater { model: controller.draftIssues; delegate: Label { required property string modelData; Layout.fillWidth: true; text: modelData; textFormat: Text.PlainText; wrapMode: Text.WordWrap; font.pixelSize: Theme.bodySize; color: Theme.warning } }
                    }
                }
            }
        }
        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: app.showAgent ? Math.min(420, Math.round(app.height * 0.52)) : 196; color: Theme.surface
            AgentPanel { id: agentPanel; anchors.fill: parent; anchors.margins: 14; visible: app.showAgent; investigator: app.investigator; history: app.history; scientificWorker: app.scientificWorker; storageScience: app.storageScience; credentials: app.credentials }
            ColumnLayout {
                visible: !app.showAgent
                anchors.fill: parent; anchors.margins: 14; spacing: 5
                RowLayout {
                    Layout.fillWidth: true
                    Label { text: "FORECAST / VALIDATION"; color: Theme.secondary; font.pixelSize: Theme.captionSize; font.letterSpacing: 1.4; font.bold: true }
                    Item { Layout.fillWidth: true }
                    Label { text: controller.runId.length > 0 ? "Run " + controller.runId.substring(0, 8) : "No executed forecast"; color: Theme.muted; font.pixelSize: Theme.captionSize }
                    ActionButton { iconName: "clipboard-check"; text: "Approve simulated preview"; enabled: controller.canApprove; onClicked: controller.approve() }
                }
                Label { Layout.fillWidth: true; Layout.fillHeight: true; text: controller.summary; textFormat: Text.PlainText; wrapMode: Text.WordWrap; font.pixelSize: Theme.bodySize; color: Theme.text; elide: Text.ElideRight }
                RowLayout {
                    Layout.fillWidth: true
                    Repeater {
                        model: controller.assets
                        delegate: Label { required property var modelData; visible: modelData.kind === "tank"; text: modelData.label + ": " + modelData.volume.toFixed(2) + " / " + modelData.capacity.toFixed(0) + " m³"; textFormat: Text.PlainText; color: modelData.violated ? Theme.error : Theme.text; Layout.rightMargin: 16; font.pixelSize: Theme.smallSize; font.family: Theme.monoFamily }
                    }
                    Item { Layout.fillWidth: true }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Label { text: "t = " + controller.forecastTimeH.toFixed(3) + " h"; Layout.preferredWidth: 95 }
                    Slider { Layout.fillWidth: true; from: 0; to: 1; enabled: controller.runId.length > 0; value: controller.forecastFraction; onMoved: controller.forecastFraction = value }
                    Label { text: controller.runId.length > 0 ? controller.forecastHorizonH.toFixed(3) + " h" : "No executed horizon"; color: Theme.secondary; font.pixelSize: Theme.smallSize }
                }
            }
        }
        }
    }
    Rectangle {
        id: ghost; parent: app.contentItem; z: 1000; visible: false; width: 120; height: 34; radius: Theme.cornerRadius; color: Theme.selected; border.color: Theme.controlBorder
        property string kind: ""; property string label: ""
        Drag.source: ghost; Drag.keys: ["equipment"]; Drag.hotSpot.x: 60; Drag.hotSpot.y: 16
        Label { anchors.centerIn: parent; text: ghost.label; textFormat: Text.PlainText }
    }
}

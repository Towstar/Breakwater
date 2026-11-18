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
    required property var workspaceSettings
    required property string buildInfo
    required property var portfolioExamples
    required property size availableClientSize
    required property string initialWorkspace
    property string workspace: initialWorkspace
    readonly property bool eogSceneReady: workspace === "eog" && eogPanel.datasetSceneReady
    readonly property int eogVisibleLabels: workspace === "eog" ? eogPanel.visibleSceneLabels : 0
    readonly property bool eogTrendReady: workspace === "eog" && eogPanel.datasetTrendReady
    readonly property int eogTrendSamples: workspace === "eog" ? eogPanel.renderedTrendSamples : 0
    readonly property int eogTrendGuides: workspace === "eog" ? eogPanel.renderedTrendGuides : 0
    readonly property bool eogAnalysisVisible: workspace === "eog" && operationsDockOpen
    readonly property bool sensorEditorVisible: sensorDialog.visible
    readonly property bool telemetryVisible: telemetryDialog.visible
    readonly property int telemetryTraceSamples: telemetryDialog.renderedNumericSamples
    readonly property bool storageScienceVisible: showAgent && agentPanel.scienceVisible
    readonly property bool pipeTransportVisible: pipeTransportDialog.visible
    readonly property bool compactUi: Qt.application.arguments.indexOf("--compact-ui") >= 0
    property bool showAgent: false
    width: Math.max(minimumWidth, Math.min(compactUi ? 1100 : 1440, availableClientSize.width))
    height: Math.max(minimumHeight, Math.min(compactUi ? 700 : 900, availableClientSize.height))
    minimumWidth: 960
    minimumHeight: 540
    readonly property bool compactLayout: width < 1280
    property string compactPane: "navigator"
    property int resultTab: 0
    property bool operationsDockOpen: false
    property int operationsTab: 2
    property url pendingRecordedArchive: ""
    readonly property bool navigatorVisible: workspaceSettings.values.navigatorOpen && (!compactLayout || compactPane === "navigator")
    readonly property bool inspectorVisible: workspaceSettings.values.inspectorOpen && (!compactLayout || compactPane === "inspector")
    property bool topView: false
    property bool fittedView: false
    property real fittedDistance: 720
    property real fitSpan: 800
    function openResults(index) {
        resultTab = index; showAgent = index > 0
        workspaceSettings.setValue("dockOpen", true)
        if (index > 0) agentPanel.showTab([0, 1, 0, 2, 3][index])
    }
    function frameScene(selectedOnly) {
        const rows = controller.assets.filter(row => !selectedOnly || row.selected)
        if (!rows.length) return
        let minX = rows[0].x, maxX = rows[0].x, minZ = rows[0].z, maxZ = rows[0].z
        for (const row of rows) { minX = Math.min(minX, row.x); maxX = Math.max(maxX, row.x); minZ = Math.min(minZ, row.z); maxZ = Math.max(maxZ, row.z) }
        fitSpan = Math.max(260, maxX - minX + 180, maxZ - minZ + 180)
        fittedDistance = Math.max(360, fitSpan * 1.65 * Math.max(1, view.height / Math.max(1, view.width)))
        cameraRig.x = (minX + maxX) / 2; cameraRig.z = (minZ + maxZ) / 2
        fittedView = true
    }
    function restoreOrbit() {
        topView = false; fittedView = false
        yaw = controller.camera.yaw; pitch = controller.camera.pitch; distance = controller.camera.distance
        cameraRig.x = controller.camera.x; cameraRig.z = controller.camera.z
    }
    function benchmarkTick() { yaw = (yaw + 0.1) % 360 }
    visible: true
    title: "BREAKWATER " + buildInfo + (workspace === "eog" ? " | Operations" : " | Sandbox")
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
    Binding { target: Theme; property: "compactControls"; value: workspaceSettings.values.compactControls }
    readonly property int sceneAssetCount: controller.assets.length
    readonly property int sceneSelectedCount: controller.selectedIds.length
    readonly property bool detailedScene: workspaceSettings.values.quality > 0 && (sceneAssetCount < 48 || (fittedView ? fittedDistance : distance) < 1500)
    readonly property int sceneQuality: detailedScene ? workspaceSettings.values.quality : 0
    readonly property var siteBounds: {
        let minX = -300, maxX = 300, minZ = -300, maxZ = 300
        for (const row of controller.assets) { minX = Math.min(minX, row.x); maxX = Math.max(maxX, row.x); minZ = Math.min(minZ, row.z); maxZ = Math.max(maxZ, row.z) }
        return {x: (minX + maxX) / 2, z: (minZ + maxZ) / 2, width: maxX - minX + 400, depth: maxZ - minZ + 400}
    }
    property string hoveredAssetId: ""
    property string listSelectionAnchor: ""
    readonly property var hoveredConnection: {
        const revision = controller.editorRevision
        return connectMode && connectionSource.length && hoveredAssetId.length ? controller.connectionEligibility(connectionSource, hoveredAssetId) : ({})
    }
    function assetPosition(id) {
        const rows = controller.assets
        for (const row of rows) if (row.id === id) return Qt.vector3d(row.x, 13, row.z)
        return Qt.vector3d(0, 13, 0)
    }
    function sceneKeysEnabled() {
        const focus = app.activeFocusItem
        const textEditing = focus && typeof focus.selectAll === "function" && focus.text !== undefined
        const blocked = [sensorDialog, telemetryDialog, pipeTransportDialog, aboutDialog, projectMenu, viewMenu, editMenu, exampleMenu].some(item => item && item.visible)
        return app.workspace === "workbench" && !textEditing && !blocked && !(agentPanel && agentPanel.dialogOpen) && !(forecastPanel && forecastPanel.dialogOpen)
    }
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
        showAgent = false; resultTab = 0
        pipeTransportDialog.close()
        sensorDialog.close()
        telemetryDialog.close()
    }
    function openPipeTransport() { closeWorkbenchPanels(); pipeTransportDialog.open() }
    function openStorageScience() { closeWorkbenchPanels(); openResults(4) }
    function openSensors() { closeWorkbenchPanels(); sensorDialog.open() }
    function openTelemetry() { closeWorkbenchPanels(); telemetryDialog.open() }
    function toggleWorkbenchPanel(name) {
        const wasVisible = name === "investigator" ? showAgent
            : name === "sensors" ? sensorDialog.visible
            : name === "telemetry" ? telemetryDialog.visible : pipeTransportDialog.visible
        closeWorkbenchPanels()
        if (wasVisible) return
        if (name === "investigator") openResults(2)
        else if (name === "sensors") sensorDialog.open()
        else if (name === "telemetry") telemetryDialog.open()
        else if (name === "pipe") pipeTransportDialog.open()
    }
    function openEogAnalysis() {
        operationsDockOpen = true; operationsTab = 2; operationsAnalysis.showTab(1)
    }
    function openRecordedInvestigation() {
        if (!portfolioExamples.recordedNemotron || !portfolioExamples.operations) return
        workspace = "eog"; pendingRecordedArchive = portfolioExamples.recordedNemotron
        eog.loadFolder(portfolioExamples.operations)
    }
    Connections {
        target: app.eog
        function onCompleted() {
            if (!String(app.pendingRecordedArchive).length || app.eog.busy) return
            // Loading must finish retiring the previous capture before import
            // records its generation. Copy the URL before clearing the intent.
            const archive = String(app.pendingRecordedArchive); app.pendingRecordedArchive = ""
            if (app.eog.loaded && app.eog.sourceFolder.toLowerCase() === String(app.portfolioExamples.operationsPath || "").toLowerCase()) {
                app.eogAnalysis.importHistory(archive); app.openEogAnalysis(); app.operationsTab = 3; operationsAnalysis.showTab(2)
            }
        }
        function onFailed() { app.pendingRecordedArchive = "" }
        function onCancelled() { app.pendingRecordedArchive = "" }
    }
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
    function clickAsset(id, modifiers, fromList) {
        if (connectMode) {
            if (connectionSource.length === 0) connectionSource = id
            else { controller.connectAssets(connectionSource, id); connectionSource = ""; connectMode = false }
        } else {
            const keys = modifiers || Qt.NoModifier
            if (fromList && (keys & Qt.ShiftModifier) && listSelectionAnchor.length) {
                const rows = controller.assets, a = rows.findIndex(row => row.id === listSelectionAnchor), b = rows.findIndex(row => row.id === id)
                if (a >= 0 && b >= 0) controller.selectAssets(controller.selectedIds.concat(rows.slice(Math.min(a, b), Math.max(a, b) + 1).map(row => row.id)), id)
                else controller.toggleAssetSelection(id, true)
            } else if (keys & Qt.ControlModifier) controller.toggleAssetSelection(id)
            else if (keys & Qt.ShiftModifier) controller.toggleAssetSelection(id, true)
            else controller.select(id)
            if (!(keys & Qt.ShiftModifier)) listSelectionAnchor = id
        }
    }
    Component.onCompleted: { yaw = controller.camera.yaw; pitch = controller.camera.pitch; distance = controller.camera.distance; cameraRig.x = controller.camera.x; cameraRig.z = controller.camera.z }
    Connections {
        target: app.controller
        function onCameraChanged() { app.topView = false; app.fittedView = false; app.yaw = app.controller.camera.yaw; app.pitch = app.controller.camera.pitch; app.distance = app.controller.camera.distance; cameraRig.x = app.controller.camera.x; cameraRig.z = app.controller.camera.z }
        function onActionFeedbackChanged() { actionNotice.showFeedback(app.controller.actionFeedback) }
        function onChanged() {
            if (app.inspectedAssetId !== app.controller.selectedId) {
                app.inspectedAssetId = app.controller.selectedId
                app.rejectedNumericFields = ({})
            }
        }
    }
    Shortcut { sequences: [StandardKey.Undo]; enabled: app.sceneKeysEnabled(); onActivated: controller.undo() }
    Shortcut { sequences: [StandardKey.Redo]; enabled: app.sceneKeysEnabled(); onActivated: controller.redo() }
    Shortcut { sequence: "Ctrl+D"; enabled: app.sceneKeysEnabled(); onActivated: controller.duplicateSelection() }
    Shortcut { sequence: "Delete"; enabled: app.sceneKeysEnabled(); onActivated: controller.deleteSelected() }
    Shortcut { sequence: "F"; enabled: app.sceneKeysEnabled(); onActivated: app.frameScene(true) }
    Shortcut {
        sequence: "Escape"
        onActivated: {
            if (actionNotice.visible && actionNotice.ownsFocus(app.activeFocusItem)) actionNotice.dismiss()
            else {
                sceneMouse.cancelDrag(); app.connectMode = false; app.connectionSource = ""
                if (actionNotice.visible) actionNotice.dismiss()
            }
        }
    }

    FileDialog { id: openDialog; nameFilters: ["BREAKWATER project (*.json)"]; onAccepted: controller.load(selectedFile) }
    FileDialog { id: saveDialog; fileMode: FileDialog.SaveFile; defaultSuffix: "json"; nameFilters: ["BREAKWATER project (*.json)"]; onAccepted: controller.save(selectedFile) }
    PipeTransportPanel { id: pipeTransportDialog; pipeTransport: app.pipeTransport }
    SensorPanel { id: sensorDialog; controller: app.controller }
    TelemetryPanel { id: telemetryDialog; telemetry: app.telemetry }


    Dialog {
        id: aboutDialog; parent: Overlay.overlay; anchors.centerIn: parent
        title: "About Breakwater"; modal: true; width: Math.min(520, app.width - 32)
        padding: 20; standardButtons: Dialog.Close
        header: DialogHeading { owner: aboutDialog }
        footer: DialogFooter { owner: aboutDialog }
        contentItem: ColumnLayout {
            spacing: 12
            Label { text: "Breakwater " + app.buildInfo; font.pixelSize: 19; font.weight: Font.DemiBold }
            Label { Layout.fillWidth: true; text: "An editable fluid-network sandbox and recorded-data investigation workspace. Local numerical tools retain the evidence behind every result."; wrapMode: Text.Wrap; color: Theme.secondary }
            Label { Layout.fillWidth: true; text: "Synthetic and advisory. Scene arrangement does not determine hydraulics. Optional Science and hosted Nemotron have separate runtime requirements."; wrapMode: Text.Wrap; color: Theme.muted }
        }
    }
    header: ToolBar {
        height: 64
        background: Rectangle { color: Theme.surface; Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.divider } }
        RowLayout {
            anchors.fill: parent; anchors.leftMargin: 16; anchors.rightMargin: 16; spacing: 8
            Image { source: "qrc:/breakwater/assets/breakwater-logo.svg"; fillMode: Image.PreserveAspectFit; Layout.preferredWidth: app.width < 1080 ? 130 : 156; Layout.preferredHeight: 42; Accessible.name: "Breakwater"; Accessible.role: Accessible.StaticText }
            Rectangle { Layout.preferredWidth: 1; Layout.preferredHeight: 26; color: Theme.divider; Layout.leftMargin: 8; Layout.rightMargin: 8 }
            ActionButton { objectName: "switchToWorkbench"; iconName: "boxes"; text: "Sandbox"; checked: app.workspace === "workbench"; onClicked: app.workspace = "workbench" }
            ActionButton { objectName: "switchToOperations"; iconName: "network"; text: "Operations"; checked: app.workspace === "eog"; onClicked: { app.closeWorkbenchPanels(); app.workspace = "eog" } }
            Item { Layout.fillWidth: true; Layout.minimumWidth: 0 }
            ActionButton {
                id: projectMenuButton; iconName: "folder-open"; text: "Project"; visible: app.workspace === "workbench"; onClicked: projectMenu.open()
                Menu {
                    id: projectMenu; y: projectMenuButton.height + 6
                    MenuItem { text: "Open project…"; onTriggered: openDialog.open() }
                    MenuItem { text: "Save project…"; onTriggered: saveDialog.open() }
                    MenuSeparator {}
                    MenuItem { text: "Healthy example / reset"; onTriggered: controller.reset() }
                    MenuItem { text: "Pump failure example"; onTriggered: controller.loadExample("baseline") }
                    MenuItem { text: "Bad reroute example"; onTriggered: controller.loadExample("bad") }
                    MenuItem { text: "Mixed response example"; onTriggered: controller.loadExample("mixed") }
                }
            }
            ActionButton { text: "Undo"; iconName: "undo-2"; visible: app.workspace === "workbench"; display: AbstractButton.IconOnly; tooltip: "Undo"; enabled: controller.canUndo; onClicked: controller.undo() }
            ActionButton { text: "Redo"; iconName: "redo-2"; visible: app.workspace === "workbench"; display: AbstractButton.IconOnly; tooltip: "Redo"; enabled: controller.canRedo; onClicked: controller.redo() }
            // Keep accessible Open/Save actions for keyboard and verification consumers.
            ActionButton { text: "Open"; iconName: "folder-open"; visible: app.workspace === "workbench" && app.width >= 1080; display: AbstractButton.IconOnly; tooltip: "Open project"; onClicked: openDialog.open() }
            ActionButton { text: "Save"; iconName: "save"; visible: app.workspace === "workbench" && app.width >= 1080; display: AbstractButton.IconOnly; tooltip: "Save project"; onClicked: saveDialog.open() }
            ActionButton { iconName: "play"; text: controller.busy ? "Calculating…" : "Run forecast"; visible: app.workspace === "workbench"; highlighted: true; enabled: !controller.busy && controller.draftIssues.length === 0; onClicked: { app.openResults(0); controller.forecast() } }
            ActionButton { objectName: "openEogAnalysis"; iconName: "workflow"; highlighted: true; text: "Investigate / history"; visible: app.workspace === "eog"; checked: app.operationsDockOpen; onClicked: { if (app.operationsDockOpen) app.operationsDockOpen = false; else app.openEogAnalysis() } }
            ActionButton {
                id: exampleButton; text: "Examples"; iconName: "play"; visible: app.workspace === "eog"; onClicked: exampleMenu.open()
                Menu {
                    id: exampleMenu; y: exampleButton.height + 6
                    MenuItem { text: "Synthetic operations demo"; enabled: !!app.portfolioExamples.operations; onTriggered: eog.loadFolder(app.portfolioExamples.operations) }
                    MenuItem { text: "Renamed / changed topology"; enabled: !!app.portfolioExamples["renamed-topology"]; onTriggered: eog.loadFolder(app.portfolioExamples["renamed-topology"]) }
                    MenuItem { text: "Recorded Nemotron investigation"; enabled: !!app.portfolioExamples.recordedNemotron; onTriggered: app.openRecordedInvestigation() }
                }
            }
            ActionButton {
                id: viewMenuButton; iconName: "layout-template"; text: "View"; display: AbstractButton.IconOnly; tooltip: "Workspace layout and rendering"; onClicked: viewMenu.open()
                Menu {
                    id: viewMenu; y: viewMenuButton.height + 6
                    MenuItem { text: "Network navigator"; checkable: true; checked: app.navigatorVisible; onTriggered: { workspaceSettings.setValue("navigatorOpen", true); app.compactPane = "navigator"; if (!app.compactLayout) workspaceSettings.setValue("navigatorOpen", !checked ? false : true) } }
                    MenuItem { text: "Equipment inspector"; checkable: true; checked: app.inspectorVisible; onTriggered: { workspaceSettings.setValue("inspectorOpen", true); app.compactPane = "inspector"; if (!app.compactLayout) workspaceSettings.setValue("inspectorOpen", checked) } }
                    MenuItem { text: "All equipment labels"; checkable: true; checked: workspaceSettings.values.allLabels; onTriggered: workspaceSettings.setValue("allLabels", checked) }
                    MenuSeparator {}
                    MenuItem { text: "Low rendering quality"; checkable: true; checked: workspaceSettings.values.quality === 0; onTriggered: workspaceSettings.setValue("quality", 0) }
                    MenuItem { text: "Medium rendering quality"; checkable: true; checked: workspaceSettings.values.quality === 1; onTriggered: workspaceSettings.setValue("quality", 1) }
                    MenuItem { text: "High rendering quality"; checkable: true; checked: workspaceSettings.values.quality === 2; onTriggered: workspaceSettings.setValue("quality", 2) }
                    MenuItem { text: "Compact controls"; checkable: true; checked: workspaceSettings.values.compactControls; onTriggered: workspaceSettings.setValue("compactControls", checked) }
                    MenuSeparator {}
                    MenuItem { text: "Reset workspace layout"; onTriggered: { workspaceSettings.reset(); app.compactPane = "navigator" } }
                }
            }
            ActionButton { iconName: "clipboard-check"; text: "About"; tooltip: "Version and capability information"; display: AbstractButton.IconOnly; onClicked: aboutDialog.open() }
        }
    }
    footer: ToolBar {
        height: 28
        background: Rectangle { color: Theme.surface; Rectangle { width: parent.width; height: 1; color: Theme.divider } }
        RowLayout {
            anchors.fill: parent; anchors.leftMargin: 16; anchors.rightMargin: 16
            Label { Layout.fillWidth: true; text: workspaceSettings.status.length ? workspaceSettings.status : app.workspace === "eog" ? "Recorded data · Advisory review · Original units" : "Synthetic network · Native numerical engine · Layout is schematic"; elide: Text.ElideRight; color: workspaceSettings.status.length ? Theme.warning : Theme.muted; font.pixelSize: Theme.captionSize }
            Label { text: app.buildInfo; color: Theme.muted; font.pixelSize: Theme.captionSize }
        }
    }
    ColumnLayout {
        anchors.fill: parent; spacing: 1; visible: app.workspace === "eog"
        EogDatasetPanel { id: eogPanel; eog: app.eog; condensed: app.operationsDockOpen && app.compactLayout; renderQuality: workspaceSettings.values.quality; Layout.fillWidth: true; Layout.fillHeight: true }
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: app.operationsDockOpen ? Math.min(workspaceSettings.values.dockHeight + 110, app.contentItem.height * 0.55) : 0
            visible: app.operationsDockOpen; color: Theme.surface
            ColumnLayout {
                anchors.fill: parent; spacing: 0
                RowLayout {
                    Layout.fillWidth: true; Layout.preferredHeight: 40; Layout.leftMargin: 12; Layout.rightMargin: 8; spacing: 4
                    Repeater { model: ["Observations", "Events", "Plans", "Trace", "History"]
                        ActionButton { required property int index; required property string modelData; text: modelData; checked: app.operationsTab === index; flat: !checked; onClicked: { if (index === 0) app.operationsDockOpen = false; else { app.operationsTab = index; operationsAnalysis.showTab(index - 1) } } }
                    }
                    Label { Layout.fillWidth: true; Layout.minimumWidth: 0; horizontalAlignment: Text.AlignRight; elide: Text.ElideRight; text: app.eog.loaded ? app.eog.datasetIdentity.substring(0, 8) + " · " + eogPanel.utc(app.eog.cursorUnix) : "No dataset"; color: Theme.muted; font.pixelSize: Theme.captionSize }
                    ActionButton { iconName: "x"; text: "Close results"; tooltip: text; display: AbstractButton.IconOnly; onClicked: app.operationsDockOpen = false }
                }
                EogAnalysisView { id: operationsAnalysis; analysis: app.eogAnalysis; eog: app.eog; showTabs: false; Layout.fillWidth: true; Layout.fillHeight: true }
            }
            MouseArea { anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; height: 5; cursorShape: Qt.SizeVerCursor
                property real originY; property int originHeight
                onPressed: function(mouse) { originY = mapToItem(app.contentItem, 0, mouse.y).y; originHeight = workspaceSettings.values.dockHeight }
                onPositionChanged: function(mouse) { if (pressed) workspaceSettings.setValue("dockHeight", Math.round(Math.max(160, Math.min(640, originHeight + originY - mapToItem(app.contentItem, 0, mouse.y).y)))) }
            }
        }
    }
    RowLayout {
        visible: app.workspace === "workbench"
        anchors.fill: parent; spacing: 1
            Rectangle {
                visible: app.navigatorVisible
                Layout.preferredWidth: 220; Layout.minimumWidth: 220; Layout.maximumWidth: 220
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
                            objectName: "networkAsset:" + modelData.id
                            width: ListView.view.width - networkScrollBar.width - 4
                            text: modelData.label; highlighted: modelData.selected
                            background: Rectangle { color: modelData.selected ? Theme.selected : parent.hovered ? Theme.hover : "transparent"; Rectangle { anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom; width: 3; color: Theme.text; visible: modelData.selected } }
                            contentItem: Label { text: parent.text; textFormat: Text.PlainText; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter; color: Theme.text; font.pixelSize: Theme.smallSize }
                            onClicked: app.clickAsset(modelData.id, Qt.NoModifier, true)
                            MouseArea {
                                anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                onPressed: parent.forceActiveFocus()
                                onClicked: function(mouse) { app.clickAsset(modelData.id, mouse.modifiers, true) }
                            }
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
                    GridLayout {
                        Layout.fillWidth: true; columns: app.height < 620 ? 2 : 1; columnSpacing: 6; rowSpacing: 4
                        ActionButton { objectName: "toggleInvestigator"; Layout.fillWidth: true; iconName: "workflow"; text: "Nemotron investigator"; display: app.height < 620 ? AbstractButton.IconOnly : AbstractButton.TextBesideIcon; checked: app.showAgent; onClicked: app.toggleWorkbenchPanel("investigator") }
                        ActionButton { objectName: "openSensorEditor"; Layout.fillWidth: true; iconName: "gauge"; text: "Sensors"; display: app.height < 620 ? AbstractButton.IconOnly : AbstractButton.TextBesideIcon; checked: sensorDialog.visible; onClicked: app.toggleWorkbenchPanel("sensors") }
                        ActionButton { objectName: "openTelemetry"; Layout.fillWidth: true; iconName: "activity"; text: "Local monitoring"; display: app.height < 620 ? AbstractButton.IconOnly : AbstractButton.TextBesideIcon; checked: telemetryDialog.visible; onClicked: app.toggleWorkbenchPanel("telemetry") }
                        ActionButton { objectName: "openPipeEvidence"; Layout.fillWidth: true; iconName: "waves"; text: "Pipe transport"; display: app.height < 620 ? AbstractButton.IconOnly : AbstractButton.TextBesideIcon; checked: pipeTransportDialog.visible; onClicked: app.toggleWorkbenchPanel("pipe") }
                    }
                    Label { text: "PhysicsNeMo: optional tracer fit\nLean proofs: standalone"; color: Theme.muted; font.pixelSize: Theme.captionSize; lineHeight: 1.3 }
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
                    environment: IndustrialEnvironment { quality: app.sceneQuality }
                    Node {
                        id: cameraRig
                        eulerRotation: Qt.vector3d(app.pitch, app.yaw, 0)
                        PerspectiveCamera { id: sceneCamera; z: app.fittedView ? app.fittedDistance : app.distance; clipNear: 1; clipFar: Math.max(5000, z + app.fitSpan * 3) }
                    }
                    OrthographicCamera { id: planCamera; x: cameraRig.x; y: 30000; z: cameraRig.z; eulerRotation.x: -90; clipNear: 1; clipFar: 60000; horizontalMagnification: Math.max(0.01, view.width / app.fitSpan); verticalMagnification: Math.max(0.01, view.width / app.fitSpan) }
                    camera: app.topView ? planCamera : sceneCamera
                    IndustrialLighting { quality: app.sceneQuality }
                    SiteGround { centerX: app.siteBounds.x; centerZ: app.siteBounds.z; spanX: app.siteBounds.width; spanZ: app.siteBounds.depth }
                    ConnectionPreview { visible: app.connectMode && app.connectionSource.length > 0 && app.hoveredAssetId.length > 0; start: app.assetPosition(app.connectionSource); end: app.assetPosition(app.hoveredAssetId); allowed: app.hoveredConnection.allowed === true }
                    Repeater3D {
                        model: controller.routes
                        delegate: PipeRoute {
                            required property var modelData
                            routeData: modelData; draggedIds: sceneMouse.draggedIds; dragDelta: sceneMouse.dragDelta; detailed: app.detailedScene
                        }
                    }
                    Repeater3D { model: controller.assets; delegate: Equipment { required property var modelData; assetData: modelData; connectionState: app.connectMode && app.connectionSource.length ? (app.connectionSource === modelData.id ? 3 : controller.connectionEligibility(app.connectionSource, modelData.id).allowed ? 1 : 0) : 0; draggedIds: sceneMouse.draggedIds; dragDelta: sceneMouse.dragDelta; detailed: app.detailedScene || (controller.selectedIds.length <= 8 && modelData.selected) } }
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
                        readonly property bool eligible: workspaceSettings.values.allLabels || modelData.selected
                            || app.hoveredAssetId === modelData.id
                            || (app.sceneSelectedCount === 0 && modelData.kind === "tank" && app.sceneAssetCount < 12)
                        property vector3d point: {
                            if (!app.sceneReady || !eligible) return Qt.vector3d(0, 0, 0)
                            const watchRotation = cameraRig.eulerRotation
                            const watchPosition = cameraRig.position
                            const watchDistance = sceneCamera.z + Number(app.topView) * planCamera.horizontalMagnification
                            const watchSize = view.width + view.height
                            const delta = sceneMouse.draggedIds.indexOf(modelData.id) >= 0 ? sceneMouse.dragDelta : Qt.vector3d(0, 0, 0)
                            return view.mapFrom3DScene(Qt.vector3d(modelData.x + delta.x, 108, modelData.z + delta.z))
                        }
                        x: point.x - width / 2; y: point.y
                        visible: eligible && point.z > 0 && point.x > 0 && point.x < view.width && point.y > 80 && point.y < view.height - 20
                        padding: 4
                        background: Rectangle { color: "#253139"; opacity: 0.9; radius: 3; border.color: modelData.selected ? "#DCECF2" : "#51616A" }
                        text: modelData.label; textFormat: Text.PlainText; font.pixelSize: 12; color: modelData.selected ? "#FFFFFF" : "#DCE5E9"
                    }
                }
                MouseArea {
                    id: sceneMouse
                    anchors.fill: parent; acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
                    hoverEnabled: true
                    property var draggedIds: []
                    property vector3d dragDelta: Qt.vector3d(0, 0, 0)
                    property vector3d startWorld: Qt.vector3d(0, 0, 0)
                    property vector3d original: Qt.vector3d(0, 0, 0)
                    property point previous: Qt.point(0, 0)
                    property point pressPoint: Qt.point(0, 0)
                    property bool moved: false
                    property bool marquee: false
                    property string pickedName: ""
                    property double editRevision: 0
                    function cancelDrag() { draggedIds = []; dragDelta = Qt.vector3d(0, 0, 0); marquee = false; moved = false }
                    onPressed: function(mouse) {
                        scenePanel.forceActiveFocus()
                        cancelDrag(); previous = Qt.point(mouse.x, mouse.y); pressPoint = previous
                        const hit = view.pick(mouse.x, mouse.y)
                        pickedName = hit.objectHit ? hit.objectHit.objectName : ""
                        if (mouse.button === Qt.LeftButton && !app.connectMode && pickedName.indexOf("asset:") === 0 && !(mouse.modifiers & (Qt.ControlModifier | Qt.ShiftModifier))) {
                            const id = pickedName.substring(6)
                            if (controller.selectedIds.indexOf(id) < 0) controller.select(id)
                            draggedIds = controller.selectedIds.slice(); original = app.assetPosition(id); startWorld = app.groundPoint(mouse.x, mouse.y); editRevision = controller.editorRevision
                        } else if (mouse.button === Qt.LeftButton && !app.connectMode && pickedName.length === 0) marquee = true
                    }
                    onPositionChanged: function(mouse) {
                        const dx = mouse.x - previous.x, dy = mouse.y - previous.y
                        if (pressedButtons & Qt.RightButton) { if (app.topView || app.fittedView) app.restoreOrbit(); app.yaw -= dx * 0.4; app.pitch = Math.max(-85, Math.min(-15, app.pitch - dy * 0.3)) }
                        if (pressedButtons & Qt.MiddleButton) { cameraRig.x -= dx * app.distance / 900; cameraRig.z -= dy * app.distance / 900 }
                        if ((pressedButtons & Qt.LeftButton) && draggedIds.length) {
                            const world = app.groundPoint(mouse.x, mouse.y)
                            dragDelta = Qt.vector3d(Math.round((world.x - startWorld.x) / 20) * 20, 0, Math.round((world.z - startWorld.z) / 20) * 20)
                            moved = moved || Math.abs(mouse.x - pressPoint.x) + Math.abs(mouse.y - pressPoint.y) > 6
                        }
                        previous = Qt.point(mouse.x, mouse.y)
                        if (!pressed) {
                            const hit = view.pick(mouse.x, mouse.y), name = hit.objectHit ? hit.objectHit.objectName : ""
                            app.hoveredAssetId = name.indexOf("asset:") === 0 ? name.substring(6) : ""
                        }
                    }
                    onReleased: function(mouse) {
                        if (mouse.button === Qt.LeftButton) {
                            if (moved && draggedIds.length) controller.moveSelection(dragDelta.x, dragDelta.z, editRevision)
                            else if (marquee && Math.abs(mouse.x - pressPoint.x) + Math.abs(mouse.y - pressPoint.y) > 6) {
                                const left = Math.min(mouse.x, pressPoint.x), right = Math.max(mouse.x, pressPoint.x), top = Math.min(mouse.y, pressPoint.y), bottom = Math.max(mouse.y, pressPoint.y)
                                const ids = (mouse.modifiers & (Qt.ControlModifier | Qt.ShiftModifier)) ? controller.selectedIds.slice() : []
                                for (const row of controller.assets) {
                                    const point = view.mapFrom3DScene(Qt.vector3d(row.x, 32, row.z))
                                    if (point.z > 0 && point.x >= left && point.x <= right && point.y >= top && point.y <= bottom) ids.push(row.id)
                                }
                                controller.selectAssets(ids)
                            } else if (pickedName.indexOf("sensor:") === 0) { controller.selectSensor(pickedName.substring(7)); app.openSensors() }
                            else if (pickedName.indexOf("asset:") === 0) app.clickAsset(pickedName.substring(6), mouse.modifiers, false)
                            else if (pickedName.indexOf("route:") === 0) controller.select(pickedName.substring(6))
                            else if (!(mouse.modifiers & (Qt.ControlModifier | Qt.ShiftModifier))) controller.select("")
                        }
                        cancelDrag()
                        if (!app.topView && !app.fittedView) controller.setCamera(app.yaw, app.pitch, app.distance, cameraRig.x, cameraRig.z)
                    }
                    onCanceled: cancelDrag()
                    onExited: app.hoveredAssetId = ""
                    onWheel: function(wheel) {
                        if (app.topView) app.fitSpan = Math.max(180, Math.min(40000, app.fitSpan * (wheel.angleDelta.y > 0 ? 0.9 : 1.1)))
                        else if (app.fittedView) app.fittedDistance = Math.max(250, Math.min(60000, app.fittedDistance - wheel.angleDelta.y * Math.max(0.6, app.fittedDistance / 2000)))
                        else { app.distance = Math.max(250, Math.min(1600, app.distance - wheel.angleDelta.y * 0.6)); controller.setCamera(app.yaw, app.pitch, app.distance, cameraRig.x, cameraRig.z) }
                    }
                }
                Rectangle {
                    visible: sceneMouse.marquee && sceneMouse.pressed
                    x: Math.min(sceneMouse.pressPoint.x, sceneMouse.previous.x); y: Math.min(sceneMouse.pressPoint.y, sceneMouse.previous.y)
                    width: Math.abs(sceneMouse.previous.x - sceneMouse.pressPoint.x); height: Math.abs(sceneMouse.previous.y - sceneMouse.pressPoint.y)
                    color: "#2449768A"; border.color: "#EAF5F8"; border.width: 1
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
                    anchors.left: parent.left; anchors.top: parent.top; anchors.margins: 14; anchors.topMargin: 58; width: parent.width - 28; wrapMode: Text.WordWrap
                    text: app.connectMode ? (app.connectionSource.length > 0 ? "Choose destination equipment" : "Choose source equipment") : "Drag to move · Right-drag to orbit · Middle-drag to pan · Scroll to zoom"
                    color: "#253B47"; font.pixelSize: Theme.smallSize
                    background: Rectangle { color: "#DCE5E7"; opacity: 0.88; radius: 3 }
                }
                RowLayout {
                    anchors.top: parent.top; anchors.left: parent.left; anchors.topMargin: 12; anchors.leftMargin: 12; spacing: 5
                    ActionButton { text: "Home"; tooltip: "Restore saved perspective"; onClicked: app.restoreOrbit() }
                    ActionButton { text: "Fit"; tooltip: "Frame the network"; onClicked: app.frameScene(false) }
                    ActionButton { text: "Focus"; enabled: controller.selectedId.length > 0; tooltip: "Frame selected equipment"; onClicked: app.frameScene(true) }
                    ActionButton { text: app.topView ? "Isometric" : "Top"; checked: app.topView; onClicked: { if (app.topView) app.restoreOrbit(); else { app.frameScene(false); app.topView = true } } }
                    ActionButton {
                        id: editMenuButton; text: controller.selectedIds.length > 1 ? "Edit " + controller.selectedIds.length + " assets" : "Edit"; onClicked: editMenu.open()
                        Menu {
                            id: editMenu; y: editMenuButton.height + 6
                            MenuItem { text: "Duplicate selection (Ctrl+D)"; enabled: controller.selectedIds.length > 0; onTriggered: controller.duplicateSelection() }
                            MenuItem { text: "Delete selection"; enabled: controller.selectedId.length > 0; onTriggered: controller.deleteSelected() }
                            MenuSeparator {}
                            MenuItem { text: "Align X to primary asset"; enabled: controller.selectedIds.length > 1; onTriggered: controller.alignSelection("x") }
                            MenuItem { text: "Align Z to primary asset"; enabled: controller.selectedIds.length > 1; onTriggered: controller.alignSelection("z") }
                            MenuItem { text: "Distribute along X"; enabled: controller.selectedIds.length > 2; onTriggered: controller.alignSelection("x", true) }
                            MenuItem { text: "Distribute along Z"; enabled: controller.selectedIds.length > 2; onTriggered: controller.alignSelection("z", true) }
                        }
                    }
                    ActionButton { text: "Navigator"; visible: app.compactLayout; checked: app.navigatorVisible; onClicked: { workspaceSettings.setValue("navigatorOpen", true); app.compactPane = app.compactPane === "navigator" ? "" : "navigator" } }
                    ActionButton { text: "Inspector"; visible: app.compactLayout; checked: app.inspectorVisible; onClicked: { workspaceSettings.setValue("inspectorOpen", true); app.compactPane = app.compactPane === "inspector" ? "" : "inspector" } }
                }
                Label { anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: 14; text: app.hoveredConnection.message || "SCHEMATIC SITE · Layout does not determine hydraulic flow"; color: "#253B47"; font.pixelSize: Theme.captionSize
                    background: Rectangle { color: "#DCE5E7"; opacity: 0.9; radius: 3 }
                }
                ActionFeedbackCard {
                    id: actionNotice
                    objectName: "actionFeedbackCard"
                    anchors.left: parent.left; anchors.bottom: parent.bottom; anchors.leftMargin: 14; anchors.bottomMargin: 38
                    width: Math.min(360, parent.width - 28)
                    z: 20
                }
            }

            Rectangle {
                visible: app.inspectorVisible
                Layout.preferredWidth: 286; Layout.minimumWidth: 286; Layout.fillHeight: true; color: Theme.surface
                ScrollView {
                    id: inspectorScroll
                    anchors.fill: parent; anchors.margins: 14; contentWidth: availableWidth; clip: true
                    ColumnLayout {
                        width: inspectorScroll.availableWidth; spacing: 10
                        Label { text: controller.selectedIds.length > 1 ? "PRIMARY ASSET · " + controller.selectedIds.length + " SELECTED" : "ASSET PROPERTIES"; font.pixelSize: Theme.captionSize; font.letterSpacing: 1.4; font.bold: true; color: Theme.secondary }
                        Label { Layout.fillWidth: true; text: controller.selected.label || "Select equipment or a pipe"; textFormat: Text.PlainText; font.pixelSize: 19; font.weight: Font.DemiBold; wrapMode: Text.WrapAnywhere }
                        Label { Layout.fillWidth: true; visible: controller.selectedIds.length > 1; text: "Fields below edit the primary asset. Use Edit in the scene toolbar for group actions."; wrapMode: Text.Wrap; color: Theme.secondary; font.pixelSize: Theme.smallSize }
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
            id: workbenchDock
            Layout.fillWidth: true; Layout.preferredHeight: workspaceSettings.values.dockOpen ? Math.min(workspaceSettings.values.dockHeight, app.contentItem.height * 0.55) : 42; color: Theme.surface
            RowLayout {
                id: resultTabs; anchors.top: parent.top; anchors.left: parent.left; anchors.right: parent.right; height: 42; anchors.leftMargin: 12; anchors.rightMargin: 8; spacing: 4
                Repeater { model: ["Forecast", "Compare", "Investigate", "History", "Science"]
                    ActionButton { required property int index; required property string modelData; text: modelData; checked: app.resultTab === index && workspaceSettings.values.dockOpen; flat: !checked; onClicked: app.openResults(index) }
                }
                Item { Layout.fillWidth: true }
                ActionButton { text: "Toggle results"; iconName: workspaceSettings.values.dockOpen ? "chevron-down" : "activity"; display: AbstractButton.IconOnly; tooltip: workspaceSettings.values.dockOpen ? "Collapse results" : "Expand results"; onClicked: workspaceSettings.setValue("dockOpen", !workspaceSettings.values.dockOpen) }
            }
            Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: Theme.divider }
            MouseArea {
                anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; height: 5; cursorShape: Qt.SizeVerCursor
                property real originY; property int originHeight
                onPressed: function(mouse) { originY = mapToItem(app.contentItem, 0, mouse.y).y; originHeight = workspaceSettings.values.dockHeight }
                onPositionChanged: function(mouse) { if (pressed) workspaceSettings.setValue("dockHeight", Math.round(Math.max(160, Math.min(640, originHeight + originY - mapToItem(app.contentItem, 0, mouse.y).y)))) }
            }
            AgentPanel { id: agentPanel; anchors.fill: parent; anchors.margins: 14; anchors.topMargin: 48; visible: app.showAgent && workspaceSettings.values.dockOpen; embedded: true; currentNetworkHash: app.controller.networkHash; investigator: app.investigator; history: app.history; scientificWorker: app.scientificWorker; storageScience: app.storageScience; credentials: app.credentials }
            ForecastPanel { id: forecastPanel; controller: app.controller; anchors.fill: parent; anchors.margins: 14; anchors.topMargin: 48; visible: !app.showAgent && workspaceSettings.values.dockOpen }

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

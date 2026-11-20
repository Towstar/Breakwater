import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtQuick3D
import "SceneFraming.js" as SceneFraming

ApplicationWindow {
    id: app
    required property var controller
    required property var investigator
    required property var observedInvestigator
    required property var observedHistory
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
    readonly property bool observedInvestigationVisible: observedInvestigationDialog.visible
    readonly property bool observedAdvisoryReviewVisible: !!observedAgentLoader.item && observedAgentLoader.item.advisoryReviewVisible
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
    property string networkMode: "equipment"
    property int resultTab: 0
    property bool operationsDockOpen: false
    property int operationsTab: 2
    property url pendingRecordedArchive: ""
    property bool sceneOnly: false
    readonly property bool navigatorVisible: !sceneOnly && workspaceSettings.values.navigatorOpen && (!compactLayout || compactPane === "navigator")
    readonly property bool inspectorVisible: !sceneOnly && workspaceSettings.values.inspectorOpen && (!compactLayout || compactPane === "inspector")
    property bool topView: false
    property bool siteGridVisible: true
    property bool fittedView: false
    property real fittedDistance: 720
    property real fitSpan: 800
    property var framedBounds: null
    property string frameScope: "saved"
    property string frameSelectionKey: ""
    property string frameCountText: ""
    property double frameEditorRevision: 0
    property bool frameAdjusted: false
    property real planZoom: 1
    property real planPanX: 0
    property real planPanZ: 0
    readonly property string selectionFrameKey: controller.selected.kind === "route" ? "route:" + controller.selectedId : JSON.stringify(controller.selectedIds.slice().sort())
    readonly property var planFit: framedBounds ? SceneFraming.fitPlan(framedBounds,
        {width:view.width,height:view.height},
        {left:14,right:14,top:(navigationHint.visible ? navigationHint.y+navigationHint.height : sceneToolbar.y+sceneToolbar.height)+14,
            bottom:Math.max(54,routeSummary.visible ? routeSummary.height+28 : 0,sceneOrientation.visible ? sceneOrientation.height+28 : 0)}, planZoom) : null
    readonly property string sceneViewStatus: {
        const mode = topView ? "Plan" : "Perspective"
        if (topView && !planFit) return mode + " · Expand workspace to fit"
        if (!fittedView) return mode + (frameAdjusted ? " · Custom view" : " · Saved view")
        if (frameAdjusted) return mode + " · Custom view"
        return mode + " · " + (frameScope === "network" ? "Network fit" : frameSelectionKey === selectionFrameKey ? "Focused view" : "Previous focus") + " · " + frameCountText
    }
    function toggleSceneFocus() { sceneOnly = !sceneOnly }
    function openResults(index) {
        sceneOnly = false
        resultTab = index; showAgent = index > 0
        workspaceSettings.setValue("dockOpen", true)
        if (index > 0) agentPanel.showTab([0, 1, 0, 2, 3][index])
    }
    function frameScene(selectedOnly) {
        const selection = controller.selected
        const rows = selectedOnly && selection.kind === "route"
            ? sceneAssets.filter(row => row.id === selection.source || row.id === selection.target)
            : sceneAssets.filter(row => !selectedOnly || row.selected)
        if (!rows.length) return
        let minX = rows[0].x, maxX = rows[0].x, minZ = rows[0].z, maxZ = rows[0].z
        for (const row of rows) { minX = Math.min(minX, row.x); maxX = Math.max(maxX, row.x); minZ = Math.min(minZ, row.z); maxZ = Math.max(maxZ, row.z) }
        framedBounds = {minX:minX,maxX:maxX,minZ:minZ,maxZ:maxZ}
        frameScope = selectedOnly ? "selection" : "network"
        frameSelectionKey = selectionFrameKey
        frameCountText = selectedOnly && selection.kind === "route" ? "2 endpoints" : rows.length + (rows.length === 1 ? " asset" : " assets")
        frameEditorRevision = controller.editorRevision; frameAdjusted = false
        planZoom = 1; planPanX = 0; planPanZ = 0
        fitSpan = Math.max(260, maxX - minX + 180, maxZ - minZ + 180)
        fittedDistance = Math.max(360, fitSpan * 1.65 * Math.max(1, view.height / Math.max(1, view.width)))
        cameraRig.x = (minX + maxX) / 2; cameraRig.z = (minZ + maxZ) / 2
        fittedView = true
    }
    function restoreOrbit() {
        topView = false; fittedView = false
        framedBounds = null; frameScope = "saved"; frameAdjusted = false
        planZoom = 1; planPanX = 0; planPanZ = 0
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
    readonly property var sceneAssets: controller.assets
    readonly property var sceneRoutes: controller.routes
    readonly property var networkAssetIndex: new Map(sceneAssets.map(row => [row.id, row]))
    readonly property int sceneAssetCount: sceneAssets.length
    readonly property int sceneSelectedCount: controller.selectedIds.length
    readonly property var filteredNetworkAssets: {
        const query = networkFilter.text.trim().toLowerCase()
        return sceneAssets.filter(row => !query || [row.label, row.id, row.kind, Theme.equipmentName(row.kind)].some(value => String(value).toLowerCase().includes(query)))
    }
    readonly property var networkRouteRows: sceneRoutes.map(row => {
        const source = networkAssetIndex.get(row.source), target = networkAssetIndex.get(row.target)
        const sourceLabel = source ? source.label : row.source, targetLabel = target ? target.label : row.target
        const sourceDriven = !!source && source.kind === "source"
        const rate = typeof row.flow === "number" && Number.isFinite(row.flow)
            ? row.flow.toLocaleString(Qt.locale(), "f", 2) + " m³/h" : "Unavailable"
        const settingText = rate + (sourceDriven ? " · source setting" : " · setpoint")
        const statusText = !row.available ? (row.pump ? "Pump unavailable" : "Route unavailable")
            : row.pump ? "Pump available" : sourceDriven ? "Source-driven" : "Configured route"
        return Object.assign({}, row, {kind:"route", label:sourceLabel + " → " + targetLabel,
            sourceLabel:sourceLabel, targetLabel:targetLabel, sourceDriven:sourceDriven,
            settingText:settingText, statusText:statusText})
    })
    readonly property var filteredNetworkRoutes: {
        const query = networkFilter.text.trim().toLowerCase()
        return networkRouteRows.filter(row => !query || [row.id, row.label, row.source, row.target,
            row.statusText, row.pump ? "pump" : "pipe", "route", "connection"]
            .some(value => String(value).toLowerCase().includes(query)))
    }
    readonly property var networkRows: networkMode === "connections" ? filteredNetworkRoutes : filteredNetworkAssets
    readonly property var selectedNetworkRoute: networkRouteRows.find(row => row.id === controller.selectedId) || null
    readonly property var routeEndpointIds: controller.selected.kind === "route"
        ? [controller.selected.source, controller.selected.target].filter(id => typeof id === "string" && id.length > 0) : []
    onNetworkModeChanged: { if (networkAssetList) networkAssetList.positionViewAtBeginning() }
    onConnectModeChanged: { if (connectMode) networkMode = "equipment" }
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
        const blocked = [sensorDialog, telemetryDialog, observedInvestigationDialog, pipeTransportDialog, aboutDialog, sceneHelpDialog, projectMenu, viewMenu, editMenu, exampleMenu].some(item => item && item.visible)
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
    readonly property int visibleAssetLabels: sceneLabelLayer.visibleCount
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
        observedInvestigationDialog.close()
    }
    function openPipeTransport() { closeWorkbenchPanels(); pipeTransportDialog.open() }
    function openStorageScience() { closeWorkbenchPanels(); openResults(4) }
    function openSensors() { closeWorkbenchPanels(); sensorDialog.open() }
    function openTelemetry() { closeWorkbenchPanels(); telemetryDialog.open() }
    function toggleWorkbenchPanel(name) {
        const wasVisible = name === "investigator" ? showAgent
            : name === "sensors" ? sensorDialog.visible
            : name === "telemetry" ? (telemetryDialog.visible || observedInvestigationDialog.visible) : pipeTransportDialog.visible
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
                const rows = app.filteredNetworkAssets, a = rows.findIndex(row => row.id === listSelectionAnchor), b = rows.findIndex(row => row.id === id)
                if (a >= 0 && b >= 0) controller.selectAssets(controller.selectedIds.concat(rows.slice(Math.min(a, b), Math.max(a, b) + 1).map(row => row.id)), id)
                else controller.toggleAssetSelection(id, true)
            } else if (keys & Qt.ControlModifier) controller.toggleAssetSelection(id)
            else if (keys & Qt.ShiftModifier) controller.toggleAssetSelection(id, true)
            else controller.select(id)
            if (!(keys & Qt.ShiftModifier)) listSelectionAnchor = id
        }
    }
    function selectRoute(id) {
        if (connectMode) return
        listSelectionAnchor = ""
        controller.select(id)
    }
    Component.onCompleted: { yaw = controller.camera.yaw; pitch = controller.camera.pitch; distance = controller.camera.distance; cameraRig.x = controller.camera.x; cameraRig.z = controller.camera.z }
    Connections {
        target: app.controller
        function onCameraChanged() { app.restoreOrbit() }
        function onActionFeedbackChanged() { actionNotice.showFeedback(app.controller.actionFeedback) }
        function onChanged() {
            if (app.fittedView && app.controller.editorRevision !== app.frameEditorRevision) app.frameAdjusted = true
            if (app.inspectedAssetId !== app.controller.selectedId) {
                app.inspectedAssetId = app.controller.selectedId
                app.rejectedNumericFields = ({})
                if (app.controller.selected.kind) app.networkMode = app.controller.selected.kind === "route" ? "connections" : "equipment"
            }
        }
    }
    Shortcut { sequences: [StandardKey.Undo]; enabled: app.sceneKeysEnabled(); onActivated: controller.undo() }
    Shortcut { sequences: [StandardKey.Redo]; enabled: app.sceneKeysEnabled(); onActivated: controller.redo() }
    Shortcut { sequence: "Ctrl+D"; enabled: app.sceneKeysEnabled(); onActivated: controller.duplicateSelection() }
    Shortcut { sequence: "Delete"; enabled: app.sceneKeysEnabled(); onActivated: controller.deleteSelected() }
    Shortcut { sequence: "F"; enabled: app.sceneKeysEnabled(); onActivated: app.frameScene(true) }
    Shortcut { sequence: "Ctrl+0"; enabled: app.sceneKeysEnabled(); onActivated: app.frameScene(false) }
    Shortcut { sequence: "Ctrl+Home"; enabled: app.sceneKeysEnabled(); onActivated: app.restoreOrbit() }
    Shortcut { sequence: "F1"; enabled: app.sceneKeysEnabled(); onActivated: sceneHelpDialog.open() }
    Shortcut { sequence: "Ctrl+Shift+F"; enabled: app.sceneKeysEnabled() && !app.connectMode && !sceneMouse.pressed; onActivated: app.toggleSceneFocus() }
    Shortcut {
        sequence: "Escape"
        onActivated: {
            if (sceneHelpDialog.visible) sceneHelpDialog.close()
            else if (actionNotice.visible && actionNotice.ownsFocus(app.activeFocusItem)) actionNotice.dismiss()
            else if (app.sceneOnly && app.sceneKeysEnabled() && !app.connectMode && !sceneMouse.pressed) app.sceneOnly = false
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
    WorkspaceHelp { id: sceneHelpDialog }
    TelemetryPanel {
        id: telemetryDialog; telemetry: app.telemetry
        onOpenInvestigation: { telemetryDialog.close(); observedInvestigationDialog.open() }
    }
    Dialog {
        id: observedInvestigationDialog; objectName: "observedInvestigationDialog"; parent: Overlay.overlay
        x: Math.max(16,(parent.width-width)/2); y: Math.max(16,(parent.height-height)/2)
        width: Math.min(1120,parent.width-32); height: Math.min(780,parent.height-32)
        title: "Observed forecast and advisory investigation"; modal: false; closePolicy: Popup.CloseOnEscape
        padding: 14
        background: Rectangle { color: Theme.surface; radius: Theme.cornerRadius; border.color: Theme.divider }
        header: DialogHeading { owner: observedInvestigationDialog; closeName: "observedInvestigationClose" }
        footer: DialogFooter { owner: observedInvestigationDialog }
        contentItem: Loader {
            id: observedAgentLoader
            active: observedInvestigationDialog.visible
            sourceComponent: AgentPanel {
                investigator: app.observedInvestigator; history: app.observedHistory; credentials: app.credentials
                scientificWorker: app.scientificWorker; storageScience: app.storageScience
                currentNetworkHash: ""
            }
        }
    }


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
            Label { Layout.fillWidth: true; text: "PhysicsNeMo: optional tracer fit. Lean proofs: standalone algorithm checks."; wrapMode: Text.Wrap; color: Theme.muted; font.pixelSize: Theme.smallSize }
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
                    MenuItem { text: "Local monitoring example"; onTriggered: { controller.loadExample("monitoring"); app.openTelemetry() } }
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
                    MenuItem { objectName: "sceneFocusMode"; text: "Scene focus mode (Ctrl+Shift+F)"; visible: app.workspace === "workbench"; enabled: !app.connectMode; checkable: true; checked: app.sceneOnly; onTriggered: app.toggleSceneFocus() }
                    MenuSeparator { visible: app.workspace === "workbench" }
                    MenuItem { text: "Network navigator"; checkable: true; checked: app.navigatorVisible; onTriggered: { const opening=checked; app.sceneOnly = false; workspaceSettings.setValue("navigatorOpen", app.compactLayout || opening); app.compactPane = "navigator" } }
                    MenuItem { text: "Equipment inspector"; checkable: true; checked: app.inspectorVisible; onTriggered: { const opening=checked; app.sceneOnly = false; workspaceSettings.setValue("inspectorOpen", app.compactLayout || opening); app.compactPane = "inspector" } }
                    MenuItem { text: "More equipment labels"; checkable: true; checked: workspaceSettings.values.allLabels; onTriggered: workspaceSettings.setValue("allLabels", checked) }
                    MenuItem { text: "Layout grid"; visible: app.workspace === "workbench"; checkable: true; checked: app.siteGridVisible; onTriggered: app.siteGridVisible = checked }
                    MenuSeparator {}
                    MenuItem { text: "Low rendering quality"; checkable: true; checked: workspaceSettings.values.quality === 0; onTriggered: workspaceSettings.setValue("quality", 0) }
                    MenuItem { text: "Medium rendering quality"; checkable: true; checked: workspaceSettings.values.quality === 1; onTriggered: workspaceSettings.setValue("quality", 1) }
                    MenuItem { text: "High rendering quality"; checkable: true; checked: workspaceSettings.values.quality === 2; onTriggered: workspaceSettings.setValue("quality", 2) }
                    MenuItem { text: "Compact controls"; checkable: true; checked: workspaceSettings.values.compactControls; onTriggered: workspaceSettings.setValue("compactControls", checked) }
                    MenuSeparator {}
                    MenuItem { text: "Reset workspace layout"; onTriggered: { app.sceneOnly = false; workspaceSettings.reset(); app.compactPane = "navigator" } }
                    MenuItem { text: "Scene controls and shortcuts…"; visible: app.workspace === "workbench"; onTriggered: sceneHelpDialog.open() }
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
            Label { objectName: "workspaceViewStatus"; Layout.fillWidth: true; text: workspaceSettings.status.length ? workspaceSettings.status : app.workspace === "eog" ? "Recorded data · Advisory review · Original units" : (app.sceneOnly ? "Scene focus · Ctrl+Shift+F to return · " : "") + app.sceneViewStatus + " · Synthetic network · Layout is schematic"; textFormat: Text.PlainText; elide: Text.ElideRight; color: workspaceSettings.status.length ? Theme.warning : Theme.muted; font.pixelSize: Theme.captionSize; Accessible.name: text }
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
                                source: Theme.equipmentIcon(modelData.kind)
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
                    RowLayout {
                        Layout.fillWidth: true; spacing: 4
                        Label { Layout.fillWidth: true; text: "NETWORK"; font.pixelSize: Theme.captionSize; font.letterSpacing: 1.4; font.bold: true; color: Theme.secondary }
                        ActionButton {
                            objectName: "networkEquipmentMode"; iconName: "boxes"; text: "Equipment"
                            display: AbstractButton.IconOnly; checked: app.networkMode === "equipment"
                            Layout.preferredWidth: 28; Layout.preferredHeight: 28; implicitWidth: 28; implicitHeight: 28
                            icon.width: 15; icon.height: 15; onClicked: app.networkMode = "equipment"
                        }
                        ActionButton {
                            objectName: "networkConnectionsMode"; iconName: "route"; text: "Connections"
                            display: AbstractButton.IconOnly; checked: app.networkMode === "connections"; enabled: !app.connectMode
                            Layout.preferredWidth: 28; Layout.preferredHeight: 28; implicitWidth: 28; implicitHeight: 28
                            icon.width: 15; icon.height: 15; onClicked: app.networkMode = "connections"
                        }
                        Label { objectName: "networkFilterCount"; text: networkFilter.text.trim().length ? app.networkRows.length + " / " + (app.networkMode === "connections" ? app.sceneRoutes.length : app.sceneAssetCount) : String(app.networkMode === "connections" ? app.sceneRoutes.length : app.sceneAssetCount); color: Theme.muted; font.family: Theme.monoFamily; font.pixelSize: Theme.captionSize }
                    }
                    TextField {
                        id: networkFilter; objectName: "networkFilter"
                        Layout.fillWidth: true; implicitHeight: Theme.controlHeight; rightPadding: 32
                        placeholderText: app.networkMode === "connections" ? "Filter connections" : "Filter equipment"; selectByMouse: true
                        Accessible.name: placeholderText
                        Accessible.description: "Filter by name, ID, type or connection endpoint. Filtering leaves scene selection unchanged."
                        ActionButton {
                            objectName: "clearNetworkFilter"
                            anchors.right: parent.right; anchors.rightMargin: 3; anchors.verticalCenter: parent.verticalCenter
                            width: 26; height: 26; implicitWidth: 26; implicitHeight: 26
                            visible: networkFilter.text.length > 0; iconName: "x"; icon.width: 13; icon.height: 13
                            text: app.networkMode === "connections" ? "Clear connection filter" : "Clear equipment filter"; display: AbstractButton.IconOnly
                            onClicked: { networkFilter.clear(); networkFilter.forceActiveFocus() }
                        }
                    }
                    ListView {
                        id: networkAssetList
                        objectName: "networkAssetList"
                        Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumHeight: 48
                        clip: true; spacing: 4; boundsBehavior: Flickable.StopAtBounds
                        flickableDirection: Flickable.VerticalFlick
                        model: app.networkRows
                        Label {
                            anchors.centerIn: parent; width: parent.width - 16; visible: networkAssetList.count === 0
                            text: app.networkMode === "connections"
                                ? app.sceneRoutes.length ? "No connections match.\nClear or change the filter." : "No connections yet."
                                : app.sceneAssetCount ? "No equipment matches.\nClear or change the filter." : "No equipment yet."
                            horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap; color: Theme.muted; font.pixelSize: Theme.smallSize
                        }
                        delegate: ItemDelegate {
                            id: networkAssetDelegate
                            required property var modelData
                            readonly property bool isRoute: modelData.kind === "route"
                            objectName: (isRoute ? "networkRoute:" : "networkAsset:") + modelData.id
                            width: ListView.view.width - networkScrollBar.width - 4
                            text: modelData.label; highlighted: modelData.selected
                            Accessible.name: isRoute ? modelData.label + ". " + modelData.settingText + ". " + modelData.statusText + ". ID " + modelData.id : text
                            ToolTip {
                                id: networkRowTip
                                visible: networkAssetDelegate.hovered || networkAssetDelegate.activeFocus
                                delay: 500; width: Math.min(360, app.width - 32)
                                text: networkAssetDelegate.isRoute ? networkAssetDelegate.modelData.label + "\n" + networkAssetDelegate.modelData.id + "\n" + networkAssetDelegate.modelData.settingText + "\n" + networkAssetDelegate.modelData.statusText + "\nSimulation settings, not measured delivery." : networkAssetDelegate.modelData.label
                                contentItem: Label { text: networkRowTip.text; textFormat: Text.PlainText; wrapMode: Text.WrapAnywhere; color: Theme.text; font.pixelSize: Theme.smallSize }
                            }
                            background: Rectangle { color: modelData.selected ? Theme.selected : parent.hovered ? Theme.hover : "transparent"; Rectangle { anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom; width: 3; color: Theme.text; visible: modelData.selected } }
                            contentItem: RowLayout {
                                spacing: 10
                                Image {
                                    Layout.preferredWidth: 18; Layout.preferredHeight: 18
                                    source: Theme.equipmentIcon(networkAssetDelegate.modelData.kind)
                                    sourceSize.width: 18; sourceSize.height: 18
                                    fillMode: Image.PreserveAspectFit; Accessible.ignored: true
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true; spacing: 2
                                    Label { Layout.fillWidth: true; text: networkAssetDelegate.text; textFormat: Text.PlainText; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter; color: Theme.text; font.pixelSize: Theme.smallSize }
                                    Label { objectName: "networkRouteSetting:" + networkAssetDelegate.modelData.id; Layout.fillWidth: true; visible: networkAssetDelegate.isRoute; text: networkAssetDelegate.modelData.settingText || ""; textFormat: Text.PlainText; elide: Text.ElideRight; color: Theme.secondary; font.pixelSize: Theme.captionSize }
                                    Label { objectName: "networkRouteState:" + networkAssetDelegate.modelData.id; Layout.fillWidth: true; visible: networkAssetDelegate.isRoute; text: networkAssetDelegate.modelData.statusText || ""; textFormat: Text.PlainText; elide: Text.ElideRight; color: networkAssetDelegate.modelData.available === false ? Theme.error : Theme.muted; font.pixelSize: Theme.captionSize }
                                }
                            }
                            onClicked: { if (isRoute) app.selectRoute(modelData.id); else app.clickAsset(modelData.id, Qt.NoModifier, true) }
                            MouseArea {
                                anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                onPressed: parent.forceActiveFocus()
                                onClicked: function(mouse) { if (networkAssetDelegate.isRoute) app.selectRoute(modelData.id); else app.clickAsset(modelData.id, mouse.modifiers, true) }
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
                        ActionButton { objectName: "openTelemetry"; Layout.fillWidth: true; iconName: "activity"; text: "Local monitoring"; display: app.height < 620 ? AbstractButton.IconOnly : AbstractButton.TextBesideIcon; checked: telemetryDialog.visible || observedInvestigationDialog.visible; onClicked: app.toggleWorkbenchPanel("telemetry") }
                        ActionButton { objectName: "openPipeEvidence"; Layout.fillWidth: true; iconName: "waves"; text: "Pipe transport"; display: app.height < 620 ? AbstractButton.IconOnly : AbstractButton.TextBesideIcon; checked: pipeTransportDialog.visible; onClicked: app.toggleWorkbenchPanel("pipe") }
                    }
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
                    OrthographicCamera { id: planCamera; x: app.planFit ? app.planFit.cameraX + app.planPanX : cameraRig.x; y: 30000; z: app.planFit ? app.planFit.cameraZ + app.planPanZ : cameraRig.z; eulerRotation.x: -90; clipNear: 1; clipFar: 60000; horizontalMagnification: app.planFit ? app.planFit.magnification : Math.max(0.000001, Math.min(view.width,view.height)/app.fitSpan); verticalMagnification: horizontalMagnification }
                    camera: app.topView ? planCamera : sceneCamera
                    IndustrialLighting { quality: app.sceneQuality }
                    SiteGround { id: siteGround; centerX: app.siteBounds.x; centerZ: app.siteBounds.z; spanX: app.siteBounds.width; spanZ: app.siteBounds.depth; showGrid: app.siteGridVisible; quality: app.sceneQuality }
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
                SceneLabels {
                    id: sceneLabelLayer; anchors.fill: parent
                    rows: controller.assets; projector: view; ready: app.sceneReady
                    allLabels: workspaceSettings.values.allLabels; primaryId: controller.selectedId; hoveredId: app.hoveredAssetId
                    relatedIds: app.routeEndpointIds
                    draggedIds: sceneMouse.draggedIds; dragDelta: sceneMouse.dragDelta
                    topInset: (navigationHint.visible ? navigationHint.y + navigationHint.height : sceneToolbar.y + sceneToolbar.height) + 8
                    exclusions: {
                        const regions = []
                        if (sceneOrientation.visible) regions.push({x:sceneOrientation.x,y:sceneOrientation.y,width:sceneOrientation.width,height:sceneOrientation.height})
                        if (routeSummary.visible) regions.push({x:routeSummary.x,y:routeSummary.y,width:routeSummary.width,height:routeSummary.height})
                        return regions
                    }
                    viewState: [cameraRig.eulerRotation,cameraRig.position,sceneCamera.z,app.topView,planCamera.position,planCamera.horizontalMagnification,view.width,view.height]
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
                        if (pressedButtons & Qt.RightButton) { if (app.topView || app.fittedView) app.restoreOrbit(); app.frameAdjusted = true; app.yaw -= dx * 0.4; app.pitch = Math.max(-85, Math.min(-15, app.pitch - dy * 0.3)) }
                        if (pressedButtons & Qt.MiddleButton) {
                            app.frameAdjusted = true
                            if (app.topView) { app.planPanX -= dx / planCamera.horizontalMagnification; app.planPanZ -= dy / planCamera.verticalMagnification }
                            else { cameraRig.x -= dx * app.distance / 900; cameraRig.z -= dy * app.distance / 900 }
                        }
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
                            else if (pickedName.indexOf("route:") === 0) app.selectRoute(pickedName.substring(6))
                            else if (!(mouse.modifiers & (Qt.ControlModifier | Qt.ShiftModifier))) controller.select("")
                        }
                        cancelDrag()
                        if (!app.topView && !app.fittedView) {
                            const adjusted = app.frameAdjusted
                            controller.setCamera(app.yaw, app.pitch, app.distance, cameraRig.x, cameraRig.z)
                            app.frameAdjusted = adjusted
                        }
                    }
                    onCanceled: cancelDrag()
                    onExited: app.hoveredAssetId = ""
                    onWheel: function(wheel) {
                        app.frameAdjusted = true
                        if (app.topView) app.planZoom = Math.max(0.05, Math.min(20, app.planZoom * (wheel.angleDelta.y > 0 ? 1.1 : 1/1.1)))
                        else if (app.fittedView) app.fittedDistance = Math.max(250, Math.min(60000, app.fittedDistance - wheel.angleDelta.y * Math.max(0.6, app.fittedDistance / 2000)))
                        else { app.distance = Math.max(250, Math.min(1600, app.distance - wheel.angleDelta.y * 0.6)); controller.setCamera(app.yaw, app.pitch, app.distance, cameraRig.x, cameraRig.z); app.frameAdjusted = true }
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
                RowLayout {
                    id: sceneToolbar
                    anchors.top: parent.top; anchors.left: parent.left; anchors.topMargin: 12; anchors.leftMargin: 12; spacing: 5
                    ActionButton { objectName: "sceneHome"; iconName: "house"; display: AbstractButton.IconOnly; text: "Home"; tooltip: "Home · Restore saved perspective (Ctrl+Home)"; onClicked: app.restoreOrbit() }
                    ActionButton { objectName: "sceneFit"; iconName: "maximize"; display: AbstractButton.IconOnly; text: "Fit"; tooltip: "Fit · Frame the network (Ctrl+0)"; checked: app.fittedView && !app.frameAdjusted && app.frameScope === "network"; onClicked: app.frameScene(false) }
                    ActionButton { objectName: "sceneFocus"; iconName: "focus"; display: AbstractButton.IconOnly; text: "Focus"; enabled: controller.selectedId.length > 0; checked: app.fittedView && !app.frameAdjusted && app.frameScope === "selection" && app.frameSelectionKey === app.selectionFrameKey; tooltip: controller.selected.kind === "route" ? "Focus · Frame connection endpoints (F)" : "Focus · Frame selected equipment (F)"; onClicked: app.frameScene(true) }
                    ActionButton { objectName: "sceneTop"; iconName: "layers"; display: AbstractButton.IconOnly; text: app.topView ? "Isometric" : "Top"; tooltip: app.topView ? "Isometric · Return to perspective" : "Top · View the site from above"; checked: app.topView; onClicked: { if (app.topView) app.restoreOrbit(); else { app.frameScene(false); app.topView = true } } }
                    ActionButton { objectName: "sceneGrid"; iconName: "grid-3x3"; display: AbstractButton.IconOnly; text: "Layout grid"; checked: app.siteGridVisible; tooltip: "Layout grid · " + siteGround.gridStep + " layout units; editor snaps to 20"; onClicked: app.siteGridVisible = !app.siteGridVisible }
                    ActionButton {
                        id: editMenuButton; objectName: "sceneEdit"; iconName: "pencil"; display: AbstractButton.IconOnly
                        text: controller.selectedIds.length > 1 ? "Edit " + controller.selectedIds.length + " assets" : "Edit"; tooltip: text + " · Selection and layout tools"; onClicked: editMenu.open()
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
                    ActionButton { iconName: "panel-left"; display: AbstractButton.IconOnly; text: "Navigator"; visible: app.compactLayout; checked: app.navigatorVisible; onClicked: { const opening=!app.navigatorVisible; app.sceneOnly = false; workspaceSettings.setValue("navigatorOpen", true); app.compactPane = opening ? "navigator" : "" } }
                    ActionButton { iconName: "settings-2"; display: AbstractButton.IconOnly; text: "Inspector"; visible: app.compactLayout; checked: app.inspectorVisible; onClicked: { const opening=!app.inspectorVisible; app.sceneOnly = false; workspaceSettings.setValue("inspectorOpen", true); app.compactPane = opening ? "inspector" : "" } }
                    ActionButton { objectName: "mouseRecommendation"; Layout.leftMargin: 7; iconName: "mouse"; text: "Mouse recommended"; tooltip: "Scene controls and keyboard shortcuts (F1)"; onClicked: sceneHelpDialog.open() }
                }
                Label {
                    id: navigationHint
                    objectName: "sceneNavigationHints"
                    visible: scenePanel.height > 260 || app.connectMode
                    anchors.left: parent.left; anchors.top: sceneToolbar.bottom; anchors.leftMargin: 12; anchors.topMargin: 8
                    width: Math.min(parent.width - 24, implicitWidth)
                    padding: 8; wrapMode: Text.WordWrap
                    text: app.connectMode ? (app.connectionSource.length > 0 ? "Choose destination equipment" : "Choose source equipment") : "Drag to move · Right-drag to orbit · Middle-drag to pan · Scroll to zoom"
                    color: Theme.text; font.pixelSize: Theme.smallSize
                    background: Rectangle { color: "#B315191B"; radius: 6; border.color: "#26FFFFFF" }
                }
                Label { visible: !routeSummary.visible; anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: 14; width: Math.min(parent.width - 28, implicitWidth); padding: 7; elide: Text.ElideRight
                    text: app.hoveredConnection.message || "SCHEMATIC SITE · Layout does not determine hydraulic flow"; textFormat: Text.PlainText; color: Theme.text; font.pixelSize: Theme.captionSize
                    background: Rectangle { color: "#B315191B"; radius: 6; border.color: "#26FFFFFF" }
                }
                Pane {
                    id: routeSummary; objectName: "sceneRouteSummary"
                    readonly property var row: app.selectedNetworkRoute || ({})
                    readonly property bool condensed: scenePanel.height < 300
                    visible: !!app.selectedNetworkRoute && !app.connectMode
                    anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: 14
                    width: Math.min(370, parent.width - 28); padding: 10
                    Accessible.role: Accessible.StaticText
                    Accessible.name: "Selected connection. " + (row.label || "") + ". ID " + (row.id || "")
                        + ". " + (row.settingText || "") + ". " + (row.statusText || "") + ". Direction only; not measured delivery. Layout is schematic."
                    background: Rectangle { color: "#E0253139"; radius: 6; border.color: "#B7CBD4" }
                    contentItem: ColumnLayout {
                        spacing: 4
                        RowLayout {
                            Layout.fillWidth: true; spacing: 8
                            Label { objectName: "sceneRouteIdentity"; Layout.fillWidth: true; text: routeSummary.row.id || ""; textFormat: Text.PlainText; elide: Text.ElideRight; color: Theme.secondary; font.family: Theme.monoFamily; font.pixelSize: Theme.captionSize }
                            Label { objectName: "sceneRouteState"; text: routeSummary.row.statusText || ""; textFormat: Text.PlainText; color: routeSummary.row.available === false ? Theme.error : Theme.secondary; font.pixelSize: Theme.captionSize }
                        }
                        Label { objectName: "sceneRouteEndpoints"; Layout.fillWidth: true; text: routeSummary.row.label || ""; textFormat: Text.PlainText; elide: Text.ElideRight; color: Theme.text; font.bold: true; font.pixelSize: Theme.bodySize }
                        Label { objectName: "sceneRouteSetting"; Layout.fillWidth: true; visible: !routeSummary.condensed; text: routeSummary.row.settingText || ""; textFormat: Text.PlainText; elide: Text.ElideRight; color: Theme.secondary; font.pixelSize: Theme.smallSize }
                        Label { Layout.fillWidth: true; text: "Direction only · Not measured delivery"; textFormat: Text.PlainText; elide: Text.ElideRight; color: Theme.muted; font.pixelSize: Theme.captionSize }
                    }
                    ToolTip {
                        id: routeSummaryTip
                        visible: routeSummaryHover.hovered; delay: 500; timeout: 10000
                        width: Math.min(360, app.width - 32)
                        text: routeSummary.Accessible.name
                        contentItem: Label { text: routeSummaryTip.text; textFormat: Text.PlainText; wrapMode: Text.WrapAnywhere; color: Theme.text; font.pixelSize: Theme.smallSize }
                    }
                    HoverHandler { id: routeSummaryHover; blocking: false }
                }
                ActionFeedbackCard {
                    id: actionNotice
                    objectName: "actionFeedbackCard"
                    anchors.left: parent.left; anchors.bottom: parent.bottom; anchors.leftMargin: 14; anchors.bottomMargin: 38
                    width: Math.min(360, parent.width - 28)
                    z: 20
                }
                SceneAxes {
                    id: sceneOrientation
                    objectName: "workbenchOrientation"
                    projector: view; focusPoint: Qt.vector3d(cameraRig.x, 0, cameraRig.z)
                    viewState: [app.yaw, app.pitch, app.topView, app.distance, app.fittedDistance, app.fitSpan, cameraRig.x, cameraRig.z, planCamera.position,planCamera.horizontalMagnification,view.width,view.height,app.sceneReady]
                    gridVisible: app.siteGridVisible; gridStep: siteGround.gridStep
                    anchors.left: parent.left; anchors.bottom: parent.bottom; anchors.margins: 14
                    visible: !actionNotice.visible && scenePanel.width > 520 && scenePanel.height > 260
                }
            }

            AssetInspector {
                id: inspectorView
                visible: app.inspectorVisible
                Layout.preferredWidth: 286; Layout.minimumWidth: 286; Layout.fillHeight: true
                controller: app.controller
                numericRejections: app.rejectedNumericFields
                pipeTransportReady: app.pipeTransport.routeId.length > 0
                onNumericCommitted: function(field, value) { app.commitNumericField(field, value) }
                onPipeTransportRequested: pipeTransportDialog.open()
                onFocusRequested: app.frameScene(true)
            }
        }
        Rectangle {
            id: workbenchDock
            visible: !app.sceneOnly
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
            AgentPanel {
                id: agentPanel; anchors.fill: parent; anchors.margins: 14; anchors.topMargin: 48
                visible: app.showAgent && workspaceSettings.values.dockOpen; embedded: true
                currentNetworkHash: app.controller.networkHash; investigator: app.investigator; history: app.history
                scientificWorker: app.scientificWorker; storageScience: app.storageScience; credentials: app.credentials
                onOpenPipeEvidence: evidence => { if (app.pipeTransport.showRetainedResult(evidence)) pipeTransportDialog.open() }
            }
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

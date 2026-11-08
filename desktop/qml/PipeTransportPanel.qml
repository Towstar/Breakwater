pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Dialog {
    id: dialog
    objectName: "pipeTransportDialog"
    required property var pipeTransport
    property real preferredDialogWidth: 1120
    property real preferredDialogHeight: 740
    readonly property var resultData: pipeTransport.result || ({})
    readonly property var preview: resultData.preview || ({})
    readonly property var diagnostics: resultData.diagnostics || ({})
    readonly property bool viewingImported: pipeTransport.viewingImportedEvidence
    readonly property var displayedParameters: viewingImported ? (resultData.parameters || ({})) : pipeTransport.parameters
    readonly property bool hasResult: resultData.result_version === 1 && (preview.time_h || []).length > 0
    readonly property var outletScale: bounds(preview.outlet_mg_l || [], false)
    readonly property var fieldScale: bounds(preview.concentration_mg_l || [], true)
    parent: Overlay.overlay
    width: Math.min(preferredDialogWidth, parent.width - 236)
    height: Math.min(preferredDialogHeight, parent.height - 96)
    x: 220 + Math.round((parent.width - 236 - width) / 2)
    y: 76 + Math.max(0, Math.round((parent.height - 96 - height) / 2))
    modal: false
    title: "Experimental pipe transport"
    closePolicy: Popup.CloseOnEscape
    onOpened: transportTabs.currentIndex = 0
    padding: 14
    background: Rectangle { color: Theme.background; border.color: Theme.controlBorder; radius: 7 }
    header: Rectangle {
        implicitHeight: 54
        color: Theme.surface
        RowLayout {
            anchors.fill: parent; anchors.leftMargin: 16; anchors.rightMargin: 12; spacing: 12
            Label { Layout.fillWidth: true; text: dialog.title; textFormat: Text.PlainText; color: Theme.text; font.pixelSize: 17; font.weight: Font.DemiBold }
            ActionButton {
                objectName: "pipeTransportClose"
                iconName: "x"; display: AbstractButton.IconOnly; text: "Close"
                tooltip: "Close pipe transport"; accessibleName: "Close pipe transport"
                onClicked: dialog.close()
            }
        }
        Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 1; color: Theme.divider }
    }

    function number(value) {
        if (value === undefined || value === null || !Number.isFinite(Number(value))) return "—"
        const n = Number(value), magnitude = Math.abs(n)
        return magnitude > 1e7 || (magnitude > 0 && magnitude < 0.001) ? n.toExponential(5) : n.toFixed(4)
    }
    function last(values) { return values && values.length ? values[values.length - 1] : undefined }
    function axisNumber(value) {
        const n = Number(value), magnitude = Math.abs(n)
        return magnitude >= 10000 || (magnitude > 0 && magnitude < 0.01) ? n.toExponential(2) : n.toPrecision(4)
    }
    function signed(values) {
        const value = last(values)
        return value === undefined ? "—" : (Number(value) > 0 ? "+" : "") + number(value)
    }
    function bounds(values, nested) {
        let low = Infinity, high = -Infinity
        for (let i = 0; i < values.length; ++i) {
            const row = nested ? values[i] : [values[i]]
            for (let j = 0; j < row.length; ++j) {
                const value = Number(row[j])
                if (Number.isFinite(value)) { low = Math.min(low, value); high = Math.max(high, value) }
            }
        }
        return Number.isFinite(low) ? {low: low, high: high} : {low: 0, high: 0}
    }
    component PlainLabel: Label { textFormat: Text.PlainText; color: Theme.text; font.pixelSize: Theme.bodySize }
    component SectionLabel: PlainLabel { font.pixelSize: Theme.captionSize; font.letterSpacing: 1; color: Theme.secondary }
    onPreviewChanged: { outletPlot.requestPaint(); fieldPlot.requestPaint() }

    contentItem: ColumnLayout {
        spacing: 10
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: contextControls.implicitHeight + 20
            color: Theme.surface; border.color: Theme.divider; radius: Theme.cornerRadius
            ColumnLayout {
                id: contextControls
                anchors.fill: parent; anchors.margins: 10; spacing: 8
        RowLayout {
            Layout.fillWidth: true; spacing: 8
            PlainLabel { Layout.fillWidth: true; text: (dialog.viewingImported ? "Recorded route: " + dialog.resultData.route_id : "Current route: " + (dialog.pipeTransport.routeId || "Select a pipe in the scene")); wrapMode: Text.WrapAnywhere; maximumLineCount: 2; elide: Text.ElideRight; font.pixelSize: Theme.smallSize }
            PlainLabel { text: "NATIVE SCALAR REFERENCE · EXPERIMENTAL"; color: Theme.warning; font.pixelSize: Theme.captionSize }
        }
        PlainLabel {
            objectName: "importedPipeEvidenceBanner"
            Layout.fillWidth: true; visible: dialog.viewingImported
            text: "Imported historical evidence · unverified file · no approval"
            wrapMode: Text.WordWrap; color: Theme.warning; font.bold: true
        }
        RowLayout {
            Layout.fillWidth: true; spacing: 8; visible: !dialog.viewingImported
            PlainLabel { Layout.preferredWidth: 76; text: "Current run"; color: Theme.secondary; font.pixelSize: Theme.smallSize }
            ComboBox {
                id: runSelector
                objectName: "pipeTransportRunSelector"
                Layout.fillWidth: true; Layout.minimumWidth: 0
                model: dialog.pipeTransport.runs; textRole: "label"; valueRole: "run_id"
                enabled: count > 0 && !dialog.pipeTransport.busy && !dialog.viewingImported
                currentIndex: {
                    const rows = dialog.pipeTransport.runs
                    for (let i = 0; i < rows.length; ++i) if (rows[i].run_id === dialog.pipeTransport.selectedRunId) return i
                    return -1
                }
                displayText: currentIndex >= 0 ? model[currentIndex].label + (model[currentIndex].historical ? " · historical" : " · current") : "Select a stored numerical run"
                onActivated: dialog.pipeTransport.selectRun(currentValue)
                contentItem: PlainLabel { text: runSelector.displayText; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter; leftPadding: 8; rightPadding: 24 }
                delegate: ItemDelegate {
                    id: runOption
                    required property var modelData
                    required property int index
                    width: runSelector.width; highlighted: runSelector.highlightedIndex === index
                    contentItem: PlainLabel { text: runOption.modelData.label + (runOption.modelData.historical ? " · historical" : " · current"); elide: Text.ElideRight }
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true; spacing: 8
            ActionButton { iconName: "arrow-down-to-line"; objectName: "importPipeEvidence"; text: "Import JSON"; enabled: !dialog.pipeTransport.busy; onClicked: importDialog.open() }
            ActionButton { iconName: "rotate-ccw"; objectName: "showCurrentPipeResult"; text: "Current calculation"; visible: dialog.viewingImported; enabled: !dialog.pipeTransport.busy; onClicked: dialog.pipeTransport.showCurrentResult() }
            ActionButton { iconName: "arrow-up-from-line"; objectName: "exportPipeEvidence"; text: "Export JSON"; enabled: dialog.hasResult && !dialog.pipeTransport.busy; onClicked: exportDialog.open() }
            Item { Layout.fillWidth: true }
            Rectangle { Layout.preferredWidth: 1; Layout.preferredHeight: 24; color: Theme.divider; Layout.leftMargin: 2; Layout.rightMargin: 2 }
            ActionButton { iconName: "play"; objectName: "runPipeTransport"; text: "Run native transport"; visible: !dialog.viewingImported; highlighted: true; enabled: dialog.pipeTransport.canRun && !dialog.pipeTransport.busy && !dialog.viewingImported; onClicked: { transportTabs.currentIndex = 0; dialog.pipeTransport.run() } }
            ActionButton { iconName: "square"; objectName: "cancelPipeTransport"; text: "Cancel"; enabled: dialog.pipeTransport.busy; onClicked: dialog.pipeTransport.cancel() }
        }
        PlainLabel { Layout.fillWidth: true; text: dialog.pipeTransport.status; wrapMode: Text.WordWrap; maximumLineCount: 2; elide: Text.ElideRight; color: Theme.secondary; font.pixelSize: Theme.smallSize }
            }
        }
        TabBar {
            id: transportTabs
            objectName: "pipeTransportTabs"
            Layout.fillWidth: true
            background: Rectangle { color: Theme.raised; radius: Theme.cornerRadius; border.color: Theme.divider }
            TabButton { text: "Native results"; font.pixelSize: Theme.smallSize; implicitHeight: 36 }
            TabButton { text: dialog.viewingImported ? "Recorded parameters" : "Physical parameters"; font.pixelSize: Theme.smallSize; implicitHeight: 36 }
            TabButton { text: "Provenance"; font.pixelSize: Theme.smallSize; implicitHeight: 36 }
        }
        Rectangle {
            Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumHeight: 0
            color: Theme.background; border.color: Theme.divider; radius: Theme.cornerRadius
        StackLayout {
            anchors.fill: parent; anchors.margins: 12
            currentIndex: transportTabs.currentIndex
            ScrollView {
                id: resultScroll; contentWidth: availableWidth; clip: true
                ColumnLayout {
                    width: resultScroll.availableWidth; spacing: 9
                    PlainLabel { Layout.fillWidth: true; visible: !dialog.hasResult; text: "Import a previously exported native result, or select a pipe and captured run to execute the native scalar reference. No profile is displayed yet."; wrapMode: Text.WordWrap; color: Theme.secondary }
                    RowLayout {
                        Layout.fillWidth: true; visible: dialog.hasResult; spacing: 14
                        ColumnLayout {
                            Layout.fillWidth: true; spacing: 4
                            SectionLabel { text: "OUTLET CONCENTRATION · mg/L" }
                            Rectangle {
                                Layout.fillWidth: true; Layout.preferredHeight: Math.max(145, Math.min(230, dialog.height * 0.30))
                                color: Theme.surface; radius: 4; border.color: Theme.divider
                                Canvas {
                                    id: outletPlot
                                    objectName: "pipeOutletPlot"
                                    anchors.fill: parent
                                    onWidthChanged: requestPaint()
                                    onHeightChanged: requestPaint()
                                    onVisibleChanged: { if (visible) requestPaint() }
                                    onAvailableChanged: { if (available) requestPaint() }
                                    onPaint: {
                                        const ctx = getContext("2d")
                                        if (!ctx) return
                                        ctx.clearRect(0, 0, width, height)
                                        const times = dialog.preview.time_h || [], values = dialog.preview.outlet_mg_l || []
                                        if (!times.length || values.length !== times.length) return
                                        const left = 62, right = width - 12, top = 16, bottom = height - 30
                                        const range = dialog.outletScale
                                        // Normalize before subtraction so opposite-sign finite extrema
                                        // do not overflow. Bounds describe the axis, never alter samples.
                                        const magnitude = Math.max(Math.abs(range.low), Math.abs(range.high), 1e-300)
                                        const normalizedLow = range.low / magnitude, normalizedHigh = range.high / magnitude
                                        const padding = Math.max((normalizedHigh - normalizedLow) * 0.08, 0.02)
                                        const low = Math.max(-1, normalizedLow - padding), high = Math.min(1, normalizedHigh + padding)
                                        const start = Number(times[0]), end = Number(times[times.length - 1])
                                        if (!(end > start) || right <= left) return
                                        const x = function(t) { return left + (Number(t) - start) / (end - start) * (right - left) }
                                        const y = function(v) { return bottom - (Number(v) / magnitude - low) / (high - low) * (bottom - top) }
                                        ctx.font = "12px sans-serif"; ctx.lineWidth = 1; ctx.textAlign = "right"; ctx.textBaseline = "middle"
                                        for (let i = 0; i <= 2; ++i) {
                                            const value = (low + (high - low) * i / 2) * magnitude, py = bottom - i / 2 * (bottom - top)
                                            ctx.strokeStyle = Theme.divider; ctx.beginPath(); ctx.moveTo(left, py); ctx.lineTo(right, py); ctx.stroke()
                                            ctx.fillStyle = Theme.muted; ctx.fillText(dialog.axisNumber(value), left - 7, py)
                                        }
                                        ctx.strokeStyle = Theme.plotCandidate; ctx.lineWidth = 2; ctx.beginPath()
                                        for (let j = 0; j < values.length; ++j) {
                                            if (j === 0) ctx.moveTo(x(times[j]), y(values[j]))
                                            else ctx.lineTo(x(times[j]), y(values[j]))
                                        }
                                        ctx.stroke(); ctx.fillStyle = Theme.muted; ctx.textBaseline = "top"
                                        ctx.textAlign = "left"; ctx.fillText(dialog.number(start) + " h", left, bottom + 9)
                                        ctx.textAlign = "right"; ctx.fillText(dialog.number(end) + " h", right, bottom + 9)
                                    }
                                }
                            }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true; spacing: 4
                            SectionLabel { text: "SAMPLED TIME–DISTANCE FIELD · mg/L" }
                            Rectangle {
                                Layout.fillWidth: true; Layout.preferredHeight: Math.max(145, Math.min(230, dialog.height * 0.30))
                                color: Theme.surface; radius: 4; border.color: Theme.divider
                                Canvas {
                                    id: fieldPlot
                                    objectName: "pipeFieldPlot"
                                    anchors.fill: parent
                                    onWidthChanged: requestPaint()
                                    onHeightChanged: requestPaint()
                                    onVisibleChanged: { if (visible) requestPaint() }
                                    onAvailableChanged: { if (available) requestPaint() }
                                    onPaint: {
                                        const ctx = getContext("2d")
                                        if (!ctx) return
                                        ctx.clearRect(0, 0, width, height)
                                        const times = dialog.preview.time_h || [], positions = dialog.preview.x_m || [], field = dialog.preview.concentration_mg_l || []
                                        if (!times.length || !positions.length || field.length !== times.length) return
                                        const left = 55, right = width - 12, top = 16, bottom = height - 30
                                        const firstX = Number(positions[0]), lastX = Number(positions[positions.length - 1])
                                        const firstT = Number(times[0]), lastT = Number(times[times.length - 1])
                                        if (!(lastT > firstT) || right <= left) return
                                        const px = function(v) { return lastX > firstX ? left + (v - firstX) / (lastX - firstX) * (right - left) : left }
                                        const py = function(v) { return top + (v - firstT) / (lastT - firstT) * (bottom - top) }
                                        const range = dialog.fieldScale
                                        const magnitude = Math.max(Math.abs(range.low), Math.abs(range.high), 1e-300)
                                        const low = range.low / magnitude, span = range.high / magnitude - low
                                        for (let i = 0; i < times.length; ++i) {
                                            const y0 = py(i === 0 ? firstT : Number(times[i - 1]) / 2 + Number(times[i]) / 2)
                                            const y1 = py(i === times.length - 1 ? lastT : Number(times[i]) / 2 + Number(times[i + 1]) / 2)
                                            for (let j = 0; j < positions.length; ++j) {
                                                const x0 = positions.length === 1 ? left : px(j === 0 ? firstX : Number(positions[j - 1]) / 2 + Number(positions[j]) / 2)
                                                const x1 = positions.length === 1 ? right : px(j === positions.length - 1 ? lastX : Number(positions[j]) / 2 + Number(positions[j + 1]) / 2)
                                                const shade = span > 0 ? (Number(field[i][j]) / magnitude - low) / span : 0.5
                                                ctx.fillStyle = Qt.rgba(0.08 + 0.78 * shade, 0.22 + 0.49 * shade, 0.38 - 0.18 * shade, 1)
                                                ctx.fillRect(x0, y0, x1 - x0 + 0.5, y1 - y0 + 0.5)
                                            }
                                        }
                                        ctx.font = "12px sans-serif"; ctx.fillStyle = Theme.secondary; ctx.textAlign = "right"; ctx.textBaseline = "middle"
                                        ctx.fillText(dialog.number(firstT) + " h", left - 6, top + 2); ctx.fillText(dialog.number(lastT) + " h", left - 6, bottom - 2)
                                        ctx.textBaseline = "top"; ctx.textAlign = "left"; ctx.fillText(dialog.number(firstX) + " m", left, bottom + 9)
                                        ctx.textAlign = "right"; ctx.fillText(dialog.number(lastX) + " m", right, bottom + 9)
                                    }
                                }
                            }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true; visible: dialog.hasResult; spacing: 7
                        Rectangle { implicitWidth: 12; implicitHeight: 10; color: Qt.rgba(0.08, 0.22, 0.38, 1) }
                        PlainLabel { text: dialog.number(dialog.fieldScale.low); font.pixelSize: Theme.captionSize }
                        PlainLabel { text: "→"; color: Theme.secondary }
                        Rectangle { implicitWidth: 12; implicitHeight: 10; color: Qt.rgba(0.86, 0.71, 0.20, 1) }
                        PlainLabel { text: dialog.number(dialog.fieldScale.high) + " mg/L"; font.pixelSize: Theme.captionSize }
                        PlainLabel { Layout.fillWidth: true; text: "Displayed samples · no interpolation or value clipping"; font.pixelSize: Theme.captionSize; color: Theme.secondary; wrapMode: Text.WordWrap }
                    }
                    SectionLabel { visible: dialog.hasResult; text: "NATIVE MASS LEDGER · mg · FINAL STORED SAMPLE" }
                    GridLayout {
                        Layout.fillWidth: true; visible: dialog.hasResult; columns: 4; columnSpacing: 20; rowSpacing: 5
                        PlainLabel { text: "Initial inventory"; color: Theme.secondary }
                        PlainLabel { text: dialog.number((dialog.preview.inventory_mg || [])[0]) }
                        PlainLabel { text: "Final inventory"; color: Theme.secondary }
                        PlainLabel { text: dialog.number(dialog.last(dialog.preview.inventory_mg)) }
                        PlainLabel { text: "Inlet exchange (+/−)"; color: Theme.secondary }
                        PlainLabel { text: dialog.signed(dialog.preview.cumulative_inlet_mg) }
                        PlainLabel { text: "Outlet removed (−)"; color: Theme.secondary }
                        PlainLabel { text: dialog.number(dialog.last(dialog.preview.cumulative_outlet_mg)) }
                        PlainLabel { text: "Source added (+)"; color: Theme.secondary }
                        PlainLabel { text: dialog.signed(dialog.preview.cumulative_source_mg) }
                        PlainLabel { text: "Decay removed (−)"; color: Theme.secondary }
                        PlainLabel { text: dialog.number(dialog.last(dialog.preview.cumulative_decay_mg)) }
                        PlainLabel { text: "Balance residual"; color: Theme.secondary }
                        PlainLabel { text: dialog.signed(dialog.preview.mass_balance_residual_mg) }
                        PlainLabel { text: "Peak absolute residual"; color: Theme.secondary }
                        PlainLabel { text: dialog.number(dialog.diagnostics.max_mass_balance_residual_mg) }
                    }
                    PlainLabel { Layout.fillWidth: true; visible: dialog.hasResult; text: "Native balance: final − initial − inlet + outlet − source + decay = residual. Outlet and decay rows show raw removed-mass totals."; wrapMode: Text.WordWrap; font.pixelSize: Theme.captionSize; color: Theme.secondary }
                    PlainLabel { Layout.fillWidth: true; visible: dialog.hasResult; text: "Substeps: " + (dialog.diagnostics.substeps ?? "—") + " · Maximum CFL: " + dialog.number(dialog.diagnostics.max_cfl_number) + " · Minimum concentration: " + dialog.number(dialog.diagnostics.minimum_concentration_mg_l) + " mg/L"; wrapMode: Text.WordWrap; font.pixelSize: Theme.captionSize }
                    PlainLabel { Layout.fillWidth: true; text: "Synthetic scalar reference only. This profile does not establish network chemistry, pressure hydraulics, treatment performance or plan approval."; wrapMode: Text.WordWrap; color: Theme.warning; font.pixelSize: Theme.captionSize }
                }
            }
            ScrollView {
                id: parametersScroll; contentWidth: availableWidth; clip: true
                ColumnLayout {
                    width: parametersScroll.availableWidth; spacing: 13
                    PlainLabel { Layout.fillWidth: true; text: dialog.viewingImported ? "Recorded parameters from the imported file. These read-only values have not been applied to the current project. Choose Current calculation to return to the project's inputs." : "Physical pipe parameters are independent of the editor layout. Flow and horizon come from the selected stored numerical run."; wrapMode: Text.WordWrap; color: Theme.secondary }
                    GridLayout {
                        Layout.fillWidth: true; columns: dialog.width < 850 ? 1 : 2; columnSpacing: 24; rowSpacing: 10
                        Repeater {
                            model: [{key:"length_m",label:"Pipe length",unit:"m"},{key:"area_m2",label:"Cross-sectional area",unit:"m²"},{key:"dispersion_m2_h",label:"Dispersion",unit:"m²/h"},{key:"decay_per_h",label:"Decay rate",unit:"1/h"},{key:"cells",label:"Spatial cells",unit:"cells"},{key:"initial_mg_l",label:"Initial concentration",unit:"mg/L"},{key:"inlet_mg_l",label:"Inlet concentration",unit:"mg/L"},{key:"source_mg_l_h",label:"Uniform source",unit:"mg/L/h"},{key:"output_step_h",label:"Output interval",unit:"h"},{key:"cfl_safety",label:"CFL safety factor",unit:"dimensionless"}]
                            delegate: RowLayout {
                                id: parameterRow
                                required property var modelData
                                Layout.fillWidth: true; spacing: 8
                                PlainLabel { Layout.preferredWidth: 150; text: parameterRow.modelData.label; wrapMode: Text.WordWrap }
                                TextField { Layout.fillWidth: true; Layout.minimumWidth: 70; text: String(dialog.displayedParameters[parameterRow.modelData.key] ?? ""); enabled: !dialog.pipeTransport.busy && !dialog.viewingImported && dialog.pipeTransport.routeId.length > 0; inputMethodHints: Qt.ImhFormattedNumbersOnly; onEditingFinished: dialog.pipeTransport.setParameter(parameterRow.modelData.key, text) }
                                PlainLabel { Layout.preferredWidth: 88; text: parameterRow.modelData.unit; color: Theme.secondary }
                            }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true; spacing: 12
                        PlainLabel { text: "Boundary condition"; Layout.preferredWidth: 150 }
                        ComboBox {
                            id: boundarySelector; Layout.preferredWidth: 260
                            model: [{label:"Dirichlet inlet",value:"dirichlet_inlet"},{label:"Closed · no flux",value:"closed_no_flux"}]
                            textRole: "label"; valueRole: "value"
                            currentIndex: dialog.displayedParameters.boundary === "closed_no_flux" ? 1 : 0
                            enabled: !dialog.pipeTransport.busy && !dialog.viewingImported && dialog.pipeTransport.routeId.length > 0
                            onActivated: dialog.pipeTransport.setParameter("boundary", currentValue)
                        }
                    }
                    PlainLabel { Layout.fillWidth: true; text: dialog.viewingImported ? "Import checks the supported file structure and identities. It does not independently verify execution or establish a quality verdict." : "The native engine validates parameter values, numerical budgets and boundary compatibility. Changing a parameter requires a new transport calculation."; wrapMode: Text.WordWrap; color: Theme.secondary }
                }
            }
            ScrollView {
                contentWidth: availableWidth
                TextArea {
                    readOnly: true; selectByMouse: true; textFormat: TextEdit.PlainText; wrapMode: TextEdit.WrapAnywhere; font.pixelSize: Theme.smallSize
                    text: !dialog.hasResult ? "No computed transport result. Provenance will be captured from the selected run and physical pipe parameters." :
                          "Quality status: " + dialog.resultData.quality_status + "\nContext: " + (dialog.viewingImported ? "imported external file · historical and unverified · no approval" : dialog.resultData.historical ? "historical evidence" : "current captured run") +
                          "\nVolume forecast: " + (dialog.resultData.volume_feasible === undefined ? "not recorded" : dialog.resultData.volume_feasible ? "reported feasible" : "reported infeasible") +
                          "\n\nRoute: " + dialog.resultData.route_id + "\nRun: " + dialog.resultData.run_id + "\nSnapshot: " + dialog.resultData.snapshot_id +
                          "\nPhysical hash: " + dialog.resultData.physical_hash + "\nParameter hash: " + dialog.resultData.parameter_hash + "\nNumerical revision: " + dialog.resultData.numerical_revision +
                          "\n\nMethod: " + dialog.resultData.method + "\nMethod version: " + dialog.resultData.method_version + "\nHorizon: " + dialog.number(dialog.resultData.horizon_h) + " h\nRoute flow: " + dialog.number(dialog.resultData.flow_m3h) + " m³/h" +
                          "\n\nBounded preview: " + (dialog.preview.time_h || []).length + " time samples × " + (dialog.preview.x_m || []).length + " distance samples. Distance samples represent cell centers. Export JSON preserves this preview, parameters and diagnostics.\n\n" + (dialog.viewingImported ? "This imported file has not been independently re-executed. Its parameters and feasibility claim do not modify this project or restore approval. " : "") + "This is native scalar-reference evidence. It has no approval action and is separate from standalone learned-model experiments."
                }
            }
        }
        }
    }
    footer: Item {
        implicitHeight: 48
        Rectangle { anchors.fill: parent; color: Theme.surface; Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; height: 1; color: Theme.divider } }
        RowLayout {
            anchors.fill: parent; anchors.margins: 6; anchors.leftMargin: 14; anchors.rightMargin: 34; spacing: 12
            PlainLabel { Layout.fillWidth: true; text: "Drag the corner to resize · no simulated controls are changed"; font.pixelSize: Theme.captionSize; color: Theme.muted }
            ActionButton { iconName: "x"; text: "Close"; onClicked: dialog.close() }
        }
        MouseArea {
            id: resizeGrip
            objectName: "pipeTransportResizeGrip"
            anchors.right: parent.right; anchors.bottom: parent.bottom; width: 26; height: 26
            cursorShape: Qt.SizeFDiagCursor
            property point startPoint
            property real startWidth
            property real startHeight
            onPressed: function(mouse) { startPoint = mapToItem(null, mouse.x, mouse.y); startWidth = dialog.width; startHeight = dialog.height }
            onPositionChanged: function(mouse) {
                if (!pressed) return
                const point = mapToItem(null, mouse.x, mouse.y)
                dialog.preferredDialogWidth = Math.max(700, Math.min(dialog.parent.width - 236, startWidth + (point.x - startPoint.x) * 2))
                dialog.preferredDialogHeight = Math.max(440, Math.min(dialog.parent.height - 96, startHeight + (point.y - startPoint.y) * 2))
            }
            PlainLabel { anchors.centerIn: parent; text: "↘"; color: Theme.muted; font.pixelSize: 18 }
        }
    }
    FileDialog { id: importDialog; objectName: "importPipeEvidenceDialog"; title: "Import native pipe transport evidence"; fileMode: FileDialog.OpenFile; nameFilters: ["BREAKWATER transport result (*.json)"]; onAccepted: { transportTabs.currentIndex = 0; dialog.pipeTransport.importResult(selectedFile) } }
    FileDialog { id: exportDialog; objectName: "exportPipeEvidenceDialog"; title: "Export native pipe transport"; fileMode: FileDialog.SaveFile; defaultSuffix: "json"; nameFilters: ["BREAKWATER transport result (*.json)"]; onAccepted: dialog.pipeTransport.exportResult(selectedFile) }
}

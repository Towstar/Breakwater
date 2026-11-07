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
    readonly property bool hasResult: resultData.result_version === 1 && (preview.time_h || []).length > 0
    readonly property var outletScale: bounds(preview.outlet_mg_l || [], false)
    readonly property var fieldScale: bounds(preview.concentration_mg_l || [], true)
    parent: Overlay.overlay
    width: Math.min(preferredDialogWidth, parent.width - 32)
    height: Math.min(preferredDialogHeight, parent.height - 48)
    x: Math.round((parent.width - width) / 2)
    y: Math.round((parent.height - height) / 2)
    modal: true
    title: "Experimental pipe transport"
    closePolicy: Popup.CloseOnEscape
    onOpened: transportTabs.currentIndex = 0

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
    component PlainLabel: Label { textFormat: Text.PlainText; color: "#d2e1e9"; font.pixelSize: 12 }
    component SectionLabel: PlainLabel { font.pixelSize: 10; font.letterSpacing: 1; color: "#9eb5c2" }
    onPreviewChanged: { outletPlot.requestPaint(); fieldPlot.requestPaint() }

    contentItem: ColumnLayout {
        spacing: 7
        RowLayout {
            Layout.fillWidth: true; spacing: 8
            PlainLabel { Layout.fillWidth: true; text: "Route: " + (dialog.pipeTransport.routeId || "Select a pipe in the scene"); wrapMode: Text.WrapAnywhere; font.pixelSize: 11 }
            PlainLabel { text: "NATIVE SCALAR REFERENCE · EXPERIMENTAL"; color: "#ffcf70"; font.pixelSize: 10 }
        }
        RowLayout {
            Layout.fillWidth: true; spacing: 8
            PlainLabel { text: "Network run"; color: "#9eb5c2" }
            ComboBox {
                id: runSelector
                objectName: "pipeTransportRunSelector"
                Layout.fillWidth: true
                model: dialog.pipeTransport.runs; textRole: "label"; valueRole: "run_id"
                enabled: count > 0 && !dialog.pipeTransport.busy
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
            Button { objectName: "runPipeTransport"; text: "Run native transport"; highlighted: true; enabled: dialog.pipeTransport.canRun && !dialog.pipeTransport.busy; onClicked: { transportTabs.currentIndex = 0; dialog.pipeTransport.run() } }
            Button { text: "Cancel"; enabled: dialog.pipeTransport.busy; onClicked: dialog.pipeTransport.cancel() }
            Button { text: "Export JSON"; enabled: dialog.hasResult && !dialog.pipeTransport.busy; onClicked: exportDialog.open() }
        }
        PlainLabel { Layout.fillWidth: true; text: dialog.pipeTransport.status; wrapMode: Text.WordWrap; maximumLineCount: 2; elide: Text.ElideRight; color: "#60d2d1" }
        TabBar {
            id: transportTabs
            objectName: "pipeTransportTabs"
            Layout.preferredWidth: 540
            TabButton { text: "Native results" }
            TabButton { text: "Physical parameters" }
            TabButton { text: "Provenance" }
        }
        StackLayout {
            Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumHeight: 0
            currentIndex: transportTabs.currentIndex
            ScrollView {
                id: resultScroll; contentWidth: availableWidth; clip: true
                ColumnLayout {
                    width: resultScroll.availableWidth; spacing: 9
                    PlainLabel { Layout.fillWidth: true; visible: !dialog.hasResult; text: "Select a stored run and execute the native scalar reference. The viewer displays actual transport output; no profile has been computed yet."; wrapMode: Text.WordWrap; color: "#9eb5c2" }
                    RowLayout {
                        Layout.fillWidth: true; visible: dialog.hasResult; spacing: 14
                        ColumnLayout {
                            Layout.fillWidth: true; spacing: 4
                            SectionLabel { text: "OUTLET CONCENTRATION · mg/L" }
                            Rectangle {
                                Layout.fillWidth: true; Layout.preferredHeight: Math.max(145, Math.min(230, dialog.height * 0.30))
                                color: "#0e202d"; radius: 4
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
                                        const padding = Math.max((range.high - range.low) * 0.08, Math.abs(range.high) * 0.02, 1e-9)
                                        const low = range.low - padding, high = range.high + padding
                                        const start = Number(times[0]), end = Number(times[times.length - 1])
                                        if (!(end > start) || right <= left) return
                                        const x = function(t) { return left + (Number(t) - start) / (end - start) * (right - left) }
                                        const y = function(v) { return bottom - (Number(v) - low) / (high - low) * (bottom - top) }
                                        ctx.font = "10px sans-serif"; ctx.lineWidth = 1; ctx.textAlign = "right"; ctx.textBaseline = "middle"
                                        for (let i = 0; i <= 2; ++i) {
                                            const value = low + (high - low) * i / 2, py = y(value)
                                            ctx.strokeStyle = "#2b4353"; ctx.beginPath(); ctx.moveTo(left, py); ctx.lineTo(right, py); ctx.stroke()
                                            ctx.fillStyle = "#91aab9"; ctx.fillText(dialog.axisNumber(value), left - 7, py)
                                        }
                                        ctx.strokeStyle = "#5ed8d0"; ctx.lineWidth = 2; ctx.beginPath()
                                        for (let j = 0; j < values.length; ++j) {
                                            if (j === 0) ctx.moveTo(x(times[j]), y(values[j]))
                                            else ctx.lineTo(x(times[j]), y(values[j]))
                                        }
                                        ctx.stroke(); ctx.fillStyle = "#91aab9"; ctx.textBaseline = "top"
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
                                color: "#0e202d"; radius: 4
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
                                        const range = dialog.fieldScale, span = range.high - range.low
                                        for (let i = 0; i < times.length; ++i) {
                                            const y0 = py(i === 0 ? firstT : (Number(times[i - 1]) + Number(times[i])) / 2)
                                            const y1 = py(i === times.length - 1 ? lastT : (Number(times[i]) + Number(times[i + 1])) / 2)
                                            for (let j = 0; j < positions.length; ++j) {
                                                const x0 = positions.length === 1 ? left : px(j === 0 ? firstX : (Number(positions[j - 1]) + Number(positions[j])) / 2)
                                                const x1 = positions.length === 1 ? right : px(j === positions.length - 1 ? lastX : (Number(positions[j]) + Number(positions[j + 1])) / 2)
                                                const shade = span > 0 ? (Number(field[i][j]) - range.low) / span : 0.5
                                                ctx.fillStyle = Qt.rgba(0.08 + 0.78 * shade, 0.22 + 0.49 * shade, 0.38 - 0.18 * shade, 1)
                                                ctx.fillRect(x0, y0, x1 - x0 + 0.5, y1 - y0 + 0.5)
                                            }
                                        }
                                        ctx.font = "10px sans-serif"; ctx.fillStyle = "#b1c7d3"; ctx.textAlign = "right"; ctx.textBaseline = "middle"
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
                        PlainLabel { text: dialog.number(dialog.fieldScale.low); font.pixelSize: 10 }
                        PlainLabel { text: "→"; color: "#9eb5c2" }
                        Rectangle { implicitWidth: 12; implicitHeight: 10; color: Qt.rgba(0.86, 0.71, 0.20, 1) }
                        PlainLabel { text: dialog.number(dialog.fieldScale.high) + " mg/L"; font.pixelSize: 10 }
                        PlainLabel { Layout.fillWidth: true; text: "Displayed samples · no interpolation or value clipping"; font.pixelSize: 10; color: "#9eb5c2"; wrapMode: Text.WordWrap }
                    }
                    SectionLabel { visible: dialog.hasResult; text: "NATIVE MASS LEDGER · mg · FINAL STORED SAMPLE" }
                    GridLayout {
                        Layout.fillWidth: true; visible: dialog.hasResult; columns: 4; columnSpacing: 20; rowSpacing: 5
                        PlainLabel { text: "Initial inventory"; color: "#9eb5c2" }
                        PlainLabel { text: dialog.number((dialog.preview.inventory_mg || [])[0]) }
                        PlainLabel { text: "Final inventory"; color: "#9eb5c2" }
                        PlainLabel { text: dialog.number(dialog.last(dialog.preview.inventory_mg)) }
                        PlainLabel { text: "Inlet exchange (+/−)"; color: "#9eb5c2" }
                        PlainLabel { text: dialog.signed(dialog.preview.cumulative_inlet_mg) }
                        PlainLabel { text: "Outlet removed (−)"; color: "#9eb5c2" }
                        PlainLabel { text: dialog.number(dialog.last(dialog.preview.cumulative_outlet_mg)) }
                        PlainLabel { text: "Source added (+)"; color: "#9eb5c2" }
                        PlainLabel { text: dialog.signed(dialog.preview.cumulative_source_mg) }
                        PlainLabel { text: "Decay removed (−)"; color: "#9eb5c2" }
                        PlainLabel { text: dialog.number(dialog.last(dialog.preview.cumulative_decay_mg)) }
                        PlainLabel { text: "Balance residual"; color: "#9eb5c2" }
                        PlainLabel { text: dialog.signed(dialog.preview.mass_balance_residual_mg) }
                        PlainLabel { text: "Peak absolute residual"; color: "#9eb5c2" }
                        PlainLabel { text: dialog.number(dialog.diagnostics.max_mass_balance_residual_mg) }
                    }
                    PlainLabel { Layout.fillWidth: true; visible: dialog.hasResult; text: "Native balance: final − initial − inlet + outlet − source + decay = residual. Outlet and decay rows show raw removed-mass totals."; wrapMode: Text.WordWrap; font.pixelSize: 10; color: "#9eb5c2" }
                    PlainLabel { Layout.fillWidth: true; visible: dialog.hasResult; text: "Substeps: " + (dialog.diagnostics.substeps ?? "—") + " · Maximum CFL: " + dialog.number(dialog.diagnostics.max_cfl_number) + " · Minimum concentration: " + dialog.number(dialog.diagnostics.minimum_concentration_mg_l) + " mg/L"; wrapMode: Text.WordWrap; font.pixelSize: 11 }
                    PlainLabel { Layout.fillWidth: true; text: "Synthetic scalar reference only. This profile does not establish network chemistry, pressure hydraulics, treatment performance or plan approval."; wrapMode: Text.WordWrap; color: "#ffcf70"; font.pixelSize: 11 }
                }
            }
            ScrollView {
                id: parametersScroll; contentWidth: availableWidth; clip: true
                ColumnLayout {
                    width: parametersScroll.availableWidth; spacing: 13
                    PlainLabel { Layout.fillWidth: true; text: "Physical pipe parameters are independent of the editor layout. Flow and horizon come from the selected stored numerical run."; wrapMode: Text.WordWrap; color: "#9eb5c2" }
                    GridLayout {
                        Layout.fillWidth: true; columns: dialog.width < 850 ? 1 : 2; columnSpacing: 24; rowSpacing: 10
                        Repeater {
                            model: [{key:"length_m",label:"Pipe length",unit:"m"},{key:"area_m2",label:"Cross-sectional area",unit:"m²"},{key:"dispersion_m2_h",label:"Dispersion",unit:"m²/h"},{key:"decay_per_h",label:"Decay rate",unit:"1/h"},{key:"cells",label:"Spatial cells",unit:"cells"},{key:"initial_mg_l",label:"Initial concentration",unit:"mg/L"},{key:"inlet_mg_l",label:"Inlet concentration",unit:"mg/L"},{key:"source_mg_l_h",label:"Uniform source",unit:"mg/L/h"},{key:"output_step_h",label:"Output interval",unit:"h"},{key:"cfl_safety",label:"CFL safety factor",unit:"dimensionless"}]
                            delegate: RowLayout {
                                id: parameterRow
                                required property var modelData
                                Layout.fillWidth: true; spacing: 8
                                PlainLabel { Layout.preferredWidth: 150; text: parameterRow.modelData.label; wrapMode: Text.WordWrap }
                                TextField { Layout.fillWidth: true; Layout.minimumWidth: 70; text: String(dialog.pipeTransport.parameters[parameterRow.modelData.key] ?? ""); enabled: !dialog.pipeTransport.busy && dialog.pipeTransport.routeId.length > 0; inputMethodHints: Qt.ImhFormattedNumbersOnly; onEditingFinished: dialog.pipeTransport.setParameter(parameterRow.modelData.key, text) }
                                PlainLabel { Layout.preferredWidth: 88; text: parameterRow.modelData.unit; color: "#9eb5c2" }
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
                            currentIndex: dialog.pipeTransport.parameters.boundary === "closed_no_flux" ? 1 : 0
                            enabled: !dialog.pipeTransport.busy && dialog.pipeTransport.routeId.length > 0
                            onActivated: dialog.pipeTransport.setParameter("boundary", currentValue)
                        }
                    }
                    PlainLabel { Layout.fillWidth: true; text: "The native engine validates parameter values, numerical budgets and boundary compatibility. Changing a parameter requires a new transport calculation."; wrapMode: Text.WordWrap; color: "#9eb5c2" }
                }
            }
            ScrollView {
                contentWidth: availableWidth
                TextArea {
                    readOnly: true; selectByMouse: true; textFormat: TextEdit.PlainText; wrapMode: TextEdit.WrapAnywhere; font.pixelSize: 12
                    text: !dialog.hasResult ? "No computed transport result. Provenance will be captured from the selected run and physical pipe parameters." :
                          "Quality status: " + dialog.resultData.quality_status + "\nContext: " + (dialog.resultData.historical ? "historical evidence" : "current captured run") +
                          "\nVolume forecast: " + (dialog.resultData.volume_feasible === undefined ? "not recorded" : dialog.resultData.volume_feasible ? "reported feasible" : "reported infeasible") +
                          "\n\nRoute: " + dialog.resultData.route_id + "\nRun: " + dialog.resultData.run_id + "\nSnapshot: " + dialog.resultData.snapshot_id +
                          "\nPhysical hash: " + dialog.resultData.physical_hash + "\nParameter hash: " + dialog.resultData.parameter_hash + "\nNumerical revision: " + dialog.resultData.numerical_revision +
                          "\n\nMethod: " + dialog.resultData.method + "\nMethod version: " + dialog.resultData.method_version + "\nHorizon: " + dialog.number(dialog.resultData.horizon_h) + " h\nRoute flow: " + dialog.number(dialog.resultData.flow_m3h) + " m³/h" +
                          "\n\nBounded preview: " + (dialog.preview.time_h || []).length + " time samples × " + (dialog.preview.x_m || []).length + " distance samples. Distance samples represent cell centers. Export JSON preserves this preview, parameters and diagnostics.\n\nThis is native scalar-reference evidence. It has no approval action and is separate from standalone learned-model experiments."
                }
            }
        }
    }
    footer: Item {
        implicitHeight: 44
        RowLayout {
            anchors.fill: parent; anchors.margins: 6; anchors.rightMargin: 30
            PlainLabel { Layout.fillWidth: true; text: "Drag the corner to resize · no simulated controls are changed"; font.pixelSize: 10; color: "#8ca5b4" }
            Button { text: "Close"; onClicked: dialog.close() }
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
                dialog.preferredDialogWidth = Math.max(700, Math.min(dialog.parent.width - 32, startWidth + (point.x - startPoint.x) * 2))
                dialog.preferredDialogHeight = Math.max(440, Math.min(dialog.parent.height - 48, startHeight + (point.y - startPoint.y) * 2))
            }
            PlainLabel { anchors.centerIn: parent; text: "↘"; color: "#8ca5b4"; font.pixelSize: 18 }
        }
    }
    FileDialog { id: exportDialog; title: "Export native pipe transport"; fileMode: FileDialog.SaveFile; defaultSuffix: "json"; nameFilters: ["BREAKWATER transport result (*.json)"]; onAccepted: dialog.pipeTransport.exportResult(selectedFile) }
}

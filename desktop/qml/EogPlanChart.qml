import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: chart
    required property var candidate
    required property var baselineRun
    required property bool compatible
    property string facilityId: ""
    readonly property var candidateRows: (candidate && candidate.trajectories) || []
    readonly property var baselineRows: compatible ? ((baselineRun && baselineRun.trajectories) || []) : []
    function row(rows) {
        if (!rows) return null
        for (let i = 0; i < rows.length; ++i) if (rows[i].facility_id === facilityId) return rows[i]
        return null
    }
    function refresh() {
        if (!row(candidateRows)) facilityId = candidateRows && candidateRows.length ? candidateRows[0].facility_id : ""
        if (canvas) canvas.requestPaint()
    }
    onCandidateChanged: refresh()
    onBaselineRunChanged: refresh()
    onCandidateRowsChanged: refresh()
    onBaselineRowsChanged: refresh()
    onCompatibleChanged: { if (canvas) canvas.requestPaint() }
    onFacilityIdChanged: { if (canvas) canvas.requestPaint() }
    Component.onCompleted: refresh()
    RowLayout {
        Layout.fillWidth: true
        ComboBox {
            Layout.preferredWidth: 220; model: chart.candidateRows; textRole: "facility_id"
            currentIndex: { for (let i = 0; i < chart.candidateRows.length; ++i) if (chart.candidateRows[i].facility_id === chart.facilityId) return i; return -1 }
            onActivated: chart.facilityId = chart.candidateRows[currentIndex].facility_id
        }
        Label { Layout.fillWidth: true; wrapMode: Text.Wrap; color: "#9bb8c6"; text: "Candidate: cyan · baseline: amber · shutdown: red · capacity: gray. Values are not clipped." }
    }
    Canvas {
        id: canvas
        Layout.fillWidth: true; Layout.preferredHeight: 240
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        onPaint: {
            const ctx = getContext("2d")
            ctx.reset(); ctx.fillStyle = "#0b1822"; ctx.fillRect(0, 0, width, height)
            const c = chart.row(chart.candidateRows), b = chart.row(chart.baselineRows)
            if (!c || !c.elapsed_s || !c.elapsed_s.length) {
                ctx.fillStyle = "#9bb8c6"; ctx.font = "13px sans-serif"; ctx.fillText("No retained candidate trajectory", 20, 35); return
            }
            let scale = Math.max(1, Math.abs(c.capacity_bbl), Math.abs(c.shutdown_volume_bbl)), xmax = 0
            const rows = b ? [c, b] : [c]
            for (const item of rows) for (let i = 0; i < item.volumes_bbl.length; ++i) {
                scale = Math.max(scale, Math.abs(item.volumes_bbl[i])); xmax = Math.max(xmax, item.elapsed_s[i])
            }
            // Normalize before subtraction/padding: finite +/-1e308 readings
            // otherwise overflow the range and disappear into NaN coordinates.
            let ymin = 0, ymax = c.capacity_bbl / scale
            for (const item of rows) for (const value of item.volumes_bbl) { ymin = Math.min(ymin, value / scale); ymax = Math.max(ymax, value / scale) }
            const padding = Math.max(0.000001, (ymax - ymin) * 0.06); ymin -= padding; ymax += padding; xmax = Math.max(1, xmax)
            const left = 74, right = width - 18, top = 20, bottom = height - 44
            const x = value => left + value / xmax * (right - left)
            const y = value => bottom - (value / scale - ymin) / (ymax - ymin) * (bottom - top)
            ctx.font = "11px sans-serif"; ctx.fillStyle = "#adc3cf"; ctx.strokeStyle = "#385361"; ctx.lineWidth = 1
            ctx.beginPath(); ctx.moveTo(left, top); ctx.lineTo(left, bottom); ctx.lineTo(right, bottom); ctx.stroke()
            for (let i = 0; i <= 4; ++i) {
                const value = ymin + (ymax - ymin) * i / 4
                const raw = value * scale
                const label = isFinite(raw) ? (Math.abs(raw) > 1e6 ? raw.toExponential(1) : raw.toFixed(0)) : value.toFixed(2) + "×" + scale.toExponential(1)
                ctx.fillText(label, 7, bottom - (value - ymin) / (ymax - ymin) * (bottom - top) + 4)
                const time = xmax * i / 4; ctx.fillText((time / 3600).toFixed(2), x(time) - 11, bottom + 18)
            }
            ctx.fillText("bbl", 10, 13); ctx.fillText("hours after plan start", Math.max(left, width / 2 - 65), height - 5)
            function guide(value, color) { ctx.strokeStyle = color; ctx.beginPath(); ctx.moveTo(left, y(value)); ctx.lineTo(right, y(value)); ctx.stroke() }
            guide(c.capacity_bbl, "#8399a5"); guide(c.shutdown_volume_bbl, "#ed8181")
            function line(item, color) {
                ctx.strokeStyle = color; ctx.lineWidth = 2; ctx.beginPath()
                for (let i = 0; i < item.elapsed_s.length; ++i) {
                    if (!i) ctx.moveTo(x(item.elapsed_s[i]), y(item.volumes_bbl[i]))
                    else ctx.lineTo(x(item.elapsed_s[i]), y(item.volumes_bbl[i]))
                }
                ctx.stroke()
            }
            if (b) line(b, "#e5bd72")
            line(c, "#5cdbd1")
        }
    }
    Label { Layout.fillWidth: true; visible: !chart.compatible && chart.baselineRows.length === 0; text: "Overlay requires matching capture, plan start, context and complete repair horizons. Runs remain individually viewable."; color: "#e5bd72"; wrapMode: Text.Wrap }
}

import QtQuick

Item {
    id: chart
    property var samples: []
    property string unit: ""
    property bool discrete: false
    implicitHeight: 200
    function utcLabel(seconds) {
        const date = new Date(seconds * 1000)
        return isNaN(date.getTime()) ? String(seconds) : date.toISOString().slice(5, 16).replace("T", " ")
    }
    onSamplesChanged: plot.requestPaint()
    onUnitChanged: plot.requestPaint()
    onDiscreteChanged: plot.requestPaint()
    Canvas {
        id: plot
        anchors.fill: parent
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        onPaint: {
            const ctx = getContext("2d")
            ctx.reset(); ctx.fillStyle = "#101d28"; ctx.fillRect(0, 0, width, height)
            ctx.fillStyle = "#a9c0cc"; ctx.font = "11px sans-serif"
            const points = []
            for (let i = 0; i < chart.samples.length; ++i) {
                const s = chart.samples[i]
                if (typeof s.value === "number" && isFinite(s.value) && isFinite(s.event_time_s)) points.push({ t: s.event_time_s, v: s.value })
            }
            if (points.length === 0) { ctx.fillText("No readings at or before the selected time.", 12, 26); return }
            let minV = points[0].v, maxV = minV
            for (const p of points) { minV = Math.min(minV, p.v); maxV = Math.max(maxV, p.v) }
            const valueScale = Math.max(1, Math.abs(minV), Math.abs(maxV))
            const low = minV / valueScale, high = maxV / valueScale
            const padding = high === low ? 0.08 : (high - low) * 0.10
            const minT = points[0].t, maxT = points[points.length - 1].t
            const left = 76, right = Math.max(left + 1, width - 18), top = 26, bottom = Math.max(top + 1, height - 32)
            const x = function(t) { return maxT === minT ? (left + right) / 2 : left + (t - minT) / (maxT - minT) * (right - left) }
            const y = function(v) { return bottom - (v / valueScale - low + padding) / (high - low + 2 * padding) * (bottom - top) }
            ctx.strokeStyle = "#385363"; ctx.beginPath(); ctx.moveTo(left, top); ctx.lineTo(left, bottom); ctx.lineTo(right, bottom); ctx.stroke()
            ctx.fillText(maxV.toPrecision(4), 5, top + 4); ctx.fillText(minV.toPrecision(4), 5, bottom)
            ctx.fillText(chart.unit + (chart.discrete ? " · discrete status samples" : " · recorded samples"), left, 15)
            ctx.fillText(chart.utcLabel(minT) + " UTC", left, height - 8)
            ctx.fillText(chart.utcLabel(maxT) + " UTC", Math.max(left, right - 130), height - 8)
            let previous = null
            ctx.strokeStyle = "#5dd0cf"; ctx.fillStyle = "#5dd0cf"
            for (const p of points) {
                if (!chart.discrete && previous && p.t - previous.t <= 450) {
                    ctx.beginPath(); ctx.moveTo(x(previous.t), y(previous.v)); ctx.lineTo(x(p.t), y(p.v)); ctx.stroke()
                }
                if (chart.discrete || points.length < 100) { ctx.beginPath(); ctx.arc(x(p.t), y(p.v), 2, 0, 2 * Math.PI); ctx.fill() }
                previous = p
            }
        }
    }
}

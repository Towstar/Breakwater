import QtQuick
import QtQuick.Controls

Item {
    id: chart
    property var samples: []
    property string sensorId: ""
    property string unit: ""
    readonly property int numericSampleCount: {
        let count = 0
        for (const s of samples) {
            const value = s.canonical_value !== undefined ? s.canonical_value : s.value
            if (s.sensor_id === sensorId && typeof value === "number" && isFinite(value) && isFinite(s.event_time_s)) ++count
        }
        return count
    }
    implicitHeight: 190
    implicitWidth: 350
    onSamplesChanged: plot.requestPaint()
    onSensorIdChanged: plot.requestPaint()
    onUnitChanged: plot.requestPaint()
    Canvas {
        id: plot
        anchors.fill: parent
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            ctx.fillStyle = "#15181B"; ctx.fillRect(0, 0, width, height)
            const points = []
            for (let i = 0; i < chart.samples.length; ++i) {
                const s = chart.samples[i]
                if (s.sensor_id !== chart.sensorId) continue
                const value = s.canonical_value !== undefined ? s.canonical_value : s.value
                if (typeof value === "number" && isFinite(value) && isFinite(s.event_time_s))
                    points.push({ t: s.event_time_s, v: value, good: (s.reported_quality || s.quality) === "good" })
            }
            ctx.font = "12px sans-serif"; ctx.fillStyle = "#BBC2C9"
            if (points.length === 0) { ctx.fillText("Select a numeric sensor with retained readings.", 12, 24); return }
            points.sort(function(a, b) { return a.t - b.t })
            let minV = points[0].v, maxV = minV
            for (const p of points) { minV = Math.min(minV, p.v); maxV = Math.max(maxV, p.v) }
            const valueScale = Math.max(1, Math.abs(minV), Math.abs(maxV))
            const low = minV / valueScale, high = maxV / valueScale
            const padding = high === low ? 0.05 : (high - low) * 0.08
            const minT = points[0].t, maxT = points[points.length - 1].t
            const left = 65, right = Math.max(left + 1, width - 14), top = 24, bottom = Math.max(top + 1, height - 30)
            const x = function(t) { return minT === maxT ? (left + right) / 2 : left + (t - minT) / (maxT - minT) * (right - left) }
            const y = function(v) { return bottom - (v / valueScale - low + padding) / (high - low + 2 * padding) * (bottom - top) }
            ctx.strokeStyle = "#414950"; ctx.beginPath(); ctx.moveTo(left, top); ctx.lineTo(left, bottom); ctx.lineTo(right, bottom); ctx.stroke()
            const upper = (high + padding) * valueScale, lower = (low - padding) * valueScale
            ctx.fillText(isFinite(upper) ? upper.toPrecision(4) : "≥" + maxV.toPrecision(4), 4, top + 4)
            ctx.fillText(isFinite(lower) ? lower.toPrecision(4) : "≤" + minV.toPrecision(4), 4, bottom)
            ctx.fillText(minT.toFixed(2) + " s", left, height - 8)
            ctx.fillText(maxT.toFixed(2) + " s", Math.max(left, right - 66), height - 8)
            ctx.font = "13px sans-serif"; ctx.fillText(chart.unit, left, 14)
            let previous = null
            for (const p of points) {
                if (previous && previous.good && p.good) {
                    ctx.strokeStyle = "#F3F3F3"; ctx.beginPath(); ctx.moveTo(x(previous.t), y(previous.v)); ctx.lineTo(x(p.t), y(p.v)); ctx.stroke()
                }
                ctx.fillStyle = p.good ? "#F3F3F3" : "#E4B55B"
                ctx.beginPath(); ctx.arc(x(p.t), y(p.v), 2.5, 0, 2 * Math.PI); ctx.fill()
                previous = p
            }
        }
    }
}

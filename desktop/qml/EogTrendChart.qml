import QtQuick

Item {
    id: chart
    property var samples: []
    property var guides: []
    property string unit: ""
    property bool discrete: false
    property bool chartReady: false
    property int renderedSampleCount: 0
    property int renderedGuideCount: 0
    implicitHeight: 210
    objectName: "eogTrendChart"
    function utcLabel(seconds) {
        const date = new Date(seconds * 1000)
        return isNaN(date.getTime()) ? String(seconds) : date.toISOString().slice(5, 16).replace("T", " ")
    }
    function axisLabel(value) {
        if (value === 0) return "0"
        return Math.abs(value) >= 100000 || Math.abs(value) < 0.001 ? value.toExponential(2) : value.toPrecision(4)
    }
    function invalidatePlot() { chartReady = false; renderedSampleCount = 0; renderedGuideCount = 0; plot.requestPaint() }
    onSamplesChanged: invalidatePlot()
    onGuidesChanged: invalidatePlot()
    onUnitChanged: invalidatePlot()
    onDiscreteChanged: invalidatePlot()
    onVisibleChanged: invalidatePlot()
    Canvas {
        id: plot
        anchors.fill: parent
        onWidthChanged: chart.invalidatePlot()
        onHeightChanged: chart.invalidatePlot()
        onPaint: {
            const ctx = getContext("2d")
            ctx.reset(); ctx.fillStyle = "#15181B"; ctx.fillRect(0, 0, width, height)
            ctx.fillStyle = "#BBC2C9"; ctx.font = "12px sans-serif"
            const points = []
            for (let i = 0; i < chart.samples.length; ++i) {
                const s = chart.samples[i]
                if (typeof s.value === "number" && isFinite(s.value) && isFinite(s.event_time_s))
                    points.push({ t: s.event_time_s, v: s.value, gap: s.break_before === true })
            }
            if (points.length === 0) {
                chart.chartReady = false; chart.renderedSampleCount = 0; chart.renderedGuideCount = 0
                ctx.fillText("No readings at or before the selected time.", 12, 26); return
            }
            let minV = points[0].v, maxV = minV
            for (const p of points) { minV = Math.min(minV, p.v); maxV = Math.max(maxV, p.v) }
            const guideLines = []
            for (const guide of chart.guides) {
                if (guide.unit !== chart.unit || typeof guide.value !== "number" || !isFinite(guide.value)) continue
                guideLines.push({ value: guide.value, label: guide.label, kind: guide.kind })
                minV = Math.min(minV, guide.value); maxV = Math.max(maxV, guide.value)
            }
            // Normalize before padding so finite extreme observations cannot overflow.
            const valueScale = Math.max(1, Math.abs(minV), Math.abs(maxV))
            const low = minV / valueScale, high = maxV / valueScale
            const padding = high === low ? 0.08 : (high - low) * 0.10
            const axisLow = low - padding, axisHigh = high + padding
            const minT = points[0].t, maxT = points[points.length - 1].t
            const left = Math.min(90, Math.max(62, width * 0.13)), right = Math.max(left + 1, width - 18)
            const top = 28, bottom = Math.max(top + 1, height - 33)
            const x = function(t) { return maxT === minT ? (left + right) / 2 : left + (t - minT) / (maxT - minT) * (right - left) }
            const y = function(v) { return bottom - (v / valueScale - axisLow) / (axisHigh - axisLow) * (bottom - top) }
            const axisText = function(normalized) {
                const real = normalized * valueScale
                return isFinite(real) ? chart.axisLabel(real) : normalized.toPrecision(3) + "×" + valueScale.toExponential(1)
            }
            ctx.strokeStyle = "#414950"; ctx.beginPath()
            ctx.moveTo(left, top); ctx.lineTo(left, bottom); ctx.lineTo(right, bottom); ctx.stroke()
            ctx.fillText(axisText(axisHigh), 5, top + 4); ctx.fillText(axisText(axisLow), 5, bottom)
            ctx.font = "13px sans-serif"
            ctx.fillText(chart.unit + (chart.discrete ? " · status observations" : " · recorded observations"), left, 15)
            ctx.font = "12px sans-serif"
            ctx.fillText(chart.utcLabel(minT) + " UTC", left, height - 8)
            if (right - left > 250) ctx.fillText(chart.utcLabel(maxT) + " UTC", right - 130, height - 8)
            const guideColor = function(kind) {
                return kind === "shutdown" ? "#EB8585" : kind === "warning" ? "#E4B55B" : "#68727D"
            }
            for (const guide of guideLines) {
                guide.y = y(guide.value)
                ctx.strokeStyle = guideColor(guide.kind); ctx.setLineDash([6, 4])
                ctx.beginPath(); ctx.moveTo(left, guide.y); ctx.lineTo(right, guide.y); ctx.stroke()
            }
            ctx.setLineDash([])
            let previous = null
            ctx.strokeStyle = "#F3F3F3"; ctx.fillStyle = "#F3F3F3"
            for (const p of points) {
                // break_before comes from source gaps, never decimation spacing.
                if (!chart.discrete && previous && !p.gap) {
                    ctx.beginPath(); ctx.moveTo(x(previous.t), y(previous.v)); ctx.lineTo(x(p.t), y(p.v)); ctx.stroke()
                }
                if (chart.discrete || points.length < 100 || p.gap) { ctx.beginPath(); ctx.arc(x(p.t), y(p.v), 2, 0, 2 * Math.PI); ctx.fill() }
                previous = p
            }
            // Keep close warning/shutdown/capacity captions apart without moving
            // their actual dashed guide lines or changing any observed values.
            guideLines.sort(function(a, b) { return a.y - b.y })
            let lastBaseline = top - 2
            for (const guide of guideLines) {
                guide.baseline = Math.max(top + 13, lastBaseline + 15, guide.y - 4)
                lastBaseline = guide.baseline
            }
            lastBaseline = bottom + 10
            for (let i = guideLines.length - 1; i >= 0; --i) {
                const guide = guideLines[i]
                guide.baseline = Math.min(guide.baseline, bottom - 3, lastBaseline - 15)
                lastBaseline = guide.baseline
            }
            ctx.font = "12px sans-serif"
            for (const guide of guideLines) {
                const text = guide.label + " " + Number(guide.value.toPrecision(4)) + chart.unit
                const textWidth = ctx.measureText(text).width
                ctx.fillStyle = "#15181B"; ctx.fillRect(right - textWidth - 8, guide.baseline - 13, textWidth + 8, 16)
                ctx.strokeStyle = guideColor(guide.kind)
                if (Math.abs(guide.baseline - 4 - guide.y) > 7) {
                    ctx.beginPath(); ctx.moveTo(right, guide.y); ctx.lineTo(right, guide.baseline - 4); ctx.stroke()
                }
                ctx.fillStyle = guideColor(guide.kind); ctx.fillText(text, right - textWidth - 4, guide.baseline)
            }
            chart.renderedSampleCount = points.length
            chart.renderedGuideCount = guideLines.length
            chart.chartReady = chart.visible && width >= 160 && height >= 100
        }
    }
}

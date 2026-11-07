pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: chartRoot
    objectName: "trajectoryChart"
    required property var chart
    color: "#0e202d"; radius: 4
    onChartChanged: plot.requestPaint()
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 6; spacing: 0
        RowLayout {
            Layout.fillWidth: true; spacing: 14
            Repeater {
                model: chartRoot.chart.series || []
                delegate: Label {
                    required property var modelData
                    text: modelData.label + (modelData.label === "Baseline" ? " · dashed" : " · solid")
                    textFormat: Text.PlainText; font.pixelSize: 10
                    color: modelData.label === "Baseline" ? "#b8c9d6" : "#5ed8d0"
                }
            }
            Item { Layout.fillWidth: true }
            Label { text: "Stored volume · m³"; textFormat: Text.PlainText; font.pixelSize: 10; color: "#8ca5b4" }
        }
        Canvas {
            id: plot
            Layout.fillWidth: true; Layout.fillHeight: true
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
            onVisibleChanged: { if (visible) requestPaint() }
            onAvailableChanged: { if (available) requestPaint() }
            onPaint: {
                const ctx = getContext("2d")
                if (!ctx) return
                ctx.clearRect(0, 0, width, height)
                const data = chartRoot.chart
                if (!data.available || width < 90 || height < 35) return
                const low = Number(data.y_min), high = Number(data.y_max), horizon = Number(data.horizon_h)
                if (!(high > low) || !(horizon > 0)) return
                const left = 47, right = width - 12, top = 13, bottom = height - 24
                const x = function(t) { return left + Number(t) / horizon * (right - left) }
                const y = function(v) { return bottom - (Number(v) - low) / (high - low) * (bottom - top) }
                ctx.font = "10px sans-serif"; ctx.lineWidth = 1; ctx.setLineDash([])
                ctx.textAlign = "right"; ctx.textBaseline = "middle"
                for (let i = 0; i <= 2; ++i) {
                    const value = low + (high - low) * i / 2, py = y(value)
                    ctx.strokeStyle = "#2b4353"; ctx.beginPath(); ctx.moveTo(left, py); ctx.lineTo(right, py); ctx.stroke()
                    ctx.fillStyle = "#91aab9"; ctx.fillText(Number(value).toFixed(1), left - 7, py)
                }
                if (low < 0 && high > 0) {
                    ctx.strokeStyle = "#526c7c"; ctx.setLineDash([2, 3]); ctx.beginPath(); ctx.moveTo(left, y(0)); ctx.lineTo(right, y(0)); ctx.stroke()
                }
                const capacityY = y(data.capacity_m3)
                ctx.strokeStyle = "#e3b86c"; ctx.lineWidth = 1.2; ctx.setLineDash([6, 4])
                ctx.beginPath(); ctx.moveTo(left, capacityY); ctx.lineTo(right, capacityY); ctx.stroke()
                ctx.fillStyle = "#e3b86c"; ctx.textAlign = "right"; ctx.textBaseline = "bottom"
                ctx.fillText("Capacity " + Number(data.capacity_m3).toFixed(1) + " m³", right, capacityY - 3)
                const series = data.series || []
                for (let j = 0; j < series.length; ++j) {
                    const line = series[j], points = line.points || []
                    ctx.strokeStyle = line.label === "Baseline" ? "#b8c9d6" : "#5ed8d0"
                    ctx.lineWidth = 2; ctx.setLineDash(line.label === "Baseline" ? [3, 4] : [])
                    ctx.beginPath()
                    for (let k = 0; k < points.length; ++k) {
                        if (k === 0) ctx.moveTo(x(points[k].time_h), y(points[k].volume_m3))
                        else ctx.lineTo(x(points[k].time_h), y(points[k].volume_m3))
                    }
                    ctx.stroke()
                }
                ctx.setLineDash([]); ctx.fillStyle = "#91aab9"; ctx.textBaseline = "top"
                ctx.textAlign = "left"; ctx.fillText("0 h", left, bottom + 7)
                ctx.textAlign = "right"; ctx.fillText(horizon.toFixed(2) + " h", right, bottom + 7)
            }
        }
    }
    Label {
        anchors.centerIn: parent; width: parent.width - 30
        visible: !chartRoot.chart.available
        text: "Select a stored run and tank to inspect its trajectory."
        textFormat: Text.PlainText; wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter
        color: "#8ca5b4"; font.pixelSize: 12
    }
}

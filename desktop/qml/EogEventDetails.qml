pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: details
    required property var event
    // The unmodified eventEvidence() map: samples plus global qualifications.
    property var evidence: ({})
    readonly property var record: event || ({})
    readonly property var context: evidence || ({})
    readonly property var samples: context.evidence || []
    readonly property bool hasEvent: typeof record.event_id === "string" && record.event_id.length > 0
    spacing: 8

    function value(item) { return item === null || item === undefined || item === "" ? "unavailable" : String(item) }
    function number(item, unit) { return typeof item === "number" && isFinite(item) ? String(item) + " " + unit : "unavailable" }
    function timestamp(item) {
        if (typeof item !== "number" || !isFinite(item)) return "unavailable"
        const date = new Date(item * 1000)
        return isNaN(date.getTime()) ? "unavailable" : date.toISOString() + " · Unix " + String(item) + " s"
    }
    function bounds() {
        const returned = record.onset_bounds
        if (!returned) return "unavailable"
        return timestamp(returned.earliest_unix) + " → " + timestamp(returned.latest_unix)
    }
    function sample(item) {
        return "Evidence " + value(item.evidence_id) + "\nAsset " + value(item.asset_id) + " · Metric " + value(item.metric)
            + " · Value " + value(item.value) + " · Unit " + value(item.unit) + "\nAcquired " + timestamp(item.timestamp_unix)
            + "\nReported quality: " + value(item.quality)
    }

    component TextLine: Label {
        Layout.fillWidth: true
        textFormat: Text.PlainText
        wrapMode: Text.WrapAnywhere
        color: "#d4e3ec"
    }
    component Heading: TextLine { font.bold: true; color: "#9bb8c6" }
    component Records: ListView {
        id: recordsList
        required property var rows
        required property var describe
        property int rowEstimate: 44
        property int maximumHeight: 176
        model: rows || []
        Layout.fillWidth: true
        Layout.preferredHeight: Math.min(count * rowEstimate, maximumHeight)
        visible: count > 0
        clip: true
        spacing: 8
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}
        delegate: TextLine {
            required property var modelData
            width: recordsList.width - 20
            text: recordsList.describe(modelData)
        }
    }

    Heading { text: "Returned event and source evidence" }
    TextLine { objectName: "eventIdentity"; text: details.hasEvent ? "Event " + details.value(details.record.event_id) + " · Category " + details.value(details.record.category) + " · State " + details.value(details.record.state) : "No event is selected." }
    TextLine { objectName: "eventAsset"; text: "System " + details.value(details.record.system_id) + " · Facility " + details.value(details.record.facility_id) + " · Asset " + details.value(details.record.asset_id) }
    TextLine { objectName: "eventCoverage"; text: "Coverage: " + details.value(details.record.coverage) + " · Recommended action: " + details.value(details.record.recommended_action) }
    TextLine { objectName: "eventOnset"; text: "Returned onset: " + details.timestamp(details.record.onset_unix) }
    TextLine { objectName: "eventOnsetBounds"; text: "Returned onset bounds: " + details.bounds() }
    TextLine { text: "Onset metadata is returned evidence; missing bounds do not establish physical onset or calibrated confidence."; color: "#9bb8c6" }
    TextLine { objectName: "eventTimes"; text: "Updated: " + details.timestamp(details.record.updated_unix) + "\nEnd: " + details.timestamp(details.record.end_unix) }
    TextLine { objectName: "eventForecast"; text: "Shutdown countdown: " + details.number(details.record.countdown_hours, "h") + " · Lost revenue: " + details.number(details.record.lost_revenue_per_hour, "USD/h") }
    TextLine { objectName: "eventVersion"; text: "Threshold version: " + details.value(details.record.threshold_version) + " · Analysis request: " + details.value(details.context.analysis_request_id) }

    Heading { text: "Source samples · " + details.samples.length + " returned (scroll to inspect all)" }
    Records { objectName: "evidenceRows"; rows: details.samples; describe: details.sample; rowEstimate: 104; maximumHeight: 280 }
    TextLine { visible: !details.samples.length; text: "No source samples returned for this selection. Missing evidence is unavailable." }
    TextLine { text: "Values, timestamps, original units and reported quality are shown as returned. No clipping, conversion or additional fault diagnosis is performed."; color: "#9bb8c6" }

    Heading { text: "Event limitations · " + (details.record.limitations || []).length + " returned" }
    Records { objectName: "eventLimitationRows"; rows: details.record.limitations || []; describe: function(item) { return details.value(item) } }
    TextLine { visible: !(details.record.limitations || []).length; text: "No event limitations reported; this is not a qualification claim." }
    Heading { text: "Analysis assumptions · " + (details.context.assumptions || []).length + " returned" }
    Records { objectName: "assumptionRows"; rows: details.context.assumptions || []; describe: function(item) { return details.value(item) } }
    TextLine { visible: !(details.context.assumptions || []).length; text: "No analysis assumptions reported." }
    Heading { text: "Analysis limitations · " + (details.context.limitations || []).length + " returned" }
    Records { objectName: "analysisLimitationRows"; rows: details.context.limitations || []; describe: function(item) { return details.value(item) } }
    TextLine { visible: !(details.context.limitations || []).length; text: "No analysis limitations reported; this is not a qualification claim." }
    CheckBox { id: advanced; objectName: "advancedDisclosure"; text: "Advanced · exact returned event and evidence" }
    Loader {
        Layout.fillWidth: true
        active: advanced.checked
        sourceComponent: TextArea {
            objectName: "advancedJson"
            readOnly: true
            selectByMouse: true
            textFormat: TextEdit.PlainText
            wrapMode: TextEdit.WrapAnywhere
            font.family: "Consolas"
            text: JSON.stringify({event: details.record, evidence: details.context}, null, 2)
        }
    }
}

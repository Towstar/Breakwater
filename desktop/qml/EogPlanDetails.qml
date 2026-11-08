pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: details
    required property var evaluation
    required property var validation
    readonly property var assessment: evaluation || ({})
    readonly property var checked: validation || ({})
    readonly property var plan: assessment.plan || ({})
    readonly property var jobs: plan.truck_jobs || []
    readonly property var selectedJob: jobs[jobChoice.currentIndex] || ({})
    readonly property bool hasPlan: typeof plan.plan_id === "string" && plan.plan_id.length > 0
    readonly property bool hasValidation: typeof checked.validation_id === "string"
    spacing: 8

    function value(item) { return item === null || item === undefined || item === "" ? "unavailable" : String(item) }
    function number(item, unit) { return typeof item === "number" && isFinite(item) ? String(item) + " " + unit : "unavailable" }
    function money(item) { return number(item, "USD") }
    function elapsed(item) { return number(item, "s after start") }
    function admissible(item) { return item === true ? "admissible" : item === false ? "inadmissible" : "unavailable" }
    function outcome(item) {
        if (item === "avoids_shutdown") return "avoids shutdown"
        if (item === "automatic_shutdown") return "automatic shutdown"
        return value(item)
    }
    function costs(item) {
        const cost = item || ({})
        return "Truck cost: " + money(cost.truck_cost) + " · Lost revenue: " + money(cost.lost_revenue) + " · Total: " + money(cost.total)
    }
    function shutdown(item) { return "Facility " + value(item.facility_id) + " · automatic shutdown at " + elapsed(item.elapsed_s) }
    function violation(item) {
        return value(item.code) + " · asset " + value(item.asset_id) + " · " + elapsed(item.elapsed_s) + "\n" + value(item.message)
    }
    function startTime() {
        const time = plan.start_unix
        if (typeof time !== "number" || !isFinite(time)) return "unavailable"
        const date = new Date(time * 1000)
        return isNaN(date.getTime()) ? "unavailable" : date.toISOString() + " · Unix " + String(time) + " s"
    }

    component TextLine: Label {
        Layout.fillWidth: true
        textFormat: Text.PlainText
        wrapMode: Text.WrapAnywhere
        color: "#d4e3ec"
    }
    component Heading: TextLine { font.bold: true; color: "#9bb8c6" }
    // Each list retains its complete model and creates only visible delegates.
    // Long schedules remain scrollable without allocating every trip/evidence row.
    component Records: ListView {
        id: recordsList
        required property var rows
        required property var describe
        model: rows || []
        Layout.fillWidth: true
        Layout.preferredHeight: Math.min(count * 44, 176)
        visible: count > 0
        clip: true
        spacing: 6
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}
        delegate: TextLine {
            required property var modelData
            required property int index
            width: recordsList.width - 20
            text: recordsList.describe(modelData, index)
        }
    }

    Heading { text: "Plan and returned assessment" }
    TextLine { objectName: "planIdentity"; text: details.hasPlan ? "Plan " + details.value(details.plan.plan_id) + " · context " + details.value(details.plan.context) : "No evaluated plan is available." }
    TextLine { text: "Plan start (UTC): " + details.startTime() }
    TextLine {
        objectName: "assessmentStatus"
        text: "Admissibility: " + details.admissible(details.assessment.feasible) + " · Outcome: " + details.outcome(details.assessment.outcome) + " · Coverage: " + details.value(details.assessment.coverage)
        color: details.assessment.feasible === false ? "#ed8181" : "#d4e3ec"
    }
    TextLine { text: "Admissibility describes allowed actions. An admissible no-action plan may still reach automatic shutdown."; color: "#9bb8c6" }
    TextLine { objectName: "returnedCosts"; text: details.costs(details.assessment.cost) }

    Heading { text: "Repair horizons · " + (details.plan.repair_deadlines || []).length + " returned" }
    Records { objectName: "repairRows"; rows: details.plan.repair_deadlines || []; describe: function(item) { return "Facility " + details.value(item.facility_id) + " · repair horizon " + details.elapsed(item.repair_elapsed_s) } }
    TextLine { visible: !(details.plan.repair_deadlines || []).length; text: "No repair horizons reported." }

    Heading { text: "Truck schedules · " + details.jobs.length + " jobs" }
    ComboBox {
        id: jobChoice
        objectName: "jobChoice"
        Layout.fillWidth: true
        visible: details.jobs.length > 0
        model: details.jobs
        textRole: "job_id"
        currentIndex: count ? 0 : -1
        Accessible.name: "Truck job to inspect"
        contentItem: Label { text: jobChoice.displayText; textFormat: Text.PlainText; verticalAlignment: Text.AlignVCenter; color: jobChoice.palette.buttonText }
        delegate: ItemDelegate {
            id: jobDelegate
            required property var modelData
            required property int index
            width: jobChoice.width
            highlighted: jobChoice.highlightedIndex === index
            contentItem: Label { text: details.value(jobDelegate.modelData.job_id); textFormat: Text.PlainText; wrapMode: Text.WrapAnywhere }
        }
    }
    TextLine { visible: !details.jobs.length; text: details.hasPlan ? "No truck jobs proposed." : "Truck schedule unavailable." }
    ColumnLayout {
        visible: details.jobs.length > 0
        Layout.fillWidth: true
        TextLine { objectName: "selectedTruckIdentity"; text: "Job " + details.value(details.selectedJob.job_id) + " · Truck " + details.value(details.selectedJob.truck_id) + " · Facility " + details.value(details.selectedJob.facility_id) + " · System " + details.value(details.selectedJob.system_id) }
        TextLine { objectName: "selectedTruckTimes"; text: "Call " + details.elapsed(details.selectedJob.called_elapsed_s) + " · Available again " + details.elapsed(details.selectedJob.available_again_elapsed_s) + "\nResponse " + details.number(details.selectedJob.response_min, "min") + " · Cycle " + details.number(details.selectedJob.cycle_min, "min") }
        Heading { text: "Trips · " + (details.selectedJob.trips || []).length + " returned (scroll to inspect all)" }
        Records {
            objectName: "tripRows"
            rows: details.selectedJob.trips || []
            describe: function(item, index) { return "Trip " + (index + 1) + " · Arrival " + details.elapsed(item.arrival_elapsed_s) + " · Load " + details.number(item.volume_bbl, "bbl") + " · Cost " + details.money(item.cost) }
        }
    }
    Heading { text: "Reported fleet occupancy · " + (details.assessment.fleet_occupancy || []).length + " intervals" }
    Records { objectName: "occupancyRows"; rows: details.assessment.fleet_occupancy || []; describe: function(item) { return "Truck " + details.value(item.truck_id) + " · Job " + details.value(item.job_id) + " · " + details.elapsed(item.start_elapsed_s) + " → " + details.elapsed(item.end_elapsed_s) } }
    TextLine { visible: !(details.assessment.fleet_occupancy || []).length; text: "No fleet occupancy intervals reported." }

    Heading { text: "Well shut-ins · " + (details.plan.well_shut_ins || []).length + " proposed" }
    Records { objectName: "shutInRows"; rows: details.plan.well_shut_ins || []; describe: function(item) { return "Well " + details.value(item.well_id) + " · Facility " + details.value(item.facility_id) + " · " + details.elapsed(item.start_elapsed_s) + " → " + details.elapsed(item.end_elapsed_s) } }
    TextLine { visible: !(details.plan.well_shut_ins || []).length; text: details.hasPlan ? "No manual well shut-ins proposed." : "Well schedule unavailable." }
    Heading { text: "Inspection assets · " + (details.plan.inspection_assets || []).length + " proposed" }
    Records { rows: details.plan.inspection_assets || []; describe: function(item) { return "Asset " + details.value(item) } }
    Heading { text: "Returned shutdowns and violations" }
    Records { objectName: "shutdownRows"; rows: details.assessment.shutdowns || []; describe: details.shutdown }
    TextLine { visible: !(details.assessment.shutdowns || []).length; text: "No shutdown records reported; assessment coverage remains " + details.value(details.assessment.coverage) + "." }
    Records { objectName: "violationRows"; rows: details.assessment.violations || []; describe: details.violation }
    TextLine { visible: !(details.assessment.violations || []).length; text: "No violation records reported." }

    Heading { objectName: "validationHeading"; text: "Independent validation" }
    TextLine { objectName: "validationIdentity"; text: details.hasValidation ? "Validation " + details.value(details.checked.validation_id) + " · plan " + details.value(details.checked.plan_id) : "Independent validation unavailable." }
    TextLine { text: "Evaluator: " + details.value(details.checked.evaluator_method) + " · Validator: " + details.value(details.checked.validator_method) }
    TextLine { text: "Independent: " + (details.checked.independent === true ? "yes (reported)" : details.checked.independent === false ? "no" : "unavailable") + " · Admissibility: " + details.admissible(details.checked.feasible) + " · Outcome: " + details.outcome(details.checked.outcome) + " · Coverage: " + details.value(details.checked.coverage) }
    TextLine {
        objectName: "validationMargin"
        text: "Minimum shutdown margin: " + details.number(details.checked.minimum_shutdown_margin_bbl, "bbl")
        color: typeof details.checked.minimum_shutdown_margin_bbl === "number" && details.checked.minimum_shutdown_margin_bbl < 0 ? "#ed8181" : "#d4e3ec"
    }
    TextLine { text: "Validation costs · " + details.costs(details.checked.cost) }
    Records { rows: details.checked.validated_horizons || []; describe: function(item) { return "Validated facility " + details.value(item.facility_id) + " · repair horizon " + details.elapsed(item.repair_elapsed_s) } }
    Records { rows: details.checked.shutdowns || []; describe: details.shutdown }
    Records { rows: details.checked.violations || []; describe: details.violation }

    Heading { text: "Assumptions · " + (details.assessment.assumptions || []).length + " returned" }
    Records { objectName: "assumptionRows"; rows: details.assessment.assumptions || []; describe: function(item) { return details.value(item) } }
    TextLine { visible: !(details.assessment.assumptions || []).length; text: "No assumptions reported." }
    Heading { text: "Limitations · " + (details.assessment.limitations || []).length + " returned" }
    Records { objectName: "limitationRows"; rows: details.assessment.limitations || []; describe: function(item) { return details.value(item) } }
    TextLine { visible: !(details.assessment.limitations || []).length; text: "No limitations reported; missing evidence is not a qualification claim." }
    CheckBox { id: advanced; objectName: "advancedDisclosure"; text: "Advanced · exact plan, timing, hashes and returned evidence" }
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
            text: JSON.stringify({evaluation: details.assessment, validation: details.checked}, null, 2)
        }
    }
}

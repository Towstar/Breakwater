import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    required property var investigator
    spacing: 8
    RowLayout {
        Layout.fillWidth: true
        TextField {
            id: objective; Layout.fillWidth: true
            text: "Our main transfer pump failed. Find a validated response for the next three hours at low modeled cost."
            placeholderText: "Operator objective"
            maximumLength: 4000
            enabled: !investigator.running
        }
        Button { text: "Investigate"; highlighted: true; enabled: !investigator.running && objective.text.trim().length > 0; onClicked: investigator.investigate(objective.text) }
        Button { text: "Cancel"; enabled: investigator.running; onClicked: investigator.cancel() }
        Button { text: "Apply staged simulated plan"; enabled: investigator.canApprove; onClicked: investigator.approvePlan() }
    }
    Label { Layout.fillWidth: true; text: investigator.status; textFormat: Text.PlainText; color: "#60d2d1"; wrapMode: Text.WordWrap }
    RowLayout {
        Layout.fillWidth: true; Layout.fillHeight: true; spacing: 18
        ColumnLayout {
            Layout.fillWidth: true; Layout.fillHeight: true
            Label { text: "RECOMMENDATION / EVIDENCE"; font.pixelSize: 11; color: "#9eb5c2" }
            ScrollView {
                Layout.fillWidth: true; Layout.fillHeight: true; contentWidth: availableWidth
                TextArea {
                    readOnly: true; wrapMode: TextEdit.Wrap; textFormat: TextEdit.PlainText
                    text: {
                        const r = investigator.recommendation
                        if (!r || !r.status) return "Numerical tools use the real C++ engine. NVIDIA credentials are read from the process environment."
                        let summary = (r.explanation || "") + "\n\nRun: " + (r.selected_run_id || "none")
                        if (r.ok) summary += "\nCost: " + r.cost + " synthetic units · Horizon: " + r.horizon_h + " h\nValidation: " + r.validation_id
                        summary += "\nApproval: " + r.approval_status + "\nQuality not evaluated."
                        const controls = r.controls || []
                        for (let i = 0; i < controls.length; ++i) summary += "\n" + controls[i].control_id + " = " + controls[i].value + " m³/h"
                        return summary
                    }
                    font.pixelSize: 12
                }
            }
        }
        ColumnLayout {
            Layout.preferredWidth: 350; Layout.fillHeight: true
            Label { text: "STORED RUNS · " + investigator.runs.length; font.pixelSize: 11; color: "#9eb5c2" }
            ListView {
                Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 7
                model: investigator.runs
                delegate: Label {
                    required property var modelData
                    width: ListView.view.width; wrapMode: Text.WordWrap; textFormat: Text.PlainText; font.pixelSize: 12
                    text: (modelData.candidate_id || "baseline") + " · " + modelData.run_id.substring(0, 8) + "\nCost " + modelData.cost + " · " + modelData.validation_status + "\nHeadroom " + modelData.validation.minimum_headroom_m3 + " m³"
                    color: modelData.validation.feasible ? "#a8d9df" : "#f5aa9a"
                }
            }
        }
        ColumnLayout {
            Layout.preferredWidth: 365; Layout.fillHeight: true
            Label { text: "ACTIVITY · ACTUAL TOOL EXECUTIONS"; font.pixelSize: 11; color: "#9eb5c2" }
            ListView {
                Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 5
                model: investigator.events
                onCountChanged: positionViewAtEnd()
                delegate: Label {
                    required property var modelData
                    width: ListView.view.width; wrapMode: Text.WordWrap; textFormat: Text.PlainText; font.pixelSize: 11
                    text: modelData.sequence + "  " + modelData.type + (modelData.tool_name ? " · " + modelData.tool_name : "") + (modelData.run_id ? "\n" + modelData.run_id : "") + (modelData.error ? "\n" + modelData.error.message : "")
                    color: modelData.type === "tool_failed" || modelData.type === "agent_failed" ? "#f5aa9a" : "#b1c7d3"
                }
            }
        }
    }
}

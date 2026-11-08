#pragma once

#include "ProjectController.hpp"
#include <QJsonArray>
#include <QUuid>
#include <cmath>
#include <exception>
#include <utility>

namespace breakwater::desktop::history {

// A rehearsal export is gated by the exact human-applied intervention and its
// fresh native forecast. This object has no ability to approve or change controls.
class RehearsalEvidenceGuard final {
public:
    void setIncident(QJsonObject incident, QString physicalHash, QString priorCapture) {
        incident_ = std::move(incident); incident_hash_ = std::move(physicalHash);
        prior_capture_ = std::move(priorCapture);
    }
    bool pinned() const { return !expected_hash_.isEmpty(); }
    QString sessionId() const { return session_; }
    void rememberPriorCapture(const QString& id) { if (!pinned() && !id.isEmpty()) prior_capture_ = id; }

    bool pinApproval(const QVariantMap& recommendation, quint64 revision) {
        if (pinned()) return recommendationMatches(recommendation);
        const auto approved = QJsonObject::fromVariantMap(recommendation);
        if (incident_.isEmpty() || approved.value("physical_hash").toString() != incident_hash_ ||
            approved.value("approval_status") != "operator_applied" || !approved.value("controls").isArray()) return false;
        const auto session = approved.value("session_id").toString();
        const auto run = approved.value("selected_run_id").toString();
        const auto validation = approved.value("validation_id").toString();
        if (QUuid::fromString(session).isNull() || QUuid::fromString(run).isNull() || QUuid::fromString(validation).isNull()) return false;
        try {
            auto expected = parse_project(incident_);
            std::vector<ControlValue> controls;
            const auto overrides = approved.value("controls").toArray();
            for (const auto& item : overrides) {
                if (!item.isObject()) return false;
                const auto value = item.toObject();
                if (!value.value("control_id").isString() || !value.value("value").isDouble() ||
                    !std::isfinite(value.value("value").toDouble())) return false;
                controls.push_back({value.value("control_id").toString().toStdString(), value.value("value").toDouble()});
            }
            expected.network = compile(expected.network, controls).network;
            expected_hash_ = physics_hash(expected); revision_ = revision;
            session_ = session; run_ = run; validation_ = validation; overrides_ = overrides;
            return true;
        } catch (const std::exception&) { return false; }
    }
    bool physicalStateMatches(const ProjectController& project) const {
        return pinned() && project.simulationRevision() == revision_ && project.networkHash() == expected_hash_;
    }
    bool captureMatches(const QVariantMap& recommendation, const ProjectController& project) const {
        const auto capture = project.capturedForecast();
        return physicalStateMatches(project) && recommendationMatches(recommendation) && !project.busy() && capture &&
            capture->validation.feasible && capture->revision == revision_ && capture->physical_hash == expected_hash_ &&
            capture->id != prior_capture_ && (fresh_capture_.isEmpty() || capture->id == fresh_capture_);
    }
    bool acceptFreshCapture(const QVariantMap& recommendation, const ProjectController& project) {
        if (!captureMatches(recommendation, project)) return false;
        fresh_capture_ = project.capturedForecast()->id;
        return true;
    }
    bool storedApprovalMatches(const QJsonObject& stored) const {
        if (!pinned() || stored.value("session_id").toString() != session_ || stored.value("status") != "completed") return false;
        const auto recommendation = stored.value("recommendation").toObject();
        // loadSession deliberately normalizes recommendation approval_status to
        // historical. Durable operator authority is in the matching approval row.
        if (recommendation.value("selected_run_id").toString() != run_ ||
            recommendation.value("validation_id").toString() != validation_) return false;
        for (const auto& value : stored.value("approvals").toArray()) {
            const auto approval = value.toObject();
            if (approval.value("status") == "operator_applied" && approval.value("run_id").toString() == run_ &&
                approval.value("validation_id").toString() == validation_ &&
                approval.value("physical_hash").toString() == incident_hash_ && approval.value("controls").toArray() == overrides_) return true;
        }
        return false;
    }

private:
    bool recommendationMatches(const QVariantMap& current) const {
        return current.value("approval_status").toString() == "operator_applied" &&
            current.value("session_id").toString() == session_ && current.value("selected_run_id").toString() == run_ &&
            current.value("validation_id").toString() == validation_ &&
            QJsonArray::fromVariantList(current.value("controls").toList()) == overrides_;
    }
    QJsonObject incident_;
    QJsonArray overrides_;
    QString incident_hash_, expected_hash_, session_, run_, validation_, prior_capture_, fresh_capture_;
    quint64 revision_ = 0;
};

} // namespace breakwater::desktop::history

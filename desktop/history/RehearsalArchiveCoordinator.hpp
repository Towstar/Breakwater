#pragma once

#include "RehearsalEvidenceGuard.hpp"
#include "SessionRepository.hpp"
#include "agent/AgentController.hpp"
#include <QFileInfo>
#include <QTimer>
#include <functional>

namespace breakwater::desktop::history {

// Uses the production signal graph in both the desktop and regression tests.
// Approval is exclusively an input from the existing operator action.
class RehearsalArchiveCoordinator final : public QObject {
public:
    RehearsalArchiveCoordinator(ProjectController& project, agent::AgentController& investigator,
        SessionRepository& repository, QString destination, std::function<void()> completed,
        std::function<void(const QString&)> failed, QObject* parent = nullptr)
        : QObject(parent), project_(project), investigator_(investigator), repository_(repository),
          destination_(std::move(destination)), completed_(std::move(completed)), failed_(std::move(failed)) {
        deadline_.setSingleShot(true);
        connect(&deadline_, &QTimer::timeout, this, [this] { fail("approval_checkpoint_timeout"); });
        connect(&project_, &ProjectController::changed, this, [this] { advance(); });
        connect(&investigator_, &agent::AgentController::changed, this, [this] { advance(); });
        connect(&repository_, &SessionRepository::sessionSaved, this, [this](const QString&) { advance(); });
        connect(&repository_, &SessionRepository::sessionInspected, this, [this](const QJsonObject& stored) { inspected(stored); });
        connect(&repository_, &SessionRepository::operationCompleted, this, [this](const QString& operation, const QString& id) {
            if (!exporting_ || operation != "export" || id != guard_.sessionId() || finished_) return;
            if (!guard_.captureMatches(investigator_.recommendation(), project_)) { fail("approved_capture_changed_during_export"); return; }
            finished_ = true; deadline_.stop();
            if (completed_) completed_();
        });
        connect(&repository_, &SessionRepository::failed, this, [this](const QString&, const QString&) {
            if (begun_ && !finished_) fail("history_operation_failed");
        });
    }
    void begin() {
        guard_.setIncident(project_.document(), project_.networkHash(), project_.runId());
        begun_ = !destination_.isEmpty();
    }
    QString failureCode() const { return failure_; }

private:
    void fail(const QString& code) {
        if (finished_) return;
        finished_ = true; deadline_.stop(); failure_ = code;
        if (failed_) failed_(code);
    }
    void advance() {
        if (!begun_ || finished_ || inspecting_ || exporting_) return;
        const auto recommendation = investigator_.recommendation();
        if (recommendation.value("approval_status").toString() != "operator_applied") {
            guard_.rememberPriorCapture(project_.runId()); return;
        }
        if (!guard_.pinned() && !guard_.pinApproval(recommendation, project_.simulationRevision())) {
            fail("approval_identity_mismatch"); return;
        }
        if (!deadline_.isActive()) deadline_.start(30000);
        if (!guard_.physicalStateMatches(project_)) { fail("approved_state_changed"); return; }
        if (project_.busy()) return;
        if (!guard_.acceptFreshCapture(recommendation, project_)) return;
        inspecting_ = true;
        repository_.inspectSession(guard_.sessionId());
    }
    void inspected(const QJsonObject& stored) {
        if (!inspecting_ || stored.value("session_id").toString() != guard_.sessionId() || finished_) return;
        inspecting_ = false;
        if (!guard_.captureMatches(investigator_.recommendation(), project_)) { fail("approved_capture_changed_before_export"); return; }
        if (!guard_.storedApprovalMatches(stored)) {
            QTimer::singleShot(100, this, [this] { advance(); }); return;
        }
        // inspectSession's worker read already fully validated this archive.
        // Keep only bounded identity/approval checks on the GUI thread.
        if (QFileInfo::exists(destination_)) { fail("archive_destination_exists"); return; }
        exporting_ = true;
        repository_.exportSession(guard_.sessionId(), QUrl::fromLocalFile(destination_));
    }
    ProjectController& project_;
    agent::AgentController& investigator_;
    SessionRepository& repository_;
    RehearsalEvidenceGuard guard_;
    QString destination_, failure_;
    std::function<void()> completed_;
    std::function<void(const QString&)> failed_;
    QTimer deadline_;
    bool begun_ = false, inspecting_ = false, exporting_ = false, finished_ = false;
};

} // namespace breakwater::desktop::history

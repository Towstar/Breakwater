#include "AgentController.hpp"
#include "NemotronClient.hpp"
#include "history/SessionRecorder.hpp"
#include <QDateTime>

namespace breakwater::desktop::agent {
AgentController::AgentController(ProjectController* project, ModelTransport* transport, QObject* parent)
    : QObject(parent), project_(project) {
    if (!transport) transport = new NemotronClient(NemotronConfig::fromEnvironment(), this);
    if (const auto* client = qobject_cast<NemotronClient*>(transport)) configured_model_ = client->configuredModelIdentifier();
    session_ = new AgentOrchestrator(transport, this);
    connect(session_, &AgentOrchestrator::changed, this, &AgentController::changed);
    connect(project_, &ProjectController::changed, this, [this] {
        const auto gateway = session_->gateway();
        if (!applying_ && gateway && gateway->isCurrent() &&
            (gateway->physicalHash() != project_->networkHash() || captured_revision_ != project_->simulationRevision())) {
            recommendation_ = {};
            session_->invalidate();
        }
        emit changed();
    });
}

AgentController::~AgentController() {
    session_->cancel();
    if (recorder_) recorder_->flush();
}

void AgentController::setHistory(history::SessionRepository* repository, history::HistoryController* historyController) {
    if (!repository || recorder_) return;
    history_ = historyController;
    recorder_ = new history::SessionRecorder(session_, repository, historyController, configured_model_, this);
    storage_status_ = "Saving investigation history automatically";
    connect(repository, &history::SessionRepository::failed, this, [this](const QString&, const QString& message) {
        storage_status_ = "History storage error: " + message + " Current forecasts remain available."; emit changed();
    });
    connect(recorder_, &history::SessionRecorder::failed, this, [this](const QString& message) { storage_status_ = message; emit changed(); });
    if (historyController) connect(historyController, &history::HistoryController::changed, this, &AgentController::changed);
    emit changed();
}

QVariantList AgentController::runs() const {
    const auto gateway = session_->gateway();
    return gateway ? gateway->runs().toVariantList() : QVariantList{};
}

void AgentController::investigate(const QString& objective) {
    if (running()) return;
    if (recorder_) recorder_->flush();
    recommendation_ = {};
    const auto project = parse_project(project_->document());
    captured_revision_ = project_->simulationRevision();
    session_->start(std::make_shared<SimulationGateway>(project.network, project_->networkHash()), objective);
    if (history_) history_->showLive();
}

bool AgentController::canApprove() const {
    if (running() || !recommendation_.isEmpty()) return false;
    const auto gateway = session_->gateway();
    const auto result = session_->result();
    if (history_ && (history_->historical() || history_->selectedSessionId() != session_->sessionId() ||
        history_->candidateRunId() != result.value("selected_run_id").toString())) return false;
    if (!gateway || !gateway->isCurrent() || gateway->physicalHash() != project_->networkHash() || captured_revision_ != project_->simulationRevision() ||
        !result.value("actionable").toBool()) return false;
    const auto staged = gateway->stagedPlan();
    return staged.value("ok").toBool() && staged.value("selected_run_id").toString() == result.value("selected_run_id").toString();
}

bool AgentController::approvePlan() {
    if (!canApprove()) return false;
    const auto gateway = session_->gateway();
    const auto controls = gateway->stagedControls();
    applying_ = true;
    const bool applied = project_->applyControlsForApproval(controls, gateway->physicalHash());
    applying_ = false;
    if (!applied) return false;
    recommendation_ = session_->result();
    recommendation_["approval_status"] = "operator_applied";
    recommendation_["actionable"] = false;
    gateway->invalidate();
    if (recorder_) recorder_->recordApproval(recommendation_);
    project_->forecast();
    emit changed();
    return true;
}

QVariantMap AgentController::review() const {
    const auto gateway = session_->gateway();
    if (!gateway) return {{"eligible", false}, {"message", "No staged intervention is available."}};
    const auto stage = gateway->stagedPlan();
    if (!stage.value("ok").toBool()) return {{"eligible", false}, {"message", "A current independently validated staged plan is required."}};
    const auto network = parse_project(project_->document()).network;
    QMap<QString, ControlDescriptor> catalog;
    for (const auto& control : control_catalog(network)) catalog[QString::fromStdString(control.id)] = control;
    QJsonArray changes;
    for (const auto& value : stage.value("controls").toArray()) {
        const auto item = value.toObject(); const auto id = item.value("control_id").toString();
        if (!catalog.contains(id)) return {{"eligible", false}, {"message", "A staged control is no longer available."}};
        const auto& control = catalog[id];
        changes.append(QJsonObject{{"control_id", id}, {"before", control.value}, {"after", item.value("value")}, {"unit", QString::fromStdString(control.unit)}});
    }
    return QJsonObject{{"eligible", canApprove()}, {"message", canApprove() ? "Review the validated simulated intervention before applying." : "Return to the current recommended run; historical or mismatched selections cannot be applied."},
        {"run_id", stage.value("selected_run_id")}, {"validation_id", stage.value("validation_id")}, {"horizon_h", stage.value("horizon_h")},
        {"cost", stage.value("verified_cost")}, {"minimum_headroom_m3", stage.value("validation").toObject().value("minimum_headroom_m3")},
        {"controls", changes}, {"warnings", stage.value("warnings")}}.toVariantMap();
}
}

#include "AgentController.hpp"
#include "NemotronClient.hpp"

namespace breakwater::desktop::agent {
AgentController::AgentController(ProjectController* project, ModelTransport* transport, QObject* parent)
    : QObject(parent), project_(project) {
    if (!transport) transport = new NemotronClient(NemotronConfig::fromEnvironment(), this);
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

QVariantList AgentController::runs() const {
    const auto gateway = session_->gateway();
    return gateway ? gateway->runs().toVariantList() : QVariantList{};
}

void AgentController::investigate(const QString& objective) {
    if (running()) return;
    recommendation_ = {};
    const auto project = parse_project(project_->document());
    captured_revision_ = project_->simulationRevision();
    session_->start(std::make_shared<SimulationGateway>(project.network, project_->networkHash()), objective);
}

bool AgentController::canApprove() const {
    if (running() || !recommendation_.isEmpty()) return false;
    const auto gateway = session_->gateway();
    const auto result = session_->result();
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
    project_->forecast();
    emit changed();
    return true;
}
}

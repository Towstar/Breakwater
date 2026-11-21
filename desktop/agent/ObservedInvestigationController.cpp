#include "ObservedInvestigationController.hpp"
#include "NemotronClient.hpp"
#include "history/SessionRecorder.hpp"
#include "science/AgentSensitivityClient.hpp"
#include <QFutureWatcher>
#include <QMap>
#include <QtConcurrentRun>

namespace breakwater::desktop::agent {
ObservedInvestigationController::ObservedInvestigationController(ProjectController* project,telemetry::TelemetryController* monitor,
                                                                 ModelTransport* transport,QObject* parent)
    :QObject(parent),project_(project),telemetry_(monitor) {
    if (!transport) transport=owned_client_=new NemotronClient(NemotronConfig::fromEnvironment(),this);
    transport_=transport;
    if (const auto* client=qobject_cast<NemotronClient*>(transport)) configured_model_=client->configuredModelIdentifier();
    session_=new AgentOrchestrator(transport,this);
    session_->setSensitivityClient(new science::AgentSensitivityClient({},{},this));
    connect(session_,&AgentOrchestrator::changed,this,&ObservedInvestigationController::changed);
    connect(session_,&AgentOrchestrator::scientificEvidenceReady,this,[this](const QJsonObject& archive) { if (repository_) repository_->saveSession(archive); });
    if (project_) {
        connect(project_,&ProjectController::changed,this,&ObservedInvestigationController::sourceChanged);
        connect(project_,&QObject::destroyed,this,&ObservedInvestigationController::sourceChanged);
    }
    if (telemetry_) {
        connect(telemetry_,&telemetry::TelemetryController::observedCaptureChanged,this,&ObservedInvestigationController::sourceChanged);
        connect(telemetry_,&QObject::destroyed,this,&ObservedInvestigationController::sourceChanged);
    }
}
ObservedInvestigationController::~ObservedInvestigationController() { session_->cancel(); if (recorder_) recorder_->flush(); }
void ObservedInvestigationController::setHistory(history::SessionRepository* repository,history::HistoryController* history) {
    if (!repository || recorder_) return;
    repository_=repository; history_=history;
    recorder_=new history::SessionRecorder(session_,repository,history,configured_model_,this);
    storage_status_="Observed investigation history is saved automatically.";
    connect(repository,&history::SessionRepository::failed,this,[this](const QString&,const QString& message) {
        storage_status_="History storage error: "+message; emit changed();
    });
    connect(recorder_,&history::SessionRecorder::failed,this,[this](const QString& message) { storage_status_=message; emit changed(); });
    connect(recorder_,&history::SessionRecorder::pendingChanged,this,&ObservedInvestigationController::changed);
    connect(repository,&QObject::destroyed,this,[this] { storage_status_="Observed investigation history is unavailable."; emit changed(); });
    if (history) connect(history,&history::HistoryController::changed,this,&ObservedInvestigationController::changed);
    emit changed();
}
QString ObservedInvestigationController::storageStatus() const {
    return repository_ && recorder_ && recorder_->pending() && !storage_status_.startsWith("History storage error:")
        ? "Saving observed investigation evidence to History…":storage_status_;
}
bool ObservedInvestigationController::currentSource(bool requireAvailable) const {
    if (!project_ || !telemetry_ || !source_ || telemetry_->capturedObservation()!=source_) return false;
    const auto& document=source_->document; const auto disposition=telemetry_->observedCaptureDisposition();
    return document.value("mode")=="synthetic_live" && (disposition=="available" || (!requireAvailable && disposition=="unavailable")) &&
        project_->networkHash()==document.value("physical_hash") &&
        project_->simulationRevision()==static_cast<quint64>(document.value("numerical_revision").toInteger(-1));
}
bool ObservedInvestigationController::localSearchAvailable() const {
    if (running() || !telemetry_ || !project_) return false;
    const auto capture=telemetry_->capturedObservation(); if (!capture) return false;
    const auto disposition=telemetry_->observedCaptureDisposition();
    return capture->document.value("mode")=="synthetic_live" && (disposition=="available" || disposition=="unavailable") &&
        project_->networkHash()==capture->document.value("physical_hash") &&
        project_->simulationRevision()==static_cast<quint64>(capture->document.value("numerical_revision").toInteger(-1));
}
void ObservedInvestigationController::sourceChanged() {
    // Expiry keeps completed numerical evidence viewable, but cannot authorize
    // acknowledgement. Replacement/restart/edits retire the original session.
    if (source_) {
        const auto disposition=telemetry_ ? telemetry_->observedCaptureDisposition():QStringLiteral("retired");
        const bool same=project_ && telemetry_ && telemetry_->capturedObservation()==source_ &&
            project_->networkHash()==source_->document.value("physical_hash") &&
            project_->simulationRevision()==static_cast<quint64>(source_->document.value("numerical_revision").toInteger(-1));
        if (!same || disposition=="retired" || disposition=="historical") {
            ++generation_; status_="Observed source retired. Capture fresh evidence before a new investigation.";
            session_->invalidate();
        }
    }
    emit changed();
}
void ObservedInvestigationController::investigate(const QString& objective) { start(objective,false); }
void ObservedInvestigationController::searchLocally() {
    start("Investigate this immutable observed state with a native finite grid: at most four available controls, flow first, at most six values each. Preserve all unspecified controls. Advisory review only; no model or control application.",true);
}
void ObservedInvestigationController::start(const QString& objective,bool local) {
    if (!localSearchAvailable() || objective.trimmed().isEmpty()) {
        if (!running()) { status_="Capture fresh synthetic observations before investigating. Expired, replayed or retired sources are read only."; emit changed(); }
        return;
    }
    if (!local && owned_client_) { owned_client_->reloadConfiguration(); configured_model_=owned_client_->configuredModelIdentifier(); }
    if (!local && transport_ && transport_->admissionDelayMs() != 0) {
        status_="The model service is busy or cooling down. Wait before starting with fresh observed evidence.";
        emit changed(); return;
    }
    if (recorder_) recorder_->flush();
    if (session_->gateway()) session_->invalidate();
    if (recorder_) recorder_->setConfiguredModel(local ? "local-native-finite-search":configured_model_);
    source_=telemetry_->capturedObservation(); acknowledged_={}; preparing_=true;
    status_="Checking the immutable observed source…"; const auto epoch=++generation_; const auto source=source_;
    using Prepared=std::pair<std::shared_ptr<SimulationGateway>,QString>;
    auto* watcher=new QFutureWatcher<Prepared>(this);
    connect(watcher,&QFutureWatcher<Prepared>::finished,this,[this,watcher,epoch,source,objective,local] {
        const auto prepared=watcher->result(); watcher->deleteLater(); preparing_=false;
        if (epoch!=generation_ || source_!=source) { emit changed(); return; }
        if (!currentSource(false)) { status_="Observed source expired or retired during initialization. Capture fresh evidence."; emit changed(); return; }
        if (!prepared.first) { status_=prepared.second; emit changed(); return; }
        status_.clear(); session_->start(prepared.first,objective,{},local);
        if (history_) history_->showLive(); emit changed();
    });
    watcher->setFuture(QtConcurrent::run([source]()->Prepared {
        try { return {SimulationGateway::fromObservation(*source),{}}; }
        catch (const std::exception& e) { return {nullptr,QString::fromUtf8(e.what())}; }
        catch (...) { return {nullptr,"The bounded observed source could not be initialized."}; }
    }));
    emit changed();
}
void ObservedInvestigationController::cancel() {
    ++generation_; session_->cancel(); status_=preparing_ ? "Cancelled. The bounded source check is finishing.":"Cancelled. Completed evidence remains in History."; emit changed();
}
QVariantList ObservedInvestigationController::runs() const { const auto g=session_->gateway(); return g ? g->runs().toVariantList():QVariantList{}; }
bool ObservedInvestigationController::canAcknowledge() const {
    if (running() || !acknowledged_.isEmpty() || !currentSource(true)) return false;
    const auto g=session_->gateway(); const auto result=session_->result();
    if (!g || !g->isCurrent() || !g->advisoryOnly() || !result.value("verified_feasible").toBool()) return false;
    if (history_ && (history_->historical() || history_->selectedSessionId()!=session_->sessionId() ||
        history_->candidateRunId()!=result.value("selected_run_id").toString())) return false;
    const auto stage=g->stagedPlan();
    return stage.value("ok").toBool() && stage.value("selected_run_id")==result.value("selected_run_id") && stage.value("validation_id")==result.value("validation_id");
}
bool ObservedInvestigationController::acknowledgePlan() {
    if (!canAcknowledge()) return false;
    acknowledged_=session_->result(); acknowledged_["approval_status"]="operator_acknowledged_advisory"; acknowledged_["actionable"]=false;
    if (recorder_) recorder_->recordAdvisoryAcknowledgement(acknowledged_);
    emit changed(); return true;
}
QVariantMap ObservedInvestigationController::sourceSummary() const {
    const auto source=source_ ? source_:telemetry_ ? telemetry_->capturedObservation():nullptr;
    if (!source) return {{"status","No observed capture"}};
    const auto& d=source->document;
    return {{"snapshot_id",d.value("snapshot_id").toVariant()},{"design_physical_hash",d.value("physical_hash").toVariant()},
        {"effective_physical_hash",source->diagnostics.value("effective_physical_hash").toVariant()},
        {"as_of_s",d.value("captured_event_time_s").toVariant()},{"expires_s",d.value("expires_event_time_s").toVariant()},
        {"mode",d.value("mode").toVariant()},{"availability",d.value("availability").toVariant()},
        {"status",source== (telemetry_ ? telemetry_->capturedObservation():nullptr) && telemetry_ ? telemetry_->observedCaptureDisposition():"retired"},
        {"assumptions",d.value("assumptions").toArray().toVariantList()}};
}
QVariantMap ObservedInvestigationController::review() const {
    const auto g=session_->gateway(); const auto stage=g ? g->stagedPlan():QJsonObject{};
    auto result=sourceSummary(); result["eligible"]=canAcknowledge();
    result["message"]=canAcknowledge() ? "Review this nominal advisory response. Acknowledgement changes no controls.":
        "Advisory evidence is read only. Fresh coverage, the current recommended/staged run and a first acknowledgement are required.";
    if (!stage.value("ok").toBool() || !source_) return result;
    QMap<QString,QJsonObject> commands;
    for (const auto& value:source_->document.value("commands").toArray()) commands[value.toObject().value("control_id").toString()]=value.toObject();
    QJsonArray changes;
    for (const auto& value:stage.value("controls").toArray()) {
        const auto c=value.toObject(); const auto before=commands.value(c.value("control_id").toString());
        changes.append(QJsonObject{{"control_id",c.value("control_id")},{"before",before.value("value")},{"after",c.value("value")},{"unit",before.value("unit")}});
    }
    result["run_id"]=stage.value("selected_run_id").toVariant(); result["validation_id"]=stage.value("validation_id").toVariant();
    result["horizon_h"]=stage.value("horizon_h").toVariant(); result["cost"]=stage.value("verified_cost").toVariant();
    result["minimum_headroom_m3"]=stage.value("validation").toObject().value("minimum_headroom_m3").toVariant();
    result["controls"]=changes.toVariantList(); result["measurements"]=source_->diagnostics.value("bounds").toArray().toVariantList();
    auto warnings=stage.value("warnings").toArray(); warnings.append("Nominal constant-delivery forecast only. Declared observation uncertainty is retained but no robust feasibility guarantee is established. No control application or physical diagnosis.");
    result["warnings"]=warnings.toVariantList(); return result;
}
}

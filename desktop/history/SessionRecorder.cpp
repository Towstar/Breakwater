#include "SessionRecorder.hpp"
#include "EvidenceEnvelope.hpp"
#include <QDateTime>
#include <QFutureWatcher>
#include <QtConcurrentRun>

namespace breakwater::desktop::history {
SessionRecorder::SessionRecorder(agent::AgentOrchestrator* session, SessionRepository* repository, HistoryController* history,
                                 const QString& configuredModel, QObject* parent)
    : QObject(parent), session_(session), repository_(repository), history_(history), model_(configuredModel) {
    connect(session_, &agent::AgentOrchestrator::activity, this, [this](const QJsonObject& event) {
        const auto type = event.value("type").toString();
        if (type == "agent_started") {
            created_ = event.value("timestamp").toString(); objective_ = event.value("objective").toString();
            status_ = "running"; sequence_ = 0; approvals_ = {}; applied_ = {};
        } else if (type == "recommendation_ready") status_ = "completed";
        else if (type == "agent_failed") status_ = "failed";
        else if (type == "agent_cancelled") status_ = "cancelled";
    });
    connect(session_, &agent::AgentOrchestrator::changed, this, &SessionRecorder::checkpoint);
    if (repository) connect(repository,&SessionRepository::checkpointFinished,this,[this](const QString& id,quint64 sequence,bool success) {
        if (!capturing_ || id!=in_flight_id_ || (sequence!=in_flight_sequence_ && !(sequence==0 && !success))) return;
        capturing_=false; in_flight_id_.clear();
        if (pending_) { pending_=false; checkpoint(); } else emit pendingChanged();
    });
}
QJsonObject SessionRecorder::header() {
    return {{"archive_version", 1}, {"session_id", session_->sessionId()}, {"checkpoint_sequence", static_cast<qint64>(++sequence_)},
        {"created_at", created_}, {"updated_at", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
        {"status", status_}, {"objective", objective_}, {"model", QJsonObject{{"configured_model", model_}}},
        {"events", QJsonArray::fromVariantList(session_->events())}, {"recommendation", applied_.isEmpty() ? session_->result() : applied_},
        {"approvals", approvals_}};
}
QJsonObject SessionRecorder::complete(QJsonObject archive, const std::shared_ptr<agent::SimulationGateway>& gateway) {
    // Read-only accessors never count as model inspection or create validation evidence.
    archive["snapshot"] = gateway->snapshot();
    QJsonArray runs;
    for (const auto& value : gateway->runs()) runs.append(gateway->runDetails(value.toObject().value("run_id").toString()));
    archive["runs"] = runs; archive["current"] = gateway->isCurrent();
    if (gateway->advisoryOnly()) {
        archive["archive_version"]=2; archive["session_kind"]="observed_origin"; archive["actionable"]=false;
        archive["evidence_envelopes"]=QJsonArray{envelope("observed_origin",gateway->observedOrigin())};
    }
    return archive;
}
void SessionRecorder::publish(const QJsonObject& archive) {
    if (repository_) repository_->enqueueCheckpoint([archive] { return archive; },archive.value("session_id").toString());
    if (history_ && archive.value("session_id").toString() == session_->sessionId()) history_->setLiveSession(archive);
}
void SessionRecorder::checkpoint() {
    if (!repository_ || !session_->gateway() || session_->sessionId().isEmpty() || created_.isEmpty()) return;
    if (capturing_) { pending_ = true; return; }
    capturing_ = true;
    const auto metadata = header(); const auto gateway = session_->gateway();
    in_flight_id_=metadata.value("session_id").toString(); in_flight_sequence_=static_cast<quint64>(metadata.value("checkpoint_sequence").toInteger());
    auto* watcher = new QFutureWatcher<QJsonObject>(this);
    connect(watcher, &QFutureWatcher<QJsonObject>::finished, this, [this, watcher] {
        const auto archive = watcher->result(); watcher->deleteLater();
        if (!archive.isEmpty() && repository_) { publish(archive); return; }
        capturing_=false; in_flight_id_.clear();
        const QPointer<SessionRecorder> guard(this); emit failed("Unable to capture completed investigation evidence.");
        if (!guard) return;
        if (pending_) { pending_=false; checkpoint(); } else emit pendingChanged();
    });
    watcher->setFuture(QtConcurrent::run([metadata, gateway] {
        try { return complete(metadata, gateway); } catch (...) { return QJsonObject{}; }
    }));
    emit pendingChanged();
}
void SessionRecorder::flush() {
    if (!repository_ || !session_->gateway() || created_.isEmpty()) return;
    const auto metadata = header(); const auto gateway = session_->gateway();
    repository_->enqueueCheckpoint([metadata, gateway] { return complete(metadata, gateway); });
}
void SessionRecorder::recordApproval(const QJsonObject& recommendation) {
    recordReview(recommendation,"operator_applied");
}
void SessionRecorder::recordAdvisoryAcknowledgement(const QJsonObject& recommendation) {
    recordReview(recommendation,"operator_acknowledged_advisory");
}
void SessionRecorder::recordReview(const QJsonObject& recommendation,const QString& status) {
    applied_ = recommendation;
    approvals_.append(QJsonObject{{"timestamp", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
        {"run_id", recommendation.value("selected_run_id")}, {"validation_id", recommendation.value("validation_id")},
        {"physical_hash", recommendation.value("physical_hash")}, {"controls", recommendation.value("controls")}, {"status", status}});
    checkpoint();
}
}

#include "SessionRecorder.hpp"
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
    return archive;
}
void SessionRecorder::publish(const QJsonObject& archive) {
    if (repository_) repository_->saveSession(archive);
    if (history_ && archive.value("session_id").toString() == session_->sessionId()) history_->setLiveSession(archive);
}
void SessionRecorder::checkpoint() {
    if (!repository_ || !session_->gateway() || session_->sessionId().isEmpty() || created_.isEmpty()) return;
    if (capturing_) { pending_ = true; return; }
    capturing_ = true;
    const auto metadata = header(); const auto gateway = session_->gateway();
    auto* watcher = new QFutureWatcher<QJsonObject>(this);
    connect(watcher, &QFutureWatcher<QJsonObject>::finished, this, [this, watcher] {
        const auto archive = watcher->result(); watcher->deleteLater(); capturing_ = false;
        if (!archive.isEmpty()) publish(archive); else emit failed("Unable to capture completed investigation evidence.");
        if (pending_) { pending_ = false; checkpoint(); }
    });
    watcher->setFuture(QtConcurrent::run([metadata, gateway] {
        try { return complete(metadata, gateway); } catch (...) { return QJsonObject{}; }
    }));
}
void SessionRecorder::flush() {
    if (!repository_ || !session_->gateway() || created_.isEmpty()) return;
    const auto metadata = header(); const auto gateway = session_->gateway();
    repository_->enqueueCheckpoint([metadata, gateway] { return complete(metadata, gateway); });
}
void SessionRecorder::recordApproval(const QJsonObject& recommendation) {
    applied_ = recommendation;
    approvals_.append(QJsonObject{{"timestamp", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
        {"run_id", recommendation.value("selected_run_id")}, {"validation_id", recommendation.value("validation_id")},
        {"physical_hash", recommendation.value("physical_hash")}, {"controls", recommendation.value("controls")}, {"status", "operator_applied"}});
    checkpoint();
}
}

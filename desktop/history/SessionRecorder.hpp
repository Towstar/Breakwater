#pragma once
#include "HistoryController.hpp"
#include "agent/AgentOrchestrator.hpp"
#include <QPointer>

namespace breakwater::desktop::history {
class SessionRecorder final : public QObject {
    Q_OBJECT
public:
    SessionRecorder(agent::AgentOrchestrator* session, SessionRepository* repository, HistoryController* history,
                    const QString& configuredModel, QObject* parent = nullptr);
    void checkpoint();
    void flush();
    void recordApproval(const QJsonObject& recommendation);
    void recordAdvisoryAcknowledgement(const QJsonObject& recommendation);
    void setConfiguredModel(const QString& model) { model_ = model; }
    bool pending() const { return capturing_ || pending_; }
signals:
    void failed(const QString& message);
    void pendingChanged();
private:
    agent::AgentOrchestrator* session_;
    QPointer<SessionRepository> repository_;
    QPointer<HistoryController> history_;
    QString model_, created_, objective_, status_;
    QJsonArray approvals_;
    QJsonObject applied_;
    quint64 sequence_ = 0;
    QString in_flight_id_;
    quint64 in_flight_sequence_=0;
    bool capturing_ = false, pending_ = false;
    QJsonObject header();
    void publish(const QJsonObject& archive);
    static QJsonObject complete(QJsonObject header, const std::shared_ptr<agent::SimulationGateway>& gateway);
    void recordReview(const QJsonObject& recommendation,const QString& status);
};
}

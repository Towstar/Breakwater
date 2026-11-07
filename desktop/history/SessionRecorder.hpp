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
signals:
    void failed(const QString& message);
private:
    agent::AgentOrchestrator* session_;
    QPointer<SessionRepository> repository_;
    QPointer<HistoryController> history_;
    QString model_, created_, objective_, status_;
    QJsonArray approvals_;
    QJsonObject applied_;
    quint64 sequence_ = 0;
    bool capturing_ = false, pending_ = false;
    QJsonObject header();
    void publish(const QJsonObject& archive);
    static QJsonObject complete(QJsonObject header, const std::shared_ptr<agent::SimulationGateway>& gateway);
};
}

#pragma once
#include "ModelTransport.hpp"
#include "SimulationGateway.hpp"
#include <QElapsedTimer>
#include <QSet>
#include <QTimer>
#include <QVariantList>
#include <memory>

namespace breakwater::desktop::agent {
struct AgentLimits {
    int toolCalls = 12;
    int refinementRounds = 3;
    int durationMs = 120000;
};

class AgentOrchestrator final : public QObject {
    Q_OBJECT
public:
    explicit AgentOrchestrator(ModelTransport* transport, QObject* parent = nullptr);
    void start(std::shared_ptr<SimulationGateway> gateway, const QString& objective, AgentLimits limits = {});
    void cancel();
    void invalidate();
    bool running() const { return running_; }
    QString status() const { return status_; }
    QString sessionId() const { return session_id_; }
    QVariantList events() const { return events_; }
    QJsonObject result() const { return result_; }
    std::shared_ptr<SimulationGateway> gateway() const { return gateway_; }
signals:
    void changed();
    void activity(const QJsonObject& event);
private:
    ModelTransport* transport_;
    std::shared_ptr<SimulationGateway> gateway_;
    AgentLimits limits_;
    QTimer deadline_;
    QElapsedTimer elapsed_;
    QString session_id_, objective_, status_ = "Ready";
    QJsonArray messages_, pending_calls_;
    QSet<QString> seen_call_ids_;
    QVariantList events_;
    QJsonObject result_;
    quint64 generation_ = 0;
    int tool_calls_ = 0, refinement_rounds_ = 0, request_rounds_ = 0;
    bool running_ = false, awaiting_model_ = false, executing_tool_ = false;
    bool network_seen_ = false, controls_seen_ = false;
    bool recording_cancel_metadata_ = false;
    void requestModel();
    void receive(const QJsonObject& message);
    void nextTool();
    void finish(const QJsonObject& message);
    void fail(const QString& code, const QString& message);
    void event(const QString& type, QJsonObject fields = {});
};
}

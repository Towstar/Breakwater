#pragma once
#include "AgentOrchestrator.hpp"
#include "../ProjectController.hpp"

namespace breakwater::desktop::agent {
class AgentController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool running READ running NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QVariantList events READ events NOTIFY changed)
    Q_PROPERTY(QVariantList runs READ runs NOTIFY changed)
    Q_PROPERTY(QVariantMap recommendation READ recommendation NOTIFY changed)
    Q_PROPERTY(bool canApprove READ canApprove NOTIFY changed)
public:
    explicit AgentController(ProjectController* project, ModelTransport* transport = nullptr, QObject* parent = nullptr);
    bool running() const { return session_->running(); }
    QString status() const { return session_->status(); }
    QVariantList events() const { return session_->events(); }
    QVariantList runs() const;
    QVariantMap recommendation() const { return recommendation_.isEmpty() ? session_->result().toVariantMap() : recommendation_.toVariantMap(); }
    bool canApprove() const;
    Q_INVOKABLE void investigate(const QString& objective);
    Q_INVOKABLE void cancel() { session_->cancel(); }
    Q_INVOKABLE bool approvePlan();
signals:
    void changed();
private:
    ProjectController* project_;
    AgentOrchestrator* session_;
    QJsonObject recommendation_;
    bool applying_ = false;
    quint64 captured_revision_ = 0;
};
}

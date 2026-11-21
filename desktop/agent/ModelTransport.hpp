#pragma once

#include <QObject>
#include <QJsonArray>
#include <QJsonObject>

namespace breakwater::desktop::agent {

class ModelTransport : public QObject {
    Q_OBJECT
public:
    explicit ModelTransport(QObject* parent = nullptr) : QObject(parent) {}
    ~ModelTransport() override = default;
    virtual void request(const QJsonArray& messages, const QJsonArray& tools) = 0;
    // Existing transports/fixtures retain their ordinary no-tools behavior.
    virtual void requestFinalJson(const QJsonArray& messages) { request(messages, {}); }
    // Owners pass the remaining original investigation deadline. Existing
    // fixtures retain their request behavior; HTTP transports enforce the cap.
    virtual void requestWithBudget(const QJsonArray& messages, const QJsonArray& tools, int budgetMs, bool finalJson) {
        Q_UNUSED(budgetMs);
        if (finalJson) requestFinalJson(messages); else request(messages, tools);
    }
    virtual void cancel() = 0;
    // Readiness never grants approval or extends an observed capture. Negative
    // means another request has admission; zero means no known provider wait.
    virtual qint64 admissionDelayMs() const { return 0; }
signals:
    void requestStarted();
    void requestCompleted(const QJsonObject& metadata);
    void replyReady(const QJsonObject& assistantMessage);
    void requestFailed(const QString& code, const QString& message);
};

} // namespace breakwater::desktop::agent

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
    virtual void cancel() = 0;
signals:
    void requestStarted();
    void replyReady(const QJsonObject& assistantMessage);
    void requestFailed(const QString& code, const QString& message);
};

} // namespace breakwater::desktop::agent

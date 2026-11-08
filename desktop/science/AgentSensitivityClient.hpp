#pragma once

#include "ProjectController.hpp"
#include "PythonWorkerClient.hpp"
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <memory>

namespace breakwater::desktop::science {

// Agent-only worker lane. It never reads or changes the editor and never grants
// validation/approval authority. Captures and comparison work remain immutable.
class AgentSensitivityClient final : public QObject {
    Q_OBJECT
public:
    explicit AgentSensitivityClient(QString pythonExecutable = {}, QString workerPath = {}, QObject* parent = nullptr);
    ~AgentSensitivityClient() override;
    bool busy() const { return waiting_ || comparing_; }
    void analyze(std::shared_ptr<const CapturedForecast> capture, QJsonArray controls, QJsonObject origin);
    void cancel();
    void setTimeoutMs(int milliseconds);
    void setTerminationGraceMs(int milliseconds);
signals:
    void completed(const QJsonObject& outcome, const QJsonObject& archive);
    void failed(const QString& code, const QString& message);
private:
    PythonWorkerClient* worker_;
    std::shared_ptr<const CapturedForecast> capture_;
    QJsonArray controls_;
    QJsonObject origin_;
    quint64 generation_ = 0;
    bool waiting_ = false, comparing_ = false;
    void accept(const QJsonObject& response);
    void reject(const QString& code, const QString& message);
};

} // namespace breakwater::desktop::science

#pragma once
#include "EogCapture.hpp"
#include <QObject>
#include <QJsonObject>
#include <QPointer>
#include <QProcess>
#include <QStringList>
#include <QTimer>
#include <memory>

namespace breakwater::desktop::eog {
// One captured request per fresh process. A verified exchange is not numerical
// qualification. The executable/path/arguments overrides are also the fixture seam.
class EogAnalysisClient final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool configured READ configured NOTIFY changed)
    Q_PROPERTY(bool executionVerified READ executionVerified NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
public:
    explicit EogAnalysisClient(QString pythonExecutable = {}, QString workerPath = {},
                               QStringList workerArguments = {}, QObject* parent = nullptr);
    ~EogAnalysisClient() override;
    bool busy() const { return busy_; }
    bool configured() const;
    bool executionVerified() const { return execution_verified_; }
    QString status() const { return status_; }
    // Call on the owning thread. Busy requests are rejected without disturbing
    // the current job. The deadline includes asynchronous pre/post validation.
    bool start(const QJsonObject& request, std::shared_ptr<const Capture> capture, int timeoutMs = 30000);
    Q_INVOKABLE void cancel();
signals:
    void changed();
    void completed(const QJsonObject& response, const QJsonObject& sanitizedMetadata);
    void failed(const QString& code, const QString& message);
private:
    struct Job;
    std::shared_ptr<Job> job_;
    QString python_executable_, worker_path_;
    QStringList worker_arguments_;
    QString status_ = QStringLiteral("EOG provider is configured only when its local interpreter and worker exist; execution is unverified.");
    QPointer<QProcess> process_, retiring_;
    QTimer deadline_;
    QByteArray stdout_buffer_;
    qint64 stdout_bytes_ = 0, stderr_bytes_ = 0;
    quint64 generation_ = 0;
    bool busy_ = false, execution_verified_ = false, prepared_ = false;
    bool current(QProcess* process, quint64 generation) const;
    void launch();
    void readStdout(QProcess*, quint64);
    void readStderr(QProcess*, quint64);
    void finished(QProcess*, quint64, int exitCode, QProcess::ExitStatus);
    void retire(QProcess*);
    void fail(const QString& code, const QString& message);
    void complete(const QJsonObject& response);
};
} // namespace breakwater::desktop::eog

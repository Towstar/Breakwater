#pragma once

#include <QObject>
#include <QJsonObject>
#include <QPointer>
#include <QProcess>
#include <QTimer>
#include <QVariantMap>

namespace breakwater::desktop::science {

// Each job owns a fresh interpreter; results publish only after clean exit.
class PythonWorkerClient final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QVariantMap readiness READ readiness NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
public:
    explicit PythonWorkerClient(QString pythonExecutable = {}, QString workerPath = {}, QObject* parent = nullptr);
    ~PythonWorkerClient() override;
    QString status() const { return status_; }
    QVariantMap readiness() const { return readiness_; }
    QString operation() const;
    bool busy() const { return busy_; }
    Q_INVOKABLE void checkHealth();
    Q_INVOKABLE void checkCapabilities();
    void runStorage(const QJsonObject& payload);
    Q_INVOKABLE void cancel();
    // Test override for both job kinds; defaults are 10s health and 60s science.
    void setTimeoutMs(int milliseconds);
    void setTerminationGraceMs(int milliseconds);
signals:
    void changed();
    void healthReady(const QJsonObject& response);
    void capabilitiesReady(const QJsonObject& response);
    void storageReady(const QJsonObject& response);
    void failed(const QString& code, const QString& message);
private:
    static constexpr qsizetype maximumLineBytes = 1024 * 1024;
    static constexpr qint64 maximumJobBytes = 3 * maximumLineBytes;
    static constexpr qint64 maximumStderrBytes = 64 * 1024;
    QString python_executable_, worker_path_;
    QString status_ = QStringLiteral("Worker health has not been checked.");
    QVariantMap readiness_{{"worker_available", false}, {"health_status", "unchecked"},
                          {"scientific_execution", "not_implemented"}, {"storage_implemented", false},
                          {"storage_execution_verified", false}};
    enum class Job { Health, Capabilities, Storage };
    enum class Stage { Health, Capabilities, Storage, VerifiedCapabilities, Done };
    Job job_ = Job::Health;
    Stage stage_ = Stage::Health;
    QPointer<QProcess> process_, retiring_;
    QTimer deadline_;
    QByteArray stdout_buffer_;
    QJsonObject pending_response_, capabilities_, storage_payload_;
    QString request_id_;
    qint64 stdout_bytes_ = 0;
    qint64 stderr_bytes_ = 0;
    quint64 generation_ = 0;
    int timeout_ms_ = 0;
    int termination_grace_ms_ = 250;
    bool busy_ = false;
    void begin(Job job, const QJsonObject& payload = {});
    void launch();
    void send(const QString& method, const QJsonObject& payload);
    void acceptFrame(const QByteArray& line);
    void updateCapabilities(const QJsonObject& result);
    bool current(QProcess* process, quint64 generation) const;
    void readStdout(QProcess* process, quint64 generation);
    void readStderr(QProcess* process, quint64 generation);
    void processFinished(QProcess* process, quint64 generation, int exitCode, QProcess::ExitStatus exitStatus);
    void fail(const QString& code, const QString& message);
    void retire(QProcess* process, bool immediateKill = false);
    void complete();
};

} // namespace breakwater::desktop::science

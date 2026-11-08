#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QThread>
#include <QUrl>
#include <functional>

namespace breakwater::desktop::history {

class SessionRepositoryWorker;

// Every SQLite operation, including connection creation and teardown, belongs to
// the dedicated worker. Public operations are queued and report errors by signal.
class SessionRepository final : public QObject {
    Q_OBJECT
public:
    explicit SessionRepository(QString databasePath = {}, QObject* parent = nullptr);
    ~SessionRepository() override;
    void saveSession(const QJsonObject& archive);
    // The capture must use immutable data and thread-safe non-QObject accessors.
    // It runs on the history worker and is drained before repository destruction.
    void enqueueCheckpoint(std::function<QJsonObject()> capture, QString checkpointId = {});
    void listSessions();
    void loadSession(const QString& sessionId);
    // C++ evidence checkpoint inspection; does not select historical UI state or
    // restore approval. The caller must retain its own live authority checks.
    void inspectSession(const QString& sessionId);
    void exportSession(const QString& sessionId, const QUrl& destination);
    void importSession(const QUrl& source);

    static bool validateArchive(const QJsonObject& archive, QString* error = nullptr);
    static constexpr qint64 maximumArchiveBytes = 512LL * 1024 * 1024;

signals:
    void ready();
    void sessionsListed(const QJsonArray& sessions);
    void sessionLoaded(const QJsonObject& archive);
    void sessionInspected(const QJsonObject& archive);
    void sessionSaved(const QString& sessionId);
    void checkpointFailed(const QString& checkpointId, const QString& code, const QString& message);
    void operationCompleted(const QString& operation, const QString& sessionId);
    void failed(const QString& code, const QString& message);

private:
    QThread worker_thread_;
    SessionRepositoryWorker* worker_ = nullptr;
};

} // namespace breakwater::desktop::history

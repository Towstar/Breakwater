#pragma once

#include "EogCapture.hpp"
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QThread>
#include <QUrl>

namespace breakwater::desktop::eog {
class EogHistoryWorker;

// All database, capture, compression and archive work belongs to the worker.
// Returned evidence is historical. Review eligibility exists only in this process
// for newly begun, unretired sessions and never grants operating-control authority.
class EogHistoryRepository final : public QObject {
    Q_OBJECT
public:
    explicit EogHistoryRepository(QString databasePath = {}, QObject* parent = nullptr);
    ~EogHistoryRepository() override;

    void beginSession(const QString& id, const QString& objective, std::shared_ptr<const Capture> capture);
    void appendAnalysis(const QString& id, const QJsonObject& request, const QJsonObject& response,
                        const QJsonObject& metadata = {});
    void appendAgentEvent(const QString& id, const QJsonObject& event);
    void recordOperatorReview(const QString& id, const QString& analysisRequestId, const QString& note = {});
    void acknowledgeEvent(const QString& id, const QString& analysisRequestId, const QString& eventId,
                          const QString& note = {});
    void finishSession(const QString& id, const QString& status = QStringLiteral("completed"));
    void retireSession(const QString& id);
    void listSessions(int offset = 0);
    void loadSession(const QString& id);
    void exportSession(const QString& id, const QUrl& destination);
    void importSession(const QUrl& source);

    static constexpr qint64 maximumArchiveBytes = 256LL * 1024 * 1024;
    static constexpr int maximumAnalyses = 12;
    static constexpr int maximumOperatorRecords = 128;
    static constexpr int maximumAgentEvents = 128;
    static constexpr qint64 maximumAgentEventBytes = 64 * 1024;
    static constexpr qint64 maximumAgentTraceBytes = 4 * 1024 * 1024;

signals:
    void ready();
    void sessionSaved(const QString& id);
    void sessionsListed(const QJsonArray& sessions, int nextOffset);
    void sessionLoaded(const QJsonObject& archive);
    void archiveExported(const QString& id, const QUrl& destination);
    void archiveImported(const QString& id);
    void operatorRecordSaved(const QString& id, const QString& recordId);
    void failed(const QString& operation, const QString& sessionId, const QString& code, const QString& message);

private:
    QThread thread_;
    EogHistoryWorker* worker_ = nullptr;
};
} // namespace breakwater::desktop::eog

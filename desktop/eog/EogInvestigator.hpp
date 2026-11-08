#pragma once
#include "agent/ModelTransport.hpp"
#include <QElapsedTimer>
#include <QJsonArray>
#include <QPointer>
#include <QSet>
#include <QTimer>
#include <QVariantList>

namespace breakwater::desktop::eog {
inline constexpr int maximum_eog_investigation_duration_ms = 180000;
struct EogInvestigationLimits {
    int toolCalls = 12;
    int refinementRounds = 3;
    int durationMs = maximum_eog_investigation_duration_ms;
};

// EOG-only model loop. Numeric evidence/reference verification belongs to the
// controller/provider; a syntactically valid final is not a grounded recommendation.
// The transport is borrowed and must be exclusively scheduled while this loop runs.
class EogInvestigator final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool running READ running NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString sessionId READ sessionId NOTIFY changed)
    Q_PROPERTY(QVariantList events READ events NOTIFY changed)
public:
    explicit EogInvestigator(agent::ModelTransport* transport, QObject* parent = nullptr);
    ~EogInvestigator() override;
    void start(const QString& objective, const QJsonObject& context, const QJsonArray& toolDefinitions,
               EogInvestigationLimits limits = {});
    // callId is the opaque execution ID emitted by toolRequested, not the model ID.
    void completeTool(const QString& callId, const QJsonObject& outcome);
    void cancel();
    void invalidate();
    bool running() const { return running_; }
    QString status() const { return status_; }
    QString sessionId() const { return session_id_; }
    QVariantList events() const { return events_; }
    QJsonObject result() const { return result_; }
    QString finalRejectionReason() const { return final_rejection_reason_; }
    static bool isFinalFormatCorrectionCode(const QString& code);
signals:
    void changed();
    void toolRequested(const QString& callId, const QString& name, const QJsonObject& arguments);
    void finished(const QJsonObject& modelFinal);
    void failed(const QString& code, const QString& message);
    void activity(const QJsonObject& event);
    void requestMetadata(const QJsonObject& sanitized);
    // A single bounded formatting retry; reason is a fixed classifier literal.
    void finalCorrectionRequested(const QString& reason);
    // Abort the controller's asynchronous numeric operation on cancel/timeout/drift.
    void cancellationRequested();
private:
    struct Pending {
        QString modelId, executionId, name;
        QJsonObject arguments;
    };
    QPointer<agent::ModelTransport> transport_;
    QTimer deadline_;
    QElapsedTimer elapsed_;
    EogInvestigationLimits limits_;
    QJsonArray messages_, tools_, queued_calls_;
    QSet<QString> seen_ids_, offered_names_;
    QVariantList events_;
    QJsonObject result_;
    QString session_id_, status_ = QStringLiteral("Ready"), final_rejection_reason_;
    Pending pending_;
    QList<QMetaObject::Connection> request_connections_;
    quint64 generation_ = 0, request_serial_ = 0;
    int tool_calls_ = 0, refinement_rounds_ = 0, request_rounds_ = 0;
    bool running_ = false, awaiting_model_ = false, executing_tool_ = false;
    bool recording_cancel_metadata_ = false, metadata_seen_ = false, started_seen_ = false;
    bool final_correction_used_ = false, json_final_phase_ = false;
    void disconnectRequest();
    bool appendMessage(const QJsonObject& message);
    bool recordEvent(const QString& type, QJsonObject fields = {});
    void requestModel();
    void receive(const QJsonObject& message);
    void nextTool();
    void stop(const QString& code, const QString& message, bool recordFailure = true);
    void completeFinal(const QJsonObject& message);
    void recordMetadata(const QJsonObject& metadata, quint64 generation, quint64 request);
};
} // namespace breakwater::desktop::eog


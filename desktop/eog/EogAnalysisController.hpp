#pragma once
#include "EogAnalysisClient.hpp"
#include "EogDatasetController.hpp"
#include "EogHistoryRepository.hpp"
#include "EogInvestigator.hpp"
#include <QPointer>
#include <QVariantList>
#include <QVariantMap>
#include <functional>

namespace breakwater::desktop::eog {
// EOG evidence and advisory review have no connection to sandbox controls.
class EogAnalysisController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool providerConfigured READ providerConfigured NOTIFY changed)
    Q_PROPERTY(bool providerVerified READ providerVerified NOTIFY changed)
    Q_PROPERTY(bool historical READ historical NOTIFY changed)
    Q_PROPERTY(bool canReview READ canReview NOTIFY changed)
    Q_PROPERTY(bool comparisonCompatible READ comparisonCompatible NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString storageStatus READ storageStatus NOTIFY changed)
    Q_PROPERTY(QString sessionId READ sessionId NOTIFY changed)
    Q_PROPERTY(QString summary READ summary NOTIFY changed)
    Q_PROPERTY(QString selectedPlanId READ selectedPlanId NOTIFY changed)
    Q_PROPERTY(QString baselinePlanId READ baselinePlanId NOTIFY changed)
    Q_PROPERTY(QVariantList events READ events NOTIFY changed)
    Q_PROPERTY(QVariantList plans READ plans NOTIFY changed)
    Q_PROPERTY(QVariantList sessions READ sessions NOTIFY changed)
    Q_PROPERTY(QVariantList trace READ trace NOTIFY changed)
    Q_PROPERTY(QVariantList operatorRecords READ operatorRecords NOTIFY changed)
    Q_PROPERTY(QVariantMap selectedEvaluation READ selectedEvaluation NOTIFY changed)
    Q_PROPERTY(QVariantMap baselineEvaluation READ baselineEvaluation NOTIFY changed)
    Q_PROPERTY(QVariantMap selectedValidation READ selectedValidation NOTIFY changed)
    Q_PROPERTY(QString selectedPlanJson READ selectedPlanJson NOTIFY changed)
    Q_PROPERTY(QVariantMap searchDetails READ searchDetails NOTIFY changed)
    Q_PROPERTY(QVariantMap evidenceContext READ evidenceContext NOTIFY changed)
    Q_PROPERTY(QVariantMap detectionDetails READ detectionDetails NOTIFY changed)
    Q_PROPERTY(QVariantMap lastOperation READ lastOperation NOTIFY changed)
public:
    EogAnalysisController(EogDatasetController*, EogAnalysisClient*, EogHistoryRepository*,
                          agent::ModelTransport*, QObject* parent = nullptr);
    ~EogAnalysisController() override;
    bool busy() const;
    bool providerConfigured() const { return client_->configured(); }
    bool providerVerified() const { return client_->executionVerified(); }
    bool historical() const { return historical_; }
    bool canReview() const;
    bool comparisonCompatible() const;
    QString status() const { return status_; }
    QString storageStatus() const { return storage_status_; }
    QString sessionId() const { return session_id_; }
    QString summary() const { return summary_; }
    QString selectedPlanId() const { return selected_; }
    QString baselinePlanId() const { return baseline_; }
    QVariantList events() const;
    QVariantList plans() const;
    QVariantList sessions() const { return sessions_; }
    QVariantList trace() const { return trace_; }
    QVariantList operatorRecords() const { return operator_records_; }
    QVariantMap selectedEvaluation() const { return evaluations_.value(selected_).toVariantMap(); }
    QVariantMap baselineEvaluation() const { return evaluations_.value(baseline_).toVariantMap(); }
    QVariantMap selectedValidation() const;
    QString selectedPlanJson() const;
    QVariantMap searchDetails() const { return search_.toVariantMap(); }
    QVariantMap evidenceContext() const { return (capture_ ? capture_->manifest() : historical_manifest_).toVariantMap(); }
    QVariantMap detectionDetails() const { return detection_.toVariantMap(); }
    QVariantMap lastOperation() const { return last_operation_.toVariantMap(); }
    Q_INVOKABLE void detect();
    Q_INVOKABLE void searchPlans();
    Q_INVOKABLE void evaluatePlan(const QString& json);
    Q_INVOKABLE QString newPlanDraftJson() const;
    Q_INVOKABLE void evaluateNoAction();
    Q_INVOKABLE void validateSelected(const QString& evaluatorMethod);
    Q_INVOKABLE void investigate(const QString& objective);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void finishSession();
    Q_INVOKABLE void selectPlan(const QString& id);
    Q_INVOKABLE void selectBaseline(const QString& id);
    Q_INVOKABLE void recordReview(const QString& note = {});
    Q_INVOKABLE void acknowledge(const QString& eventId, const QString& note = {});
    Q_INVOKABLE QVariantMap eventEvidence(const QString& eventId) const;
    Q_INVOKABLE void refreshHistory(int offset = 0);
    Q_INVOKABLE void openHistory(const QString& id);
    Q_INVOKABLE void returnToLive();
    Q_INVOKABLE void exportHistory(const QString& id, const QUrl& destination);
    Q_INVOKABLE void importHistory(const QUrl& source);
signals:
    void changed();
private:
    EogDatasetController* dataset_;
    EogAnalysisClient* client_;
    EogHistoryRepository* history_;
    QPointer<agent::ModelTransport> transport_;
    EogInvestigator investigator_;
    std::shared_ptr<const Dataset> source_, observed_source_;
    std::shared_ptr<const Capture> capture_;
    std::shared_ptr<std::atomic_bool> cancellation_;
    quint64 generation_ = 0;
    quint64 import_generation_ = 0;
    qint64 cutoff_ = 0, observed_cutoff_ = 0;
    bool preparing_ = false, historical_ = false, sealed_ = false, observed_loading_ = false, retiring_ = false, import_pending_ = false;
    bool review_disabled_ = false;
    int request_count_ = 0;
    QString session_id_, status_ = "Run analysis explicitly. Loading or selecting data makes no model request.";
    QString storage_status_ = "Opening EOG history...", summary_, selected_, baseline_, pending_execution_, pending_name_, pending_history_id_;
    QJsonObject pending_request_, search_, historical_manifest_, detection_, last_operation_;
    QMap<QString, QJsonObject> responses_, evaluations_, validations_, event_rows_;
    QMap<QString, QString> validation_requests_, event_requests_;
    QVariantList sessions_, trace_, operator_records_;
    void datasetChanged();
    void retire(const QString& reason, bool clear);
    void prepare(const QString& objective, std::function<void()> continuation);
    void runOperation(const QString& operation, const QJsonObject& arguments, const QString& execution = {});
    void acceptResponse(const QJsonObject&, const QJsonObject&, bool retain = true);
    void toolRequested(const QString& execution, const QString& name, const QJsonObject& arguments);
    QJsonObject readTool(const QString& name, const QJsonObject& arguments) const;
    QJsonObject request(const QString& operation, const QJsonObject& arguments) const;
    QJsonObject bindPlanProposal(QJsonObject proposal) const;
    QJsonObject validationFor(const QString& plan) const;
    QJsonArray toolDefinitions() const;
    void completeTool(const QJsonObject& outcome);
    void recordEvent(const QJsonObject& event);
    void finalResult(const QJsonObject& result);
    void clearEvidence();
};
} // namespace breakwater::desktop::eog

#pragma once
#include "PipeParameters.hpp"
#include "ProjectController.hpp"
#include "history/HistoryController.hpp"
#include <QPointer>
#include <QSet>
#include <atomic>
#include <memory>

namespace breakwater::desktop::quality {

class PipeTransportController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString routeId READ routeId NOTIFY changed)
    Q_PROPERTY(QVariantList runs READ runs NOTIFY changed)
    Q_PROPERTY(QString selectedRunId READ selectedRunId NOTIFY changed)
    Q_PROPERTY(QVariantMap parameters READ parameters NOTIFY changed)
    Q_PROPERTY(QVariantMap result READ result NOTIFY changed)
    Q_PROPERTY(bool viewingImportedEvidence READ viewingImportedEvidence NOTIFY changed)
    Q_PROPERTY(bool viewingRetainedEvidence READ viewingRetainedEvidence NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool canRun READ canRun NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString historyStatus READ historyStatus NOTIFY changed)
    Q_PROPERTY(bool calibrationBusy READ calibrationBusy NOTIFY changed)
    Q_PROPERTY(QString calibrationStatus READ calibrationStatus NOTIFY changed)
    Q_PROPERTY(QVariantMap calibrationResult READ calibrationResult NOTIFY changed)
    Q_PROPERTY(bool canApplyCalibration READ canApplyCalibration NOTIFY changed)
public:
    explicit PipeTransportController(ProjectController* project, history::HistoryController* history = nullptr, QObject* parent = nullptr);
    ~PipeTransportController() override;
    QString routeId() const { return route_; }
    QVariantList runs() const { return runs_; }
    QString selectedRunId() const { return selected_run_; }
    QVariantMap parameters() const;
    QVariantMap result() const { return viewing_imported_ ? imported_result_ : result_; }
    bool viewingImportedEvidence() const { return viewing_imported_; }
    bool viewingRetainedEvidence() const { return viewing_retained_; }
    bool busy() const { return busy_; }
    bool canRun() const;
    QString status() const { return status_; }
    QString historyStatus() const { return history_status_; }
    void setHistoryRepository(history::SessionRepository* repository);
    bool calibrationBusy() const { return calibration_busy_; }
    QString calibrationStatus() const { return calibration_status_; }
    QVariantMap calibrationResult() const { return calibration_result_; }
    bool canApplyCalibration() const;
    Q_INVOKABLE void calibrateDecay(const QString& initial, const QString& observations);
    Q_INVOKABLE void discardCalibration();
    Q_INVOKABLE void cancelCalibration();
    Q_INVOKABLE void applyCalibration();
    Q_INVOKABLE void selectRun(const QString& id);
    Q_INVOKABLE void setParameter(const QString& field, const QString& value);
    Q_INVOKABLE void run();
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void importResult(const QUrl& source);
    Q_INVOKABLE bool showRetainedResult(const QVariantMap& result);
    Q_INVOKABLE void showCurrentResult();
    Q_INVOKABLE bool exportResult(const QUrl& destination);
signals:
    void changed();
private:
    QPointer<ProjectController> project_;
    QPointer<history::HistoryController> history_;
    QPointer<history::SessionRepository> repository_;
    QSet<QString> pending_history_;
    QString history_status_="Automatic pipe history is not configured.";
    QString route_, selected_run_, binding_, status_ = "Select a pipe and capture a volume forecast.";
    QString retired_status_;
    QVariantMap result_, imported_result_;
    QVariantList runs_;
    QMap<QString, QJsonObject> contexts_;
    quint64 generation_ = 0;
    quint64 cancelled_generation_=0;
    quint64 import_generation_ = 0;
    bool busy_ = false;
    bool importing_ = false, viewing_imported_ = false;
    bool viewing_retained_=false;
    bool calibration_busy_ = false;
    QString calibration_status_ = "Estimate first-order decay from a closed tracer test.";
    QString calibration_binding_;
    QVariantMap calibration_result_;
    std::shared_ptr<std::atomic_bool> calibration_cancel_;
    QString calibrationBinding() const;
    QJsonObject context(const QString& runId) const;
    QString currentBinding() const;
    void refresh();
    void invalidate(const QString& message);
};

} // namespace breakwater::desktop::quality

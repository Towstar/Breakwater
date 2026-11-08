#pragma once
#include "PipeParameters.hpp"
#include "ProjectController.hpp"
#include "history/HistoryController.hpp"
#include <QPointer>

namespace breakwater::desktop::quality {

class PipeTransportController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString routeId READ routeId NOTIFY changed)
    Q_PROPERTY(QVariantList runs READ runs NOTIFY changed)
    Q_PROPERTY(QString selectedRunId READ selectedRunId NOTIFY changed)
    Q_PROPERTY(QVariantMap parameters READ parameters NOTIFY changed)
    Q_PROPERTY(QVariantMap result READ result NOTIFY changed)
    Q_PROPERTY(bool viewingImportedEvidence READ viewingImportedEvidence NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool canRun READ canRun NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
public:
    explicit PipeTransportController(ProjectController* project, history::HistoryController* history = nullptr, QObject* parent = nullptr);
    QString routeId() const { return route_; }
    QVariantList runs() const { return runs_; }
    QString selectedRunId() const { return selected_run_; }
    QVariantMap parameters() const;
    QVariantMap result() const { return viewing_imported_ ? imported_result_ : result_; }
    bool viewingImportedEvidence() const { return viewing_imported_; }
    bool busy() const { return busy_; }
    bool canRun() const;
    QString status() const { return status_; }
    Q_INVOKABLE void selectRun(const QString& id);
    Q_INVOKABLE void setParameter(const QString& field, const QString& value);
    Q_INVOKABLE void run();
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void importResult(const QUrl& source);
    Q_INVOKABLE void showCurrentResult();
    Q_INVOKABLE bool exportResult(const QUrl& destination);
signals:
    void changed();
private:
    QPointer<ProjectController> project_;
    QPointer<history::HistoryController> history_;
    QString route_, selected_run_, binding_, status_ = "Select a pipe and capture a volume forecast.";
    QString retired_status_;
    QVariantMap result_, imported_result_;
    QVariantList runs_;
    QMap<QString, QJsonObject> contexts_;
    quint64 generation_ = 0;
    quint64 import_generation_ = 0;
    bool busy_ = false;
    bool importing_ = false, viewing_imported_ = false;
    QJsonObject context(const QString& runId) const;
    QString currentBinding() const;
    void refresh();
    void invalidate(const QString& message);
};

} // namespace breakwater::desktop::quality

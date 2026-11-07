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
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool canRun READ canRun NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
public:
    explicit PipeTransportController(ProjectController* project, history::HistoryController* history = nullptr, QObject* parent = nullptr);
    QString routeId() const { return route_; }
    QVariantList runs() const { return runs_; }
    QString selectedRunId() const { return selected_run_; }
    QVariantMap parameters() const;
    QVariantMap result() const { return result_; }
    bool busy() const { return busy_; }
    bool canRun() const;
    QString status() const { return status_; }
    Q_INVOKABLE void selectRun(const QString& id);
    Q_INVOKABLE void setParameter(const QString& field, const QString& value);
    Q_INVOKABLE void run();
    Q_INVOKABLE void cancel();
    Q_INVOKABLE bool exportResult(const QUrl& destination);
signals:
    void changed();
private:
    QPointer<ProjectController> project_;
    QPointer<history::HistoryController> history_;
    QString route_, selected_run_, binding_, status_ = "Select a pipe and capture a volume forecast.";
    QString retired_status_;
    QVariantMap result_;
    QVariantList runs_;
    QMap<QString, QJsonObject> contexts_;
    quint64 generation_ = 0;
    bool busy_ = false;
    QJsonObject context(const QString& runId) const;
    QString currentBinding() const;
    void refresh();
    void invalidate(const QString& message);
};

} // namespace breakwater::desktop::quality

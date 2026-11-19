#pragma once

#include "ProjectController.hpp"
#include "PythonWorkerClient.hpp"
#include "history/SessionRepository.hpp"
#include "history/HistoryController.hpp"
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QPointer>
#include <QSet>
#include <QStringList>
#include <vector>

namespace breakwater::desktop::science {

class StorageScienceController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool canRun READ canRun NOTIFY changed)
    Q_PROPERTY(QString sourceMode READ sourceMode NOTIFY changed)
    Q_PROPERTY(QVariantMap sourceIdentity READ sourceIdentity NOTIFY changed)
    Q_PROPERTY(QVariantList controls READ controls NOTIFY changed)
    Q_PROPERTY(QStringList selectedControlIds READ selectedControlIds NOTIFY changed)
    Q_PROPERTY(bool canRunSensitivities READ canRunSensitivities NOTIFY changed)
    Q_PROPERTY(QVariantMap result READ result NOTIFY changed)
    Q_PROPERTY(QVariantList tanks READ tanks NOTIFY changed)
    Q_PROPERTY(QString selectedTankId READ selectedTankId NOTIFY changed)
    Q_PROPERTY(QVariantMap chart READ chart NOTIFY changed)
    Q_PROPERTY(QVariantList sensitivityRows READ sensitivityRows NOTIFY changed)
    Q_PROPERTY(QString historyStatus READ historyStatus NOTIFY historyChanged)
    Q_PROPERTY(bool historyPending READ historyPending NOTIFY historyChanged)
    Q_PROPERTY(bool historyFailed READ historyFailed NOTIFY historyChanged)
public:
    explicit StorageScienceController(ProjectController* project, PythonWorkerClient* worker, QObject* parent = nullptr);
    ~StorageScienceController() override;
    QString status() const;
    bool busy() const { return waiting_ || comparing_ || source_loading_; }
    QString sourceMode() const { return source_mode_; }
    QVariantMap sourceIdentity() const { return stored_origin_.toVariantMap(); }
    Q_INVOKABLE void setSourceMode(const QString& mode);
    void setHistoryController(history::HistoryController* controller);
    bool canRun() const;
    QVariantList controls() const;
    QStringList selectedControlIds() const;
    bool canRunSensitivities() const;
    QVariantMap result() const { return result_.toVariantMap(); }
    QVariantList tanks() const { return result_.value("tanks").toArray().toVariantList(); }
    QString selectedTankId() const { return tank_; }
    QVariantMap chart() const;
    QVariantList sensitivityRows() const;
    QString historyStatus() const { return history_status_; }
    bool historyPending() const { return !pending_history_ids_.isEmpty(); }
    bool historyFailed() const { return history_failed_; }
    QString historySavedResultId() const { return history_saved_result_id_; }
    void setHistoryRepository(history::SessionRepository* repository);
    Q_INVOKABLE void run();
    Q_INVOKABLE bool selectControl(const QString& id, bool selected);
    Q_INVOKABLE void runSensitivities();
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void selectTank(const QString& id);
signals:
    void changed();
    void historyChanged();
    void completed(const QJsonObject& evidence);
    void failed(const QString& code, const QString& message);
private:
    QPointer<ProjectController> project_;
    QPointer<PythonWorkerClient> worker_;
    QPointer<history::SessionRepository> repository_;
    QPointer<history::HistoryController> history_controller_;
    QString source_mode_="forecast",stored_binding_;
    std::shared_ptr<const CapturedForecast> stored_capture_;
    QJsonObject stored_origin_,job_origin_,pending_source_archive_;
    QString pending_source_run_;
    quint64 source_generation_=0;
    bool source_loading_=false;
    QSet<QString> pending_history_ids_;
    QString history_status_ = "Scientific history is not configured.";
    QString history_saved_result_id_;
    bool history_failed_ = false;
    std::shared_ptr<const CapturedForecast> captured_;
    std::shared_ptr<const CapturedForecast> catalog_capture_;
    std::vector<ControlDescriptor> catalog_;
    QMap<QString, QString> ineligible_reasons_;
    QSet<QString> selected_controls_;
    QJsonArray job_controls_;
    QJsonObject result_;
    QString tank_, status_ = "Run a native forecast before comparing it with PyTorch.";
    quint64 generation_ = 0;
    bool waiting_ = false, comparing_ = false, sensitivities_ = false;
    void refresh();
    std::shared_ptr<const CapturedForecast> currentCapture() const;
    void refreshStoredSource();
    void captureStoredSource(QJsonObject archive,QString run,quint64 generation);
    void rebuildCatalog(const std::shared_ptr<const CapturedForecast>& capture);
    QJsonArray selectedControls() const;
    void begin(bool sensitivities);
    void retire(const QString& message);
    void accept(const QJsonObject& response);
};

} // namespace breakwater::desktop::science

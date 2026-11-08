#pragma once

#include "ProjectController.hpp"
#include "PythonWorkerClient.hpp"
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
    Q_PROPERTY(QVariantList controls READ controls NOTIFY changed)
    Q_PROPERTY(QStringList selectedControlIds READ selectedControlIds NOTIFY changed)
    Q_PROPERTY(bool canRunSensitivities READ canRunSensitivities NOTIFY changed)
    Q_PROPERTY(QVariantMap result READ result NOTIFY changed)
    Q_PROPERTY(QVariantList tanks READ tanks NOTIFY changed)
    Q_PROPERTY(QString selectedTankId READ selectedTankId NOTIFY changed)
    Q_PROPERTY(QVariantMap chart READ chart NOTIFY changed)
    Q_PROPERTY(QVariantList sensitivityRows READ sensitivityRows NOTIFY changed)
public:
    explicit StorageScienceController(ProjectController* project, PythonWorkerClient* worker, QObject* parent = nullptr);
    ~StorageScienceController() override;
    QString status() const;
    bool busy() const { return waiting_ || comparing_; }
    bool canRun() const;
    QVariantList controls() const;
    QStringList selectedControlIds() const;
    bool canRunSensitivities() const;
    QVariantMap result() const { return result_.toVariantMap(); }
    QVariantList tanks() const { return result_.value("tanks").toArray().toVariantList(); }
    QString selectedTankId() const { return tank_; }
    QVariantMap chart() const;
    QVariantList sensitivityRows() const;
    Q_INVOKABLE void run();
    Q_INVOKABLE bool selectControl(const QString& id, bool selected);
    Q_INVOKABLE void runSensitivities();
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void selectTank(const QString& id);
signals:
    void changed();
    void completed(const QJsonObject& evidence);
    void failed(const QString& code, const QString& message);
private:
    QPointer<ProjectController> project_;
    QPointer<PythonWorkerClient> worker_;
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
    void rebuildCatalog(const std::shared_ptr<const CapturedForecast>& capture);
    QJsonArray selectedControls() const;
    void begin(bool sensitivities);
    void retire(const QString& message);
    void accept(const QJsonObject& response);
};

} // namespace breakwater::desktop::science

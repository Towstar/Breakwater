#pragma once

#include "ProjectController.hpp"
#include "PythonWorkerClient.hpp"
#include <QJsonArray>
#include <QJsonObject>
#include <QPointer>

namespace breakwater::desktop::science {

class StorageScienceController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool canRun READ canRun NOTIFY changed)
    Q_PROPERTY(QVariantMap result READ result NOTIFY changed)
    Q_PROPERTY(QVariantList tanks READ tanks NOTIFY changed)
    Q_PROPERTY(QString selectedTankId READ selectedTankId NOTIFY changed)
    Q_PROPERTY(QVariantMap chart READ chart NOTIFY changed)
public:
    explicit StorageScienceController(ProjectController* project, PythonWorkerClient* worker, QObject* parent = nullptr);
    ~StorageScienceController() override;
    QString status() const;
    bool busy() const { return waiting_ || comparing_; }
    bool canRun() const;
    QVariantMap result() const { return result_.toVariantMap(); }
    QVariantList tanks() const { return result_.value("tanks").toArray().toVariantList(); }
    QString selectedTankId() const { return tank_; }
    QVariantMap chart() const;
    Q_INVOKABLE void run();
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
    QJsonObject result_;
    QString tank_, status_ = "Run a native forecast before comparing it with PyTorch.";
    quint64 generation_ = 0;
    bool waiting_ = false, comparing_ = false;
    void refresh();
    void retire(const QString& message);
    void accept(const QJsonObject& response);
};

} // namespace breakwater::desktop::science

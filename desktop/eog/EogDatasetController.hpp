#pragma once
#include "EogDataset.hpp"
#include <QObject>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

namespace breakwater::desktop::eog {
// A read-only dataset view. No answer key, forecast, detector or control authority.
class EogDatasetController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool loaded READ loaded NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString sourceFolder READ sourceFolder NOTIFY changed)
    Q_PROPERTY(QString datasetIdentity READ datasetIdentity NOTIFY changed)
    Q_PROPERTY(double firstUnix READ firstUnix NOTIFY changed)
    Q_PROPERTY(double nowUnix READ nowUnix NOTIFY changed)
    Q_PROPERTY(double cursorUnix READ cursorUnix NOTIFY changed)
    Q_PROPERTY(QVariantList systems READ systems NOTIFY changed)
    Q_PROPERTY(QVariantList facilities READ facilities NOTIFY changed)
    Q_PROPERTY(QVariantList assets READ assets NOTIFY changed)
    Q_PROPERTY(QVariantList nodes READ assets NOTIFY changed)
    Q_PROPERTY(QVariantList routes READ routes NOTIFY changed)
    Q_PROPERTY(QString sceneMode READ sceneMode NOTIFY changed)
    Q_PROPERTY(QVariantList sceneNodes READ sceneNodes NOTIFY changed)
    Q_PROPERTY(QVariantList sceneRoutes READ sceneRoutes NOTIFY changed)
    Q_PROPERTY(QVariantMap sceneBounds READ sceneBounds NOTIFY changed)
    Q_PROPERTY(QVariantList metrics READ metrics NOTIFY changed)
    Q_PROPERTY(QVariantList selectedSeries READ selectedSeries NOTIFY changed)
    Q_PROPERTY(QVariantList selectedThresholds READ selectedThresholds NOTIFY changed)
    Q_PROPERTY(QVariantMap selectedValues READ selectedValues NOTIFY changed)
    Q_PROPERTY(QVariantMap opsSummary READ opsSummary NOTIFY changed)
    Q_PROPERTY(QStringList warnings READ warnings NOTIFY changed)
    Q_PROPERTY(QString systemId READ systemId NOTIFY changed)
    Q_PROPERTY(QString facilityId READ facilityId NOTIFY changed)
    Q_PROPERTY(QString assetId READ assetId NOTIFY changed)
    Q_PROPERTY(QString metric READ metric NOTIFY changed)
    Q_PROPERTY(QString seriesId READ seriesId NOTIFY changed)
    Q_PROPERTY(QString unit READ unit NOTIFY changed)
public:
    explicit EogDatasetController(QObject* parent = nullptr);
    ~EogDatasetController() override;
    bool busy() const { return busy_; }
    bool loaded() const { return bool(dataset_); }
    QString error() const { return error_; }
    QString status() const { return status_; }
    QString sourceFolder() const { return dataset_ ? dataset_->folder : QString{}; }
    QString datasetIdentity() const { return dataset_ ? dataset_->identity : QString{}; }
    double firstUnix() const { return dataset_ ? double(dataset_->first_unix) : 0; }
    double nowUnix() const { return dataset_ ? double(dataset_->now_unix) : 0; }
    double cursorUnix() const { return double(cursor_); }
    QVariantList systems() const;
    QVariantList facilities() const;
    QVariantList assets() const;
    QVariantList routes() const;
    QString sceneMode() const { return scene_mode_; }
    QVariantList sceneNodes() const;
    QVariantList sceneRoutes() const;
    QVariantMap sceneBounds() const;
    QVariantList metrics() const;
    QVariantList selectedSeries() const;
    QVariantList selectedThresholds() const;
    QVariantMap selectedValues() const;
    QVariantMap opsSummary() const;
    QStringList warnings() const { return dataset_ ? dataset_->warnings : QStringList{}; }
    QString systemId() const { return system_id_; }
    QString facilityId() const { return facility_id_; }
    QString assetId() const { return asset_id_; }
    QString metric() const { return metric_; }
    QString seriesId() const { return series_key(asset_id_, metric_); }
    QString unit() const;
    std::shared_ptr<const Dataset> capturedDataset() const { return dataset_; }
    Q_INVOKABLE void loadFolder(const QUrl& folder);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void selectSystem(const QString& id);
    Q_INVOKABLE void selectFacility(const QString& id);
    Q_INVOKABLE void selectAsset(const QString& id);
    Q_INVOKABLE void selectMetric(const QString& metric);
    Q_INVOKABLE void setCursorUnix(double timestamp);
    Q_INVOKABLE void showOverview();
    Q_INVOKABLE void showFacility(const QString& id);
    Q_INVOKABLE void pickSceneNode(const QString& id);
signals:
    void cancelled();
    void changed();
    void completed();
    void failed(const QString& message);
private:
    std::shared_ptr<const Dataset> dataset_;
    std::shared_ptr<std::atomic_bool> cancellation_;
    quint64 generation_ = 0;
    bool busy_ = false;
    QString error_, status_ = "Choose an EOG dataset folder (three input files only).";
    QString system_id_, facility_id_, asset_id_, metric_;
    QString scene_mode_ = "overview";
    qint64 cursor_ = 0;
    QVariantMap observation(const QString& asset, const QString& metric) const;
    QVariantList overviewNodes() const;
    QVariantList overviewRoutes() const;
};
} // namespace breakwater::desktop::eog

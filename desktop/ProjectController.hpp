#pragma once
#include "ProjectDocument.hpp"
#include <QObject>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <functional>
#include <optional>

namespace breakwater::desktop {
class ProjectController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList assets READ assets NOTIFY changed)
    Q_PROPERTY(QVariantList routes READ routes NOTIFY changed)
    Q_PROPERTY(QVariantMap selected READ selected NOTIFY changed)
    Q_PROPERTY(QString selectedId READ selectedId NOTIFY changed)
    Q_PROPERTY(QStringList draftIssues READ draftIssues NOTIFY changed)
    Q_PROPERTY(QString summary READ summary NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY changed)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY changed)
    Q_PROPERTY(bool canApprove READ canApprove NOTIFY changed)
    Q_PROPERTY(QString runId READ runId NOTIFY changed)
    Q_PROPERTY(QString networkHash READ networkHash NOTIFY changed)
    Q_PROPERTY(double forecastFraction READ forecastFraction WRITE setForecastFraction NOTIFY changed)
    Q_PROPERTY(double forecastTimeH READ forecastTimeH NOTIFY changed)
    Q_PROPERTY(QVariantMap camera READ camera NOTIFY cameraChanged)
public:
    explicit ProjectController(QObject* parent = nullptr);
    QVariantList assets() const;
    QVariantList routes() const;
    QVariantMap selected() const;
    QString selectedId() const { return selected_id_; }
    QStringList draftIssues() const;
    QString summary() const { return summary_; }
    bool busy() const { return busy_; }
    bool canUndo() const { return !undo_.empty(); }
    bool canRedo() const { return !redo_.empty(); }
    bool canApprove() const;
    QString runId() const;
    QString networkHash() const { return physics_hash(project_); }
    quint64 simulationRevision() const { return revision_; }
    double forecastFraction() const { return forecast_fraction_; }
    double forecastTimeH() const;
    QVariantMap camera() const;
    void setForecastFraction(double value);
    QJsonObject document() const { return project_json(project_); }
    bool restore(const QJsonObject& object);
    // Called only by the human-facing approval controller after stored validation.
    bool applyControlsForApproval(const std::vector<ControlValue>& controls, const QString& expectedHash);

    Q_INVOKABLE void select(const QString& id);
    Q_INVOKABLE void addAsset(const QString& kind, double x = 0.0, double z = 0.0);
    Q_INVOKABLE void moveAsset(const QString& id, double x, double z);
    Q_INVOKABLE void connectAssets(const QString& source, const QString& target);
    Q_INVOKABLE void attachPump(const QString& route);
    Q_INVOKABLE void editSelected(const QString& field, const QString& value);
    Q_INVOKABLE void deleteSelected();
    Q_INVOKABLE void failSelectedPump();
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    Q_INVOKABLE void reset();
    Q_INVOKABLE void loadExample(const QString& name);
    Q_INVOKABLE bool save(const QUrl& url);
    Q_INVOKABLE bool load(const QUrl& url);
    Q_INVOKABLE void forecast();
    Q_INVOKABLE void approve();
    Q_INVOKABLE void setCamera(double yaw, double pitch, double distance, double x, double z);
signals:
    void changed();
    void cameraChanged();
private:
    struct StoredRun { SimulationResult result; ValidationReport validation; QString id; quint64 revision; };
    ProjectDocument project_;
    QString selected_id_;
    QString summary_ = "Synthetic constant-flow model. Healthy preview accumulates 8 m3/h in Tank A.";
    std::vector<QJsonObject> undo_, redo_;
    quint64 revision_ = 0, job_generation_ = 0;
    bool busy_ = false, approved_ = false;
    double forecast_fraction_ = 0.0;
    std::optional<StoredRun> run_;
    void edit(const std::function<void()>& mutation);
    void refresh(bool physics_changed);
    std::size_t sampleIndex() const;
};
}

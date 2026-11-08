#pragma once
#include "ProjectDocument.hpp"
#include <QObject>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <functional>
#include <memory>

namespace breakwater::desktop {
// Shared immutable numerical evidence. QML receives bounded display previews;
// scientific jobs retain this handle instead of re-reading mutable editor state.
struct CapturedForecast {
    Network network;
    SimulationConfig config;
    SimulationResult result;
    ValidationReport validation;
    QString id, physical_hash;
    quint64 revision = 0;
};

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
    Q_PROPERTY(double forecastHorizonH READ forecastHorizonH NOTIFY changed)
    Q_PROPERTY(QVariantMap actionFeedback READ actionFeedback NOTIFY actionFeedbackChanged)
    Q_PROPERTY(QVariantMap camera READ camera NOTIFY cameraChanged)
    Q_PROPERTY(QVariantList sensorMarkers READ sensorMarkers NOTIFY changed)
    Q_PROPERTY(QVariantMap selectedSensor READ selectedSensor NOTIFY changed)
    Q_PROPERTY(QString selectedSensorId READ selectedSensorId NOTIFY changed)
    Q_PROPERTY(QString sensorConfigurationHash READ sensorConfigurationHash NOTIFY sensorConfigurationChanged)
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
    double forecastHorizonH() const { return run_ ? run_->config.horizon_h : 0.0; }
    QVariantMap actionFeedback() const { return action_feedback_; }
    QVariantMap camera() const;
    void setForecastFraction(double value);
    QJsonObject document() const { return project_json(project_); }
    QJsonObject editorDocument() const;
    QJsonObject sensorConfiguration() const;
    QJsonObject sensorContext() const;
    QString sensorConfigurationHash() const;
    QVariantList sensorMarkers() const;
    QVariantMap selectedSensor() const;
    QString selectedSensorId() const { return selected_sensor_id_; }
    QJsonObject pipeParameters(const QString& routeId) const;
    bool setPipeParameters(const QString& routeId, const QJsonObject& parameters);
    QJsonObject capturedRun() const;
    std::shared_ptr<const CapturedForecast> capturedForecast() const;
    // C++ integration/test entry point; the normal UI retains its fixed defaults.
    void forecastWithConfig(SimulationConfig configuration);
    bool restore(const QJsonObject& object);
    // Called only by the human-facing approval controller after stored validation.
    bool applyControlsForApproval(const std::vector<ControlValue>& controls, const QString& expectedHash);

    Q_INVOKABLE void select(const QString& id);
    Q_INVOKABLE void selectSensor(const QString& id);
    Q_INVOKABLE bool addSensor(const QString& quantity, const QString& attachmentId, const QVariantMap& fields = {});
    Q_INVOKABLE bool configureSensor(const QString& id, const QVariantMap& fields);
    Q_INVOKABLE bool deleteSensor(const QString& id);
    Q_INVOKABLE bool moveSensor(const QString& id, double x, double y, double z);
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
    void sensorConfigurationChanged();
    void actionFeedbackChanged();
private:
    ProjectDocument project_;
    QString selected_id_;
    QString selected_sensor_id_;
    QVariantMap action_feedback_;
    quint64 action_feedback_sequence_ = 0;
    QJsonObject sensor_configuration_, sensor_layout_;
    QString summary_ = "Synthetic constant-flow model. Healthy preview accumulates 8 m3/h in Tank A.";
    std::vector<QJsonObject> undo_, redo_;
    quint64 revision_ = 0, job_generation_ = 0;
    bool busy_ = false, approved_ = false;
    double forecast_fraction_ = 0.0;
    std::shared_ptr<const CapturedForecast> run_;
    bool edit(const std::function<void()>& mutation);
    void reportAction(const QString& code, const QString& title, const QString& message,
                      const QStringList& assetIds = {}, const QString& field = {}, const QString& severity = "warning");
    void clearActionFeedback();
    void synchronizeSensorConfiguration();
    void refresh(bool physics_changed);
    std::size_t sampleIndex() const;
};
}

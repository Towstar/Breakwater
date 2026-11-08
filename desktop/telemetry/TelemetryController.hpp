#pragma once

#include <QObject>
#include <QJsonObject>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

namespace breakwater::desktop::telemetry {
class TelemetryIoWorker;

// Acquisition has its own worker-thread lifecycle. These views contain evidence;
// this object never writes project controls or constructs actionable forecasts.
class TelemetryController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString mode READ mode NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QString configurationHash READ configurationHash NOTIFY changed)
    Q_PROPERTY(bool active READ active NOTIFY changed)
    Q_PROPERTY(bool paused READ paused NOTIFY changed)
    Q_PROPERTY(bool publisherConfigured READ publisherConfigured CONSTANT)
    Q_PROPERTY(double eventTime READ eventTime NOTIFY changed)
    Q_PROPERTY(double rate READ rate NOTIFY changed)
    Q_PROPERTY(QVariantList sensors READ sensors NOTIFY changed)
    Q_PROPERTY(QVariantList tracePreview READ tracePreview NOTIFY changed)
    Q_PROPERTY(QVariantMap counters READ counters NOTIFY changed)
    Q_PROPERTY(QVariantMap snapshot READ snapshot NOTIFY changed)
public:
    explicit TelemetryController(QObject* parent = nullptr);
    ~TelemetryController() override;
    QString mode() const { return snapshot_.value("mode", "manual").toString(); }
    QString status() const { return snapshot_.value("status", "unconfigured").toString(); }
    QString error() const { return snapshot_.value("error").toString(); }
    QString configurationHash() const { return snapshot_.value("configuration_hash").toString(); }
    bool active() const { return snapshot_.value("active").toBool(); }
    bool paused() const { return snapshot_.value("paused").toBool(); }
    bool publisherConfigured() const { return publisher_configured_; }
    double eventTime() const { return snapshot_.value("event_time_s").toDouble(); }
    double rate() const { return snapshot_.value("clock_rate", 1.0).toDouble(); }
    QVariantList sensors() const { return snapshot_.value("sensors").toList(); }
    QVariantList tracePreview() const { return trace_preview_; }
    QVariantMap counters() const { return snapshot_.value("counters").toMap(); }
    QVariantMap snapshot() const { return snapshot_; }

    void configure(const QJsonObject& configuration, const QJsonObject& context);
    void startProcess(const QString& executable, const QStringList& arguments = {});
    void setTimeoutMs(int milliseconds);
    Q_INVOKABLE void startConfiguredPublisher();
    Q_INVOKABLE void manual();
    Q_INVOKABLE void startReplay(const QUrl& recording);
    Q_INVOKABLE void showHistorical(const QUrl& recording);
    Q_INVOKABLE void pause();
    Q_INVOKABLE void resume();
    Q_INVOKABLE void setRate(double rate);
    Q_INVOKABLE void restart();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void exportRecording(const QUrl& destination);
signals:
    void changed();
    void failed(const QString& code, const QString& message);
    void recordingExported(const QUrl& destination);
private:
    QThread thread_;
    TelemetryIoWorker* worker_ = nullptr;
    QTimer poll_;
    QVariantMap snapshot_{{"mode", "manual"}, {"status", "unconfigured"}, {"active", false}};
    QVariantList trace_preview_;
    QString publisher_executable_;
    QStringList publisher_arguments_;
    bool publisher_configured_ = false, poll_pending_ = false;
};
} // namespace breakwater::desktop::telemetry

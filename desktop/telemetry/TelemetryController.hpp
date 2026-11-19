#pragma once

#include <QObject>
#include <QJsonArray>
#include <QJsonObject>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <atomic>
#include <memory>

namespace breakwater::desktop::telemetry {
class TelemetryIoWorker;
struct ObservedEvidence;

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
    Q_PROPERTY(bool bundledPublisher READ bundledPublisher CONSTANT)
    Q_PROPERTY(QStringList syntheticPresets READ syntheticPresets CONSTANT)
    Q_PROPERTY(double eventTime READ eventTime NOTIFY changed)
    Q_PROPERTY(double rate READ rate NOTIFY changed)
    Q_PROPERTY(QVariantList sensors READ sensors NOTIFY changed)
    Q_PROPERTY(QVariantList tracePreview READ tracePreview NOTIFY changed)
    Q_PROPERTY(QVariantMap counters READ counters NOTIFY changed)
    Q_PROPERTY(QVariantMap snapshot READ snapshot NOTIFY changed)
    Q_PROPERTY(QVariantMap observedCapture READ observedCapture NOTIFY observedCaptureChanged)
    Q_PROPERTY(QVariantMap observedCaptureDiagnostics READ observedCaptureDiagnostics NOTIFY observedCaptureChanged)
    Q_PROPERTY(QString observedCaptureDisposition READ observedCaptureDisposition NOTIFY observedCaptureChanged)
    Q_PROPERTY(bool observedCaptureBusy READ observedCaptureBusy NOTIFY observedCaptureChanged)
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
    bool bundledPublisher() const { return bundled_publisher_; }
    QStringList syntheticPresets() const;
    double eventTime() const { return snapshot_.value("event_time_s").toDouble(); }
    double rate() const { return snapshot_.value("clock_rate", 1.0).toDouble(); }
    QVariantList sensors() const { return snapshot_.value("sensors").toList(); }
    QVariantList tracePreview() const { return trace_preview_; }
    QVariantMap counters() const { return snapshot_.value("counters").toMap(); }
    QVariantMap snapshot() const { return snapshot_; }
    QVariantMap observedCapture() const;
    QVariantMap observedCaptureDiagnostics() const;
    QString observedCaptureDisposition() const;
    bool observedCaptureBusy() const { return observed_pending_; }
    std::shared_ptr<const ObservedEvidence> capturedObservation() const { return observed_; }

    // C++ integration seam: the caller freezes the actual UI-owned registry,
    // numerical revision and complete command catalog. No caller supplies samples.
    // Existing acquisition does not enable capture until this context is set.
    bool setObservedCaptureContext(const QJsonObject& registry, quint64 numericalRevision, const QJsonArray& commands);
    void clearObservedCaptureContext();
    Q_INVOKABLE QString requestObservedCapture();
    Q_INVOKABLE void cancelObservedCapture();

    void configure(const QJsonObject& configuration, const QJsonObject& context);
    void startProcess(const QString& executable, const QStringList& arguments = {});
    void setTimeoutMs(int milliseconds);
    Q_INVOKABLE void startConfiguredPublisher();
    Q_INVOKABLE void startSynthetic(const QString& preset, quint32 seed);
    Q_INVOKABLE void manual();
    Q_INVOKABLE void startReplay(const QUrl& recording);
    Q_INVOKABLE void showHistorical(const QUrl& recording);
    Q_INVOKABLE void pause();
    Q_INVOKABLE void resume();
    Q_INVOKABLE void setRate(double rate);
    Q_INVOKABLE void restart();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void exportRecording(const QUrl& destination);
    Q_INVOKABLE void exportReceiverJournal(const QUrl& destination);
signals:
    void changed();
    void failed(const QString& code, const QString& message);
    void recordingExported(const QUrl& destination);
    void observedCaptureChanged();
    void observedCaptureReady(const QString& snapshotId);
    void observedCaptureFailed(const QString& requestId, const QString& code, const QString& message);
private:
    QThread thread_;
    TelemetryIoWorker* worker_ = nullptr;
    QTimer poll_;
    QVariantMap snapshot_{{"mode", "manual"}, {"status", "unconfigured"}, {"active", false}};
    QVariantList trace_preview_;
    QString publisher_executable_;
    QStringList publisher_arguments_;
    bool publisher_configured_ = false, bundled_publisher_ = false, poll_pending_ = false;
    QJsonObject observed_registry_;
    QJsonArray observed_commands_;
    QString observed_context_hash_, acquisition_hash_, observed_request_, observed_disposition_ = "not_configured";
    quint64 observed_revision_ = 0, observed_generation_ = 0, observed_source_serial_ = 0;
    bool observed_context_set_ = false, observed_pending_ = false, observed_retired_ = false;
    std::shared_ptr<const ObservedEvidence> observed_;
    std::shared_ptr<std::atomic<quint64>> source_serial_ = std::make_shared<std::atomic<quint64>>(0);
    void retireObserved(const QString& reason);
    void refreshObservedDisposition();
};
} // namespace breakwater::desktop::telemetry

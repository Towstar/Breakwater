#pragma once

#include <QByteArray>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <memory>

namespace breakwater::desktop::telemetry {

// Stateful, thread-confined monitoring only. The lifecycle adapter owns the
// thread, source process and receiver/playback clocks. No forecast or operating
// controls are written here. Configure and a new epoch retire current readings.
class TelemetryStream final {
public:
    TelemetryStream();
    ~TelemetryStream();
    TelemetryStream(TelemetryStream&&) noexcept;
    TelemetryStream& operator=(TelemetryStream&&) noexcept;
    TelemetryStream(const TelemetryStream&) = delete;
    TelemetryStream& operator=(const TelemetryStream&) = delete;

    void configure(const QJsonObject& configuration, const QJsonObject& context);
    void beginEpoch(const QString& mode);
    bool ingest(const QByteArray& frame, double nowEventS, double receivedMonotonicS, const QString& receivedUtc);
    void advance(double nowEventS, double receivedMonotonicS);

    QJsonObject report() const;
    QJsonArray trace(int maximum = 128) const;
    // Original validated frame bytes, including start; no exporter may quietly
    // represent a truncated recording as a complete replayable epoch.
    QByteArray recording() const;
    bool faulted() const;
    QString streamId() const;
    QString epochId() const;
    QString mode() const;
    double eventTime() const;
    bool paused() const;
    double clockRate() const;

private:
    struct State;
    std::unique_ptr<State> state_;
};

} // namespace breakwater::desktop::telemetry

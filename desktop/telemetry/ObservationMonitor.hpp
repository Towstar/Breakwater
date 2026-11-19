#pragma once
#include "TelemetryStream.hpp"
#include "breakwater/observation_detection.hpp"
#include <memory>

namespace breakwater::desktop::telemetry {
// Local retained diagnostic, separate from the unchanged G1 event/snapshot DTOs.
void validate_detector_capture(const QJsonObject& report,const QJsonObject& snapshot);
// Worker-thread only. No timers, database writes, model calls or control writes.
class ObservationMonitor final {
public:
    ObservationMonitor();
    void reset(bool declaredHoldModel=false);
    QJsonObject observe(const ObservedEvidence&);
    QJsonObject acknowledge(const QString& eventId,const QString& actor,const QString& utc);
    QJsonObject report() const { return report_; }
private:
    std::unique_ptr<observations::ObservationDetector> detector_;
    QJsonObject context_,report_;
};
}

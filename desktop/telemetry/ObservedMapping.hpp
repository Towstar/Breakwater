#pragma once
#include <QJsonObject>
#include "breakwater/observations.hpp"

namespace breakwater::desktop::telemetry {
struct MappedCapture { QJsonObject document, diagnostics, effective_network; };
// The registered design context remains unchanged. The separate effective network
// is for immutable native calculations only; it is never an editor document.
MappedCapture map_volume_capture(const QJsonObject& snapshot, const QJsonObject& context, bool sourceAvailable);
observations::MappingResult native_volume_capture(const QJsonObject& snapshot, const QJsonObject& context, bool sourceAvailable);
}

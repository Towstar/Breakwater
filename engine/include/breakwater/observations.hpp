#pragma once

#include "breakwater/telemetry.hpp"
#include "transport.hpp"
#include <optional>
#include <span>

namespace breakwater::observations {

inline constexpr const char* mapper_version = "bounded-observations-0.1.0";

struct BoundedValue { double value = 0, bound = 0; };
struct MappedValue {
    std::string quantity, attachment_id, unit;
    telemetry::Value value;
    double bound = 0;
    std::vector<std::string> sensor_ids;
};
struct MappingIssue { std::string code, attachment_id, sensor_id; };
struct MappingResult {
    // A copy of the original evidence with newly computed coverage/derived state.
    // Neither this snapshot nor the effective network grants control authority.
    telemetry::ObservedSnapshot snapshot;
    std::vector<MappedValue> values;
    std::vector<MappingIssue> issues;
    std::optional<Network> effective_network;
};

// Absolute uncertainty, including calibration error. No extrapolation/clipping.
std::optional<BoundedValue> calibrate(const telemetry::Sensor&, double canonical_value);

// Cell and interval averages only, with exact overlap weights. Unsupported point
// support is unavailable. This does not infer concentration from volume sensors.
std::optional<double> concentration_average(const PipeConfig&, std::span<const double>,
                                          const telemetry::PhysicalSupport&);

// Caller binds the DTOs to a semantically validated network/configuration context.
// This pure numerical mapper also rejects invalid arithmetic, duplicate samples,
// incoherent/stale observations and insufficient operation-specific coverage.
// Inventory, net source delivery, tank-route delivery and pump feedback are needed
// for a volume forecast; pressure/temperature/concentration are not substituted.
MappingResult map_volume_state(const Network& design, const telemetry::SensorConfiguration&,
                               const telemetry::ObservedSnapshot& captured);

} // namespace breakwater::observations

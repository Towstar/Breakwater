#pragma once

#include "types.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace breakwater::telemetry {

inline constexpr int version = 1;
inline constexpr std::size_t maximum_sensors = 256;
inline constexpr std::size_t maximum_readings_per_batch = 256;
inline constexpr std::size_t maximum_frame_bytes = 256 * 1024;
inline constexpr std::size_t maximum_reorder_batches = 16;
inline constexpr std::size_t maximum_reorder_bytes = 4 * 1024 * 1024;
inline constexpr std::size_t maximum_trace_bytes = 32 * 1024 * 1024;
inline constexpr std::size_t maximum_trace_readings = 32768;
inline constexpr std::size_t maximum_ingress_readings_per_s = 1024;
inline constexpr std::size_t maximum_ingress_bytes_per_s = 1024 * 1024;
inline constexpr std::uint64_t maximum_sequence = 9007199254740991ULL;

// These DTOs describe evidence and lifecycle commands, never an operating-control
// authority. They deliberately have no Qt, Python, transport or model dependency.
using Value = std::variant<double, std::string>;
struct Policy {
    double max_skew_s = 1.0;
    double reorder_window_s = 2.0;
    double future_allowance_s = 0.25;
    double heartbeat_period_s = 1.0;
    double source_timeout_s = 5.0;
};
struct Attachment { std::string kind, id; };
struct PhysicalSupport {
    std::string kind = "none";
    double acquisition_window_s = 0.0, delay_s = 0.0;
    std::optional<std::uint64_t> cell_index;
    std::optional<double> start_m, end_m;
};
struct CalibrationKnot { double level_m = 0.0, volume_m3 = 0.0; };
struct Calibration {
    std::string calibration_id, kind = "identity";
    std::uint64_t version = 1;
    double error_bound = 0.0;
    std::vector<CalibrationKnot> knots;
};
struct Sensor {
    std::string sensor_id, label, quantity, unit, source_id, channel_id;
    Attachment attachment;
    bool enabled = true;
    double expected_period_s = 1.0, max_age_s = 5.0, uncertainty_bound = 0.0;
    std::string group_id, role = "primary";
    PhysicalSupport physical_support;
    std::optional<std::string> pipe_parameter_hash;
    Calibration calibration;
    std::optional<double> instrument_min, instrument_max;
};
struct SensorConfiguration {
    std::string configuration_id, physical_hash;
    std::uint64_t revision = 0;
    Policy policy;
    std::vector<Sensor> sensors;
};
struct Reading {
    std::string sensor_id;
    double event_time_s = 0.0, acquisition_start_s = 0.0, acquisition_end_s = 0.0;
    Value value;
    std::string unit, quality;
};
struct ClockState { bool paused = false; double clock_rate = 1.0; };
struct Acknowledgement {
    std::string request_id, operation;
    double effective_event_time_s = 0.0;
    ClockState clock;
};
struct StreamError { std::string code, message; };
struct StreamFrame {
    std::string type, stream_id, epoch_id, configuration_hash, physical_hash;
    std::uint64_t sequence = 0;
    double event_time_s = 0.0;
    std::string mode, end_reason;
    ClockState clock;
    std::vector<Reading> readings;
    std::optional<Acknowledgement> acknowledgement;
    std::optional<StreamError> error;
};
struct StreamCommand {
    std::string request_id, stream_id, epoch_id, operation;
    std::optional<double> clock_rate;
};
struct Sample {
    std::string sensor_id;
    std::uint64_t frame_sequence = 0;
    double event_time_s = 0.0, acquisition_start_s = 0.0, acquisition_end_s = 0.0, received_monotonic_s = 0.0;
    std::string received_utc;
    Value original_value, canonical_value;
    std::string original_unit, canonical_unit, reported_quality, receiver_status;
};
struct DerivedState {
    std::string quantity, attachment_id, unit, origin;
    Value value;
    std::vector<std::string> source_sensor_ids;
};
struct Coverage {
    std::vector<std::string> required_sensor_ids, missing_sensor_ids, conflicting_sensor_ids;
};
struct ObservedSnapshot {
    std::string snapshot_id, physical_hash, configuration_hash, stream_id, epoch_id, mode;
    std::uint64_t numerical_revision = 0;
    double captured_event_time_s = 0.0, expires_event_time_s = 0.0;
    std::vector<Sample> samples;
    std::vector<ControlValue> commands;
    std::vector<DerivedState> derived_state;
    Coverage coverage;
    std::vector<std::string> assumptions;
    std::string availability;
    // All G1 interchange snapshots are non-actionable. G3 must independently
    // establish current eligibility and preserve the existing operator action.
    bool actionable = false;
};
struct EventAcknowledgement { std::string actor, acknowledged_utc; };
struct EventEvidence { std::string metric, unit; double value = 0.0, threshold = 0.0; };
struct DetectionEvent {
    std::string event_id, physical_hash, configuration_hash, stream_id, epoch_id, snapshot_id;
    std::uint64_t revision = 0;
    std::string detector_id, detector_version, threshold_version, status, severity;
    double onset_event_time_s = 0.0, updated_event_time_s = 0.0;
    std::optional<double> cleared_event_time_s;
    std::optional<EventAcknowledgement> acknowledgement;
    std::vector<std::string> sensor_ids, asset_ids;
    std::vector<EventEvidence> evidence;
    std::string diagnosis, explanation;
    bool actionable = false;
};

} // namespace breakwater::telemetry

#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace breakwater::eog {

// Interchange only: no Qt, numerical implementation, transport or control authority.
// Wire field names and version rules are defined by contracts/eog/*.schema.json.
inline constexpr int version = 1;
inline constexpr std::size_t maximum_request_bytes = 1024 * 1024;
inline constexpr std::size_t maximum_document_bytes = 4 * 1024 * 1024;
inline constexpr std::size_t maximum_csv_bytes = 128 * 1024 * 1024;
inline constexpr std::size_t maximum_rows = 2'000'000;
inline constexpr std::size_t maximum_trajectory_samples = 8192;
inline constexpr std::size_t maximum_search_candidates = 10'000;
inline constexpr std::int64_t sample_period_s = 300;
inline constexpr std::int64_t maximum_age_s = 300;
using UnixTime = std::int64_t;
using AssetId = std::string; // UTF-8; at most 256 UTF-16 code units on the wire.

struct CapturedFile { std::string sha256; std::uint64_t size_bytes = 0; };
struct CaptureManifest {
    std::string dataset_identity, capture_id;
    std::map<std::string, std::string> source_hashes;
    std::map<std::string, CapturedFile> captured_files; // exactly the three fixed filenames
    UnixTime source_first_unix = 0, source_now_unix = 0, cutoff_unix = 0;
    std::string context = "snapshot_now";
    std::int64_t sample_period_s = eog::sample_period_s, max_age_s = maximum_age_s;
};
struct Binding {
    std::string request_id, operation, dataset_identity, capture_id;
    int method_version = 1;
    UnixTime cutoff_unix = 0;
};
struct Diagnostic { std::string code, message; };
struct Evidence {
    std::string evidence_id;
    AssetId asset_id;
    std::string metric, unit, quality;
    UnixTime timestamp_unix = 0;
    std::optional<double> value;
};
struct OnsetBounds { UnixTime earliest_unix = 0, latest_unix = 0; };
struct Event {
    std::string event_id;
    AssetId system_id, facility_id, asset_id;
    std::string category, state, coverage, threshold_version, recommended_action;
    std::optional<UnixTime> onset_unix, end_unix;
    std::optional<OnsetBounds> onset_bounds;
    UnixTime updated_unix = 0;
    std::vector<std::string> evidence_ids, limitations;
    std::optional<double> countdown_hours, lost_revenue_per_hour;
};
struct Trip { double arrival_elapsed_s = 0; double volume_bbl = 0, cost = 0; };
struct TruckJob {
    std::string job_id, truck_id;
    AssetId system_id, facility_id;
    double called_elapsed_s = 0, available_again_elapsed_s = 0;
    double response_min = 0, cycle_min = 0;
    std::vector<Trip> trips;
};
struct WellShutIn { AssetId well_id, facility_id; double start_elapsed_s = 0, end_elapsed_s = 0; };
struct RepairDeadline { AssetId facility_id; double repair_elapsed_s = 0; };
struct Plan {
    std::string plan_id, plan_hash, context;
    UnixTime start_unix = 0;
    std::vector<RepairDeadline> repair_deadlines;
    std::vector<TruckJob> truck_jobs;
    std::vector<WellShutIn> well_shut_ins;
    std::vector<AssetId> inspection_assets;
};
struct Cost { double truck_cost = 0, lost_revenue = 0, total = 0; };
struct Trajectory {
    AssetId facility_id, tank_id;
    double capacity_bbl = 0, shutdown_volume_bbl = 0;
    std::vector<double> elapsed_s; // duplicate pairs retain before/after jumps
    std::vector<double> volumes_bbl; // preserve finite negative/overflow predictions
};
struct Occupancy { std::string truck_id, job_id; double start_elapsed_s = 0, end_elapsed_s = 0; };
struct Violation {
    std::string code, message;
    std::optional<AssetId> asset_id;
    std::optional<double> elapsed_s;
};
struct Shutdown { AssetId facility_id; double elapsed_s = 0; };
struct Evaluation {
    Plan plan;
    bool feasible = false;
    std::string coverage, outcome;
    std::vector<Shutdown> shutdowns;
    std::vector<Trajectory> trajectories;
    std::optional<Cost> cost;
    std::vector<Occupancy> fleet_occupancy;
    std::vector<Violation> violations;
    std::vector<std::string> assumptions, limitations;
};
struct Search {
    std::string scope, optimality, space_description;
    std::optional<double> action_time_resolution_s;
    std::uint32_t evaluated_candidates = 0, candidate_limit = 0;
    double time_limit_s = 0, elapsed_s = 0;
    bool complete = false;
};
struct Validation {
    std::string validation_id, plan_id, plan_hash, evaluator_method, validator_method;
    bool independent = true, feasible = false;
    std::string coverage, outcome;
    std::vector<Shutdown> shutdowns;
    std::vector<RepairDeadline> validated_horizons;
    std::optional<Cost> cost;
    std::vector<Violation> violations;
    std::optional<double> minimum_shutdown_margin_bbl;
    std::vector<std::string> evidence_ids;
    std::vector<Evidence> evidence;
};
struct Arguments {
    std::vector<AssetId> system_ids;
    std::optional<Plan> plan;
    std::string evaluator_method;
    std::uint32_t candidate_limit = 10'000;
    double time_limit_s = 90;
};
struct AnalysisRequest {
    Binding binding;
    std::string capture_directory;
    CaptureManifest capture_manifest;
    Arguments arguments; // only fields for the selected operation may be serialized
};
struct AnalysisResult {
    std::string analysis_id, coverage;
    std::vector<Event> events;
    std::vector<Evidence> evidence;
    std::vector<std::string> assumptions, limitations;
    std::optional<Evaluation> evaluation;
    std::vector<Evaluation> evaluations;
    std::optional<std::string> selected_plan_id;
    std::optional<Search> search;
    std::optional<Validation> validation;
};
struct AnalysisResponse {
    Binding binding;
    std::string status;
    std::vector<Diagnostic> diagnostics;
    std::optional<AnalysisResult> result; // absent for unavailable/error/cancelled
};
} // namespace breakwater::eog

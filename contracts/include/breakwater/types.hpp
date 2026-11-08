#pragma once

#include <optional>
#include <cstdint>
#include <string>
#include <vector>

namespace breakwater {

inline constexpr const char* solver_version = "constant-flow-0.2.0";
inline constexpr int project_version = 1;
// Version 2 is reserved by G1 for a sensor-bearing legacy-project envelope.
inline constexpr int source_allocation_project_version = 3;
inline constexpr std::uint32_t source_share_total = 10000;

enum class AssetKind { tank, source, treatment, trucking };

struct Asset {
    std::string id;
    std::string label;
    AssetKind kind = AssetKind::tank;
    double capacity_m3 = 0.0; // Tank: m3; destination: m3/h (see kind).
    double initial_m3 = 0.0;
    double inflow_m3h = 0.0;
    double curtailment_m3h = 0.0;
    double curtailment_max_m3h = 0.0;
    double cost_per_m3 = 0.0; // Source curtailment / destination throughput.
};

struct Route {
    std::string id;
    std::string source_id;
    std::string target_id;
    double capacity_m3h = 0.0;
    double setpoint_m3h = 0.0;
    bool has_pump = false;
    bool available = true;
    double cost_per_m3 = 0.0;
    // Production-feed allocation, in hundredths of one percent. Source routes
    // must sum to 10000; tank-origin routes keep 10000. Fractions are fixed
    // configuration, independent of commanded curtailment/route-flow controls.
    std::uint32_t source_share_bps = source_share_total;
};

struct Network { std::vector<Asset> assets; std::vector<Route> routes; };
struct ControlValue { std::string id; double value = 0.0; };
struct ControlDescriptor {
    std::string id;
    std::string asset_id;
    std::string unit = "m3/h";
    double value = 0.0;
    double minimum = 0.0;
    double maximum = 0.0;
    double cost_per_m3 = 0.0;
};

struct SimulationConfig { double horizon_h = 3.0; double step_h = 0.025; };
enum class Solver { sparse, dense_reference, affine };

struct Issue {
    std::string code;
    std::string asset_id;
    std::string message;
    std::optional<double> time_h;
};
struct LimitEvent { std::string asset_id; std::string limit; double time_h = 0.0; };
struct SimulationResult {
    std::vector<std::string> tank_ids;
    std::vector<double> time_h;
    std::vector<std::vector<double>> volumes_m3; // tank-major; sample 0 is incident state.
    double cost = 0.0;
    Solver solver = Solver::sparse;
};
struct ValidationReport {
    bool feasible = false;
    std::vector<Issue> violations;
    std::vector<LimitEvent> limits;
    double max_conservation_residual_m3 = 0.0;
    double minimum_headroom_m3 = 0.0;
};

std::string kind_name(AssetKind kind);
std::optional<AssetKind> parse_kind(const std::string& name);

} // namespace breakwater

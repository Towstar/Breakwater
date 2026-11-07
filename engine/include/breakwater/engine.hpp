#pragma once
#include "breakwater/types.hpp"
#include <span>

namespace breakwater {

struct InternalEdge { std::size_t source; std::size_t target; double flow_m3h; };
struct CompiledNetwork {
    Network network; // Resolved immutable copy for this candidate.
    std::vector<std::string> tank_ids;
    std::vector<double> initial_m3;
    std::vector<double> capacity_m3;
    std::vector<double> external_in_m3h;
    std::vector<double> external_out_m3h;
    std::vector<InternalEdge> edges;
    double cost_per_h = 0.0;
};

std::vector<Issue> inspect_network(const Network& network);
std::vector<ControlDescriptor> control_catalog(const Network& network);
CompiledNetwork compile(const Network& network, std::span<const ControlValue> overrides = {});
SimulationResult simulate(const CompiledNetwork& network, SimulationConfig config = {}, Solver solver = Solver::sparse);
ValidationReport validate(const CompiledNetwork& network, const SimulationResult& result, SimulationConfig config = {});
// Caller owns output workspace; no allocation in endpoint evaluation.
void evaluate_endpoints(const CompiledNetwork& network, double horizon_h, std::span<double> output);
Network canonical_scenario();
Network main_pump_failure(const Network& network, const std::string& route_id = "main_pump");
std::vector<ControlValue> incident_controls(const std::string& case_name);

} // namespace breakwater

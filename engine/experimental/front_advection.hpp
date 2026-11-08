#pragma once
// Private F1b experiment. Never installed or linked into breakwater_engine.
#include "breakwater/transport.hpp"

namespace breakwater::front_experiment {
inline constexpr const char* solver_version = "fvm-muscl-mc-ssprk2-experimental-0.1.0";
struct Result {
    TransportResult transport;
    std::size_t stage_evaluations = 0, cell_stage_updates = 0;
    double maximum_concentration_mg_l = 0.0;
};
// Stage primitives are private test seams, not application interfaces.
double mc_slope(double left_difference, double right_difference);
std::vector<double> face_fluxes(std::span<const double> field, double velocity, double inlet);
void check_step_budget(std::size_t completed_steps, std::size_t cells, std::size_t max_steps);
Result advect(const PipeConfig&, std::span<const double> initial,
              std::span<const FlowKnot>, std::span<const InletKnot>, TransportConfig,
              std::span<const double> source = {});
} // namespace breakwater::front_experiment

#include "breakwater/transport.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <future>
#include <iostream>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <utility>

using namespace breakwater;

namespace {
void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}
void near(double actual, double expected, double tolerance = 1e-8) {
    require(std::isfinite(actual) && std::isfinite(expected) &&
                std::abs(actual - expected) <= tolerance + 1e-11 * std::max(std::abs(actual), std::abs(expected)),
            "Expected " + std::to_string(expected) + ", got " + std::to_string(actual));
}
template<class Exception = std::invalid_argument, class Function>
void rejects(Function&& function) {
    bool rejected = false;
    try { function(); } catch (const Exception&) { rejected = true; }
    require(rejected, "Expected request rejection did not occur");
}
PipeConfig pipe(std::size_t cells = 4, double length = 4.0, double area = 0.5) {
    return {length, area, 0.0, 0.0, cells};
}
TransportConfig config(double horizon = 1.0, double output_step = 0.1,
                       TransportBoundary boundary = TransportBoundary::dirichlet_inlet) {
    TransportConfig options;
    options.horizon_h = horizon; options.output_step_h = output_step; options.boundary = boundary;
    return options;
}
TransportResult solve(const PipeConfig& p, const std::vector<double>& initial, TransportConfig options,
                      const std::vector<FlowKnot>& flow = {{0.0, 0.0}},
                      const std::vector<InletKnot>& inlet = {{0.0, 0.0}},
                      const std::vector<double>& source = {}) {
    return transport(p, initial, flow, inlet, options, source);
}
void check_result(const PipeConfig& p, const TransportConfig& options, const TransportResult& result) {
    const auto frames = result.time_h.size();
    require(frames >= 2 && frames <= max_transport_frames, "Invalid saved frame count");
    require(result.x_m.size() == p.cells && result.concentration_mg_l.size() == frames,
            "Transport field shape does not match mesh/time grid");
    require(result.solver_version == transport_solver_version, "Missing solver provenance");
    for (const auto* series : {&result.inventory_mg, &result.cumulative_inlet_mg,
         &result.cumulative_outlet_mg, &result.cumulative_source_mg, &result.cumulative_decay_mg,
         &result.mass_balance_residual_mg}) require(series->size() == frames, "Ledger sample counts differ");
    near(result.time_h.front(), 0.0); near(result.time_h.back(), options.horizon_h);
    const double dx = p.length_m / static_cast<double>(p.cells);
    for (std::size_t j = 0; j < p.cells; ++j) near(result.x_m[j], (static_cast<double>(j) + 0.5) * dx);
    double minimum = std::numeric_limits<double>::infinity();
    for (std::size_t t = 0; t < frames; ++t) {
        require(std::isfinite(result.time_h[t]) && (t == 0 || result.time_h[t] > result.time_h[t - 1]),
                "Saved times are not strictly increasing");
        require(result.concentration_mg_l[t].size() == p.cells, "Wrong concentration row width");
        double sum = 0.0;
        for (const double c : result.concentration_mg_l[t]) {
            require(std::isfinite(c) && c >= -1e-10, "Nonfinite/materially negative concentration returned");
            minimum = std::min(minimum, c); sum += c;
        }
        near(result.inventory_mg[t], 1000.0 * p.area_m2 * dx * sum, 1e-6);
        const double residual = result.inventory_mg[t] - result.inventory_mg.front()
            - result.cumulative_inlet_mg[t] + result.cumulative_outlet_mg[t]
            - result.cumulative_source_mg[t] + result.cumulative_decay_mg[t];
        near(result.mass_balance_residual_mg[t], residual, 1e-6);
        near(residual, 0.0, 2e-6);
    }
    require(result.diagnostics.substeps > 0 && result.diagnostics.substeps <= options.max_substeps,
            "Invalid executed substep count");
    require(std::isfinite(result.diagnostics.max_cfl_number) &&
                result.diagnostics.max_cfl_number <= options.cfl_safety + 1e-10,
            "Reported positivity/CFL bound was exceeded");
    require(result.diagnostics.minimum_concentration_mg_l <= minimum + 1e-10 &&
                result.diagnostics.minimum_concentration_mg_l >= -1e-10,
            "Minimum concentration diagnostics are inconsistent");
    require(std::isfinite(result.diagnostics.max_mass_balance_residual_mg) &&
                result.diagnostics.max_mass_balance_residual_mg <= 2e-6,
            "Mass balance diagnostic exceeds numerical tolerance");
}
}

int main() {
    const std::vector<std::pair<std::string, std::function<void()>>> tests = {
        {"large finite concentration and mass do not overflow auxiliary norms", [] {
            for (const bool tiny_pipe : {false, true}) {
                auto p = tiny_pipe ? pipe(2, 0.002, 1e-308) : pipe(2, 1, 1);
                p.decay_per_h = 1.0;
                const double concentration = tiny_pipe ? 1e308 : 1e305;
                const auto options = config(1, 1, TransportBoundary::closed_no_flux);
                const auto run = solve(p, {concentration, concentration}, options, {{0, 0}}, {});
                near(run.concentration_mg_l.back()[0] / concentration, 0.09, 1e-12);
                near(run.inventory_mg.back() / run.inventory_mg.front(), 0.09, 1e-12);
                near(run.cumulative_decay_mg.back() / run.inventory_mg.front(), 0.91, 1e-12);
                require(std::isfinite(run.diagnostics.max_mass_balance_residual_mg) &&
                            run.diagnostics.max_mass_balance_residual_mg / run.inventory_mg.front() < 1e-12,
                        "Finite mass was rejected or normalized balance was lost");
            }
        }},
        {"finite physical mass survives a concentration-sum overflow", [] {
            const auto p = pipe(2, 0.002, 1e-308);
            const auto options = config(1, 1, TransportBoundary::closed_no_flux);
            const auto run = solve(p, {1e308, 1e308}, options, {{0, 0}}, {});
            near(run.inventory_mg.front(), 2.0, 1e-9);
            near(run.inventory_mg.back(), run.inventory_mg.front(), 1e-9);
            near(run.mass_balance_residual_mg.back(), 0.0, 1e-9);
            const auto sourced = solve(p, {0, 0}, options, {{0, 0}}, {}, {1e308, 1e308});
            near(sourced.inventory_mg.back(), 2.0, 1e-9);
            near(sourced.cumulative_source_mg.back(), 2.0, 1e-9);
            near(sourced.mass_balance_residual_mg.back(), 0.0, 1e-9);
        }},
        {"disabled processes preserve a nonuniform field and exact output horizon", [] {
            const auto p = pipe(); const auto options = config(0.3, 0.11);
            const std::vector<double> initial{0.1, 0.3, 0.7, 1.2};
            const auto result = solve(p, initial, options); check_result(p, options, result);
            require(result.time_h.size() == 4, "Unexpected final partial output interval");
            near(result.time_h[1], 0.11); near(result.time_h[2], 0.22); near(result.time_h[3], 0.3);
            for (const auto& row : result.concentration_mg_l)
                for (std::size_t j = 0; j < initial.size(); ++j) near(row[j], initial[j]);
            near(result.inventory_mg.front(), 1150.0);
        }},
        {"pure decay agrees with analytic solution and converges in time", [] {
            auto p = pipe(); p.decay_per_h = 0.4;
            const std::vector<double> initial(p.cells, 2.0);
            const auto coarse_config = config(1.0, 0.1, TransportBoundary::closed_no_flux);
            const auto fine_config = config(1.0, 0.05, TransportBoundary::closed_no_flux);
            const auto coarse = solve(p, initial, coarse_config, {{0, 0}}, {});
            const auto fine = solve(p, initial, fine_config, {{0, 0}}, {});
            check_result(p, coarse_config, coarse); check_result(p, fine_config, fine);
            const double exact = 2.0 * std::exp(-0.4);
            const double coarse_error = std::abs(coarse.concentration_mg_l.back()[0] - exact);
            const double fine_error = std::abs(fine.concentration_mg_l.back()[0] - exact);
            require(coarse_error < 0.02 && fine_error < 0.6 * coarse_error, "Decay does not show first-order convergence");
            near(fine.cumulative_decay_mg.back(), fine.inventory_mg.front() - fine.inventory_mg.back(), 1e-6);
            near(fine.cumulative_inlet_mg.back(), 0); near(fine.cumulative_outlet_mg.back(), 0);
        }},
        {"one open Euler step balances signed faces source and old-state decay", [] {
            auto p = pipe(2, 4, 0.25); p.dispersion_m2_h = 1; p.decay_per_h = 0.5;
            const auto options = config(0.125, 0.125);
            const auto result = solve(p, {4, 2}, options, {{0, 0.25}}, {{0, 1}}, {1, 3});
            check_result(p, options, result);
            require(result.diagnostics.substeps == 1, "Expected one declared Euler step");
            // dx=2 m, 500 L/cell, v=1 m/h. The three signed faces are
            // [-2, 5, 2] mg/L*m/h: half-cell inlet diffusion exceeds advection.
            // The shared internal face transfers mass but cancels from the ledger.
            near(result.concentration_mg_l.back()[0], 3.4375, 1e-12);
            near(result.concentration_mg_l.back()[1], 2.4375, 1e-12);
            near(result.inventory_mg.front(), 3000, 1e-10);
            near(result.cumulative_inlet_mg.back(), -62.5, 1e-10);
            near(result.cumulative_outlet_mg.back(), 62.5, 1e-10);
            near(result.cumulative_source_mg.back(), 250, 1e-10);
            near(result.cumulative_decay_mg.back(), 187.5, 1e-10);
            near(result.inventory_mg.back(), 2937.5, 1e-10);
            near(result.mass_balance_residual_mg.back(), 0, 1e-10);
        }},
        {"one closed Euler step cancels internal faces with source and decay", [] {
            auto p = pipe(3, 6, 0.125); p.dispersion_m2_h = 2; p.decay_per_h = 0.25;
            const auto options = config(0.25, 0.25, TransportBoundary::closed_no_flux);
            const auto result = solve(p, {1, 4, 2}, options, {{0, 0}}, {}, {0, 1, 2});
            check_result(p, options, result);
            require(result.diagnostics.substeps == 1, "Expected one declared Euler step");
            // dx=2 m, 250 L/cell and faces [0, -3, 2, 0]. The nonzero
            // interior exchanges change the field but not the summed inventory.
            near(result.concentration_mg_l.back()[0], 1.3125, 1e-12);
            near(result.concentration_mg_l.back()[1], 3.375, 1e-12);
            near(result.concentration_mg_l.back()[2], 2.625, 1e-12);
            near(result.inventory_mg.front(), 1750, 1e-10);
            near(result.cumulative_inlet_mg.back(), 0, 1e-10);
            near(result.cumulative_outlet_mg.back(), 0, 1e-10);
            near(result.cumulative_source_mg.back(), 187.5, 1e-10);
            near(result.cumulative_decay_mg.back(), 109.375, 1e-10);
            near(result.inventory_mg.back(), 1828.125, 1e-10);
            near(result.mass_balance_residual_mg.back(), 0, 1e-10);
        }},
        {"variable event steps telescope the old-inventory source and decay ledger", [] {
            auto p = pipe(2, 2, 0.5); p.decay_per_h = 0.5;
            const auto options = config(1, 1, TransportBoundary::closed_no_flux);
            const auto result = solve(p, {2, 4}, options, {{0, 0}, {0.125, 0}, {0.5, 0}}, {}, {1, 0});
            check_result(p, options, result);
            require(result.time_h.size() == 2, "Only initial and terminal output were requested");
            require(result.diagnostics.substeps == 3, "Did not stop at both schedule events");
            // The schedule partitions dt into 1/8, 3/8, 1/2 h. The first cell
            // is at source/decay equilibrium; the second is 4*(15/16)*(13/16)*(3/4).
            // Decay masses from the three OLD inventories are 187.5,
            // 539.0625 and 630.859375 mg, rather than a terminal-state estimate.
            near(result.concentration_mg_l.back()[0], 2, 1e-12);
            near(result.concentration_mg_l.back()[1], 585.0 / 256.0, 1e-12);
            near(result.inventory_mg.front(), 3000, 1e-10);
            near(result.cumulative_inlet_mg.back(), 0, 1e-10);
            near(result.cumulative_outlet_mg.back(), 0, 1e-10);
            near(result.cumulative_source_mg.back(), 500, 1e-10);
            near(result.cumulative_decay_mg.back(), 1357.421875, 1e-10);
            near(result.inventory_mg.back(), 2142.578125, 1e-10);
            near(result.mass_balance_residual_mg.back(), 0, 1e-10);
        }},
        {"constant field survives positive advection and diffusion", [] {
            auto p = pipe(8); p.dispersion_m2_h = 0.2;
            const auto options = config(0.75, 0.17);
            const auto result = solve(p, std::vector<double>(p.cells, 0.7), options, {{0, 0.5}}, {{0, 0.7}});
            check_result(p, options, result);
            for (const auto& row : result.concentration_mg_l) for (const double c : row) near(c, 0.7, 1e-12);
            near(result.inventory_mg.back(), 1400.0);
            near(result.cumulative_inlet_mg.back(), 262.5); near(result.cumulative_outlet_mg.back(), 262.5);
        }},
        {"CFL one pure upwind advection shifts by one cell", [] {
            const auto p = pipe(4, 4, 1); auto options = config(1, 1); options.cfl_safety = 1.0;
            const auto result = solve(p, {1, 2, 3, 4}, options, {{0, 1}}, {{0, 0}});
            check_result(p, options, result);
            for (std::size_t j = 0; j < p.cells; ++j) near(result.concentration_mg_l.back()[j], static_cast<double>(j), 1e-12);
            near(result.inventory_mg.back(), 6000); near(result.cumulative_outlet_mg.back(), 4000);
        }},
        {"closed diffusion cosine cell averages converge under mesh refinement", [] {
            const double pi = std::acos(-1.0);
            std::vector<double> errors;
            for (const std::size_t cells : {32U, 64U, 128U}) {
                auto p = pipe(cells, 1.0, 0.3); p.dispersion_m2_h = 0.1;
                auto options = config(0.2, 0.2, TransportBoundary::closed_no_flux); options.cfl_safety = 0.8;
                std::vector<double> initial(cells), mode(cells);
                for (std::size_t j = 0; j < cells; ++j) {
                    const double left = static_cast<double>(j) / static_cast<double>(cells);
                    const double right = static_cast<double>(j + 1) / static_cast<double>(cells);
                    mode[j] = (std::sin(pi * right) - std::sin(pi * left)) / (pi * (right - left));
                    initial[j] = 1.0 + 0.25 * mode[j];
                }
                const auto result = solve(p, initial, options, {{0, 0}}, {}); check_result(p, options, result);
                double l1_error = 0.0;
                for (std::size_t j = 0; j < cells; ++j) {
                    const double exact = 1.0 + 0.25 * mode[j] * std::exp(-0.1 * pi * pi * options.horizon_h);
                    l1_error += std::abs(result.concentration_mg_l.back()[j] - exact) / static_cast<double>(cells);
                }
                errors.push_back(l1_error); near(result.inventory_mg.back(), 300.0, 1e-7);
            }
            require(errors[1] < 0.4 * errors[0] && errors[2] < 0.4 * errors[1] && errors[2] < 1e-4,
                    "Closed diffusion does not show the expected refinement trend");
        }},
        {"zero-flow Dirichlet diffusion records signed inlet mass loss", [] {
            auto p = pipe(2, 2, 0.5); p.dispersion_m2_h = 1;
            const auto options = config(0.1, 0.1);
            const auto result = solve(p, {1, 1}, options); check_result(p, options, result);
            near(result.concentration_mg_l.back()[0], 0.8); near(result.concentration_mg_l.back()[1], 1);
            near(result.cumulative_inlet_mg.back(), -100); near(result.cumulative_outlet_mg.back(), 0);
            near(result.inventory_mg.back(), 900);
        }},
        {"inlet half-cell diffusion CFL preserves first-cell positivity", [] {
            auto p = pipe(2, 2, 1); p.dispersion_m2_h = 1;
            auto options = config(0.5, 0.5); options.cfl_safety = 1;
            const auto result = solve(p, {1, 0}, options); check_result(p, options, result);
            require(result.diagnostics.substeps >= 2, "Used unstable interior-only diffusion bound at inlet");
            require(result.diagnostics.minimum_concentration_mg_l >= -1e-12, "First-cell diffusion became negative");
        }},
        {"flow changes and stops are resolved between output samples", [] {
            const auto p = pipe(2, 2, 1); auto options = config(0.5, 0.5); options.cfl_safety = 1;
            for (const auto& schedule : std::vector<std::vector<FlowKnot>>{{{0, 0}, {0.25, 2}}, {{0, 2}, {0.25, 0}}}) {
                const auto result = solve(p, {0, 0}, options, schedule, {{0, 1}}); check_result(p, options, result);
                near(result.concentration_mg_l.back()[0], 0.5); near(result.concentration_mg_l.back()[1], 0);
                near(result.cumulative_inlet_mg.back(), 500);
                require(result.diagnostics.substeps >= 2, "Flow event was skipped");
            }
        }},
        {"piecewise linear inlet uses interval-average forcing", [] {
            const auto p = pipe(2, 2, 1); auto options = config(0.5, 0.5); options.cfl_safety = 1;
            const auto result = solve(p, {0, 0}, options, {{0, 2}}, {{0, 0}, {0.25, 1}, {0.5, 0}});
            check_result(p, options, result);
            near(result.concentration_mg_l.back()[0], 0.375); near(result.concentration_mg_l.back()[1], 0.125);
            near(result.cumulative_inlet_mg.back(), 500); near(result.inventory_mg.back(), 500);
        }},
        {"source and reaction ledger uses concentration-to-mass conversion", [] {
            auto p = pipe(); p.decay_per_h = 0.5;
            const auto options = config(1, 0.125, TransportBoundary::closed_no_flux);
            const auto balanced = solve(p, {1, 1, 1, 1}, options, {{0, 0}}, {}, {0.5, 0.5, 0.5, 0.5});
            check_result(p, options, balanced);
            for (const double c : balanced.concentration_mg_l.back()) near(c, 1.0);
            near(balanced.inventory_mg.back(), 2000); near(balanced.cumulative_source_mg.back(), 1000);
            near(balanced.cumulative_decay_mg.back(), 1000);
            p.decay_per_h = 0; const auto source_config = config(0.5, 0.1, TransportBoundary::closed_no_flux);
            const auto sourced = solve(p, {0, 0, 0, 0}, source_config, {{0, 0}}, {}, {1, 0, 2, 0});
            check_result(p, source_config, sourced);
            near(sourced.concentration_mg_l.back()[0], 0.5); near(sourced.concentration_mg_l.back()[2], 1.0);
            near(sourced.inventory_mg.back(), 750); near(sourced.cumulative_source_mg.back(), 750);
        }},
        {"input profiles and concurrent transport calls remain independent", [] {
            const auto p = pipe(); const auto options = config(0.25, 0.25);
            const std::vector<double> initial(p.cells, 0.2), source(p.cells, 0.0);
            const auto before_initial = initial, before_source = source;
            const std::vector<InletKnot> inlet{{0, 1}};
            const std::vector<FlowKnot> slow{{0, 0.5}}, fast{{0, 1}};
            auto a = std::async(std::launch::async, [&] { return solve(p, initial, options, slow, inlet, source); });
            auto b = std::async(std::launch::async, [&] { return solve(p, initial, options, fast, inlet, source); });
            const auto left = a.get(), right = b.get(); check_result(p, options, left); check_result(p, options, right);
            near(left.concentration_mg_l.back()[0], 0.4); near(right.concentration_mg_l.back()[0], 0.6);
            near(left.inventory_mg.back(), 500); near(right.inventory_mg.back(), 600);
            require(initial == before_initial && source == before_source, "Input concentration buffers changed");
            near(slow.front().flow_m3h, 0.5); near(fast.front().flow_m3h, 1); near(inlet.front().concentration_mg_l, 1);
        }},
        {"invalid pipe geometry and integration options are rejected", [] {
            for (auto p : std::vector<PipeConfig>{{0, 1, 0, 0, 4}, {1, 0, 0, 0, 4}, {1, 1, -1, 0, 4},
                    {1, 1, 0, -1, 4}, {1, 1, 0, 0, 1}, {1, 1, 0, 0, max_transport_cells + 1}}) {
                const std::vector<double> initial(p.cells, 0); rejects([&] { (void)solve(p, initial, config()); });
            }
            for (const double bad : {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()}) {
                auto p = pipe(); p.length_m = bad; rejects([&] { (void)solve(p, {0, 0, 0, 0}, config()); });
                p = pipe(); p.area_m2 = bad; rejects([&] { (void)solve(p, {0, 0, 0, 0}, config()); });
                p = pipe(); p.dispersion_m2_h = bad; rejects([&] { (void)solve(p, {0, 0, 0, 0}, config()); });
            }
            std::vector<TransportConfig> invalid;
            auto c = config(); c.horizon_h = 0; invalid.push_back(c); c = config(); c.horizon_h = 25; invalid.push_back(c);
            c = config(); c.output_step_h = 0; invalid.push_back(c); c = config(); c.cfl_safety = 0; invalid.push_back(c);
            c = config(); c.cfl_safety = 1.01; invalid.push_back(c); c = config(); c.max_substeps = 0; invalid.push_back(c);
            c = config(); c.max_substeps = max_transport_substeps + 1; invalid.push_back(c);
            c = config(); c.boundary = static_cast<TransportBoundary>(99); invalid.push_back(c);
            c = config(); c.horizon_h = std::numeric_limits<double>::quiet_NaN(); invalid.push_back(c);
            c = config(); c.cfl_safety = std::numeric_limits<double>::infinity(); invalid.push_back(c);
            for (const auto& options : invalid) rejects([&] { (void)solve(pipe(), {0, 0, 0, 0}, options); });
        }},
        {"invalid initial and source profiles are rejected", [] {
            const auto p = pipe();
            for (const auto& initial : std::vector<std::vector<double>>{{}, {0, 0}, {0, -1, 0, 0},
                    {0, std::numeric_limits<double>::quiet_NaN(), 0, 0}, {0, std::numeric_limits<double>::infinity(), 0, 0}})
                rejects([&] { (void)solve(p, initial, config()); });
            for (const auto& source : std::vector<std::vector<double>>{{1}, {0, -1, 0, 0},
                    {0, std::numeric_limits<double>::quiet_NaN(), 0, 0}})
                rejects([&] { (void)solve(p, {0, 0, 0, 0}, config(), {{0, 0}}, {{0, 0}}, source); });
        }},
        {"invalid schedules and closed-pipe nonzero flow are rejected", [] {
            const auto p = pipe(); const auto options = config();
            const double nan = std::numeric_limits<double>::quiet_NaN(), inf = std::numeric_limits<double>::infinity();
            for (const auto& flow : std::vector<std::vector<FlowKnot>>{{}, {{0.1, 0}}, {{0, -1}}, {{0, 0}, {0, 1}},
                    {{0, 0}, {0.5, 0}, {0.25, 0}}, {{0, inf}}, {{nan, 0}}})
                rejects([&] { (void)solve(p, {0, 0, 0, 0}, options, flow); });
            for (const auto& inlet : std::vector<std::vector<InletKnot>>{{}, {{0.1, 0}}, {{0, -1}}, {{0, 0}, {0, 1}},
                    {{0, 0}, {0.5, 0}, {0.25, 0}}, {{0, inf}}, {{nan, 0}}})
                rejects([&] { (void)solve(p, {0, 0, 0, 0}, options, {{0, 0}}, inlet); });
            const auto closed = config(1, 0.1, TransportBoundary::closed_no_flux);
            rejects([&] { (void)solve(p, {0, 0, 0, 0}, closed, {{0, 0}, {0.5, 1}}, {}); });
        }},
        {"output memory substep and cell-update budgets are enforced", [] {
            auto options = config(1, 1e-9);
            rejects([&] { (void)solve(pipe(), {0, 0, 0, 0}, options); });
            auto p = pipe(max_transport_cells); options = config(1, 0.001);
            rejects([&] { (void)solve(p, std::vector<double>(p.cells, 0), options); });
            p = pipe(8, 1, 1); p.dispersion_m2_h = 1; options = config(1, 1); options.max_substeps = 1;
            rejects<std::runtime_error>([&] { (void)solve(p, std::vector<double>(p.cells, 0), options); });
            p = pipe(max_transport_cells, 1, 1); p.dispersion_m2_h = 1; options = config(0.001, 0.001);
            rejects<std::runtime_error>([&] { (void)solve(p, std::vector<double>(p.cells, 0), options); });
        }},
        {"nonfinite derived arithmetic and non-advancing time fail explicitly", [] {
            auto p = pipe(); p.length_m = 1e-300; p.dispersion_m2_h = 1;
            rejects<std::runtime_error>([&] { (void)solve(p, {0, 0, 0, 0}, config()); });
            p = pipe();
            rejects<std::runtime_error>([&] { (void)solve(p, {0, 0, 0, 0}, config(), {{0, std::numeric_limits<double>::max()}}, {{0, 0}}); });
            p = pipe(4, 1, 1);
            rejects<std::runtime_error>([&] { (void)solve(p, std::vector<double>(p.cells, std::numeric_limits<double>::max()), config()); });
            const auto options = config(1, 1);
            rejects<std::runtime_error>([&] { (void)solve(p, {0, 0, 0, 0}, options, {{0, 0}, {0.5, 1e300}}, {{0, 0}}); });
        }}
    };
    int failures = 0;
    for (const auto& [name, test] : tests) {
        try { test(); std::cout << "PASS " << name << '\n'; }
        catch (const std::exception& error) { ++failures; std::cerr << "FAIL " << name << ": " << error.what() << '\n'; }
    }
    std::cout << tests.size() - static_cast<std::size_t>(failures) << '/' << tests.size() << " transport cases passed\n";
    return failures == 0 ? 0 : 1;
}

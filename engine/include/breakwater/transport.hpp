#pragma once
#include <cstddef>
#include <span>
#include <string>
#include <vector>

namespace breakwater {

inline constexpr const char* transport_solver_version = "fvm-upwind-euler-0.1.0";
inline constexpr std::size_t max_transport_cells = 4096;
inline constexpr std::size_t max_transport_frames = 10001;
inline constexpr std::size_t max_transport_values = 4000000;
inline constexpr std::size_t max_transport_substeps = 1000000;
inline constexpr std::size_t max_transport_cell_updates = 50000000;

struct PipeConfig {
    double length_m = 100.0;
    double area_m2 = 0.1;
    double dispersion_m2_h = 1.0;
    double decay_per_h = 0.1;
    std::size_t cells = 64;
};

struct FlowKnot { double time_h; double flow_m3h; };
struct InletKnot { double time_h; double concentration_mg_l; };
enum class TransportBoundary { dirichlet_inlet, closed_no_flux };

struct TransportConfig {
    double horizon_h = 3.0;
    double output_step_h = 0.025;
    double cfl_safety = 0.9;
    TransportBoundary boundary = TransportBoundary::dirichlet_inlet;
    std::size_t max_substeps = max_transport_substeps;
};

struct TransportDiagnostics {
    std::size_t substeps = 0;
    double max_cfl_number = 0.0;
    double minimum_concentration_mg_l = 0.0;
    double max_mass_balance_residual_mg = 0.0;
};

struct TransportResult {
    std::string solver_version = transport_solver_version;
    std::vector<double> x_m; // Cell centers; concentration values are cell averages.
    std::vector<double> time_h; // Includes the initial state and exact requested horizon.
    std::vector<std::vector<double>> concentration_mg_l; // [time][cell].
    std::vector<double> inventory_mg;
    std::vector<double> cumulative_inlet_mg; // Signed net exchange into the left boundary.
    std::vector<double> cumulative_outlet_mg;
    std::vector<double> cumulative_source_mg;
    std::vector<double> cumulative_decay_mg;
    std::vector<double> mass_balance_residual_mg;
    TransportDiagnostics diagnostics;
};

// Synthetic scalar advection/diffusion/reaction, not pressure hydraulics or network
// chemistry. All initial/inlet/source concentrations and flow rates must be finite
// and nonnegative. Initial/source profiles contain one value per cell; empty source
// means zero. Source units are mg/L/h. No input buffers are mutated.
// Flow is piecewise constant and right-continuous. Inlet is piecewise linear; the
// interval-average inlet forcing is used with old-state explicit Euler fluxes. Both
// schedules start at time zero and hold their last value after their last knot.
// Dirichlet inlet diffusion uses dx/2, zero-gradient diffusive outlet, and a sufficient
// positivity bound dt*(v/dx + 3D/dx^2 + decay)<=cfl_safety. Closed mode requires zero
// flow and no boundary exchange (inlet may be empty). Substeps hit schedule/output times.
// Invalid input throws invalid_argument. Nonfinite derived arithmetic, materially
// negative concentrations, non-advancing time or exhausted budgets throw runtime_error.
// Values are never clipped; roundoff-sized negative values remain visible in diagnostics.
TransportResult transport(const PipeConfig& pipe, std::span<const double> initial_mg_l,
                          std::span<const FlowKnot> flow, std::span<const InletKnot> inlet,
                          TransportConfig config = {},
                          std::span<const double> source_mg_l_h = {});

} // namespace breakwater

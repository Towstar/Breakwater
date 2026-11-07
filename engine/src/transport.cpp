#include "breakwater/transport.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace breakwater {
namespace {
struct Sum {
    double value = 0.0, correction = 0.0;
    void add(double term) {
        const double adjusted = term - correction;
        const double next = value + adjusted;
        correction = (next - value) - adjusted;
        value = next;
    }
};
double mass_sum(std::span<const double> values, double cell_liters) {
    // Scale each cell before summing: the concentration sum can overflow even when
    // a very small pipe contains a finite physical tracer mass.
    Sum total; for (const double value : values) total.add(value * cell_liters); return total.value;
}
bool nonnegative(double v) { return std::isfinite(v) && v >= 0.0; }
void finite(double value, const char* message) {
    if (!std::isfinite(value)) throw std::runtime_error(message);
}
void check_profile(std::span<const double> values, std::size_t cells, bool optional, const char* message) {
    if ((values.size() != cells && !(optional && values.empty())) ||
        std::any_of(values.begin(), values.end(), [](double c) { return !nonnegative(c); }))
        throw std::invalid_argument(message);
}
void check_inputs(const PipeConfig& p, std::span<const double> initial,
                  std::span<const FlowKnot> flow, std::span<const InletKnot> inlet,
                  const TransportConfig& c, std::span<const double> source) {
    if (!std::isfinite(p.length_m) || p.length_m <= 0.0 || !std::isfinite(p.area_m2) || p.area_m2 <= 0.0 ||
        !nonnegative(p.dispersion_m2_h) || !nonnegative(p.decay_per_h) || p.cells < 2 || p.cells > max_transport_cells)
        throw std::invalid_argument("Pipe needs positive finite length/area, nonnegative dispersion/decay, and 2..4096 cells.");
    if (!std::isfinite(c.horizon_h) || c.horizon_h <= 0.0 || c.horizon_h > 24.0 ||
        !std::isfinite(c.output_step_h) || c.output_step_h <= 0.0 ||
        !std::isfinite(c.cfl_safety) || c.cfl_safety <= 0.0 || c.cfl_safety > 1.0 ||
        c.max_substeps == 0 || c.max_substeps > max_transport_substeps ||
        (c.boundary != TransportBoundary::dirichlet_inlet && c.boundary != TransportBoundary::closed_no_flux))
        throw std::invalid_argument("Invalid transport horizon, output step, CFL safety, boundary or step budget.");
    const double intervals = std::ceil(c.horizon_h / c.output_step_h);
    if (!std::isfinite(intervals) || intervals > static_cast<double>(max_transport_frames - 1))
        throw std::invalid_argument("Transport output exceeds 10001 frames.");
    const auto frames = std::max(std::size_t{1}, static_cast<std::size_t>(intervals)) + 1;
    if (frames > max_transport_values / p.cells)
        throw std::invalid_argument("Transport output exceeds four million concentration values.");
    check_profile(initial, p.cells, false, "Initial concentration must contain one finite nonnegative value per cell.");
    check_profile(source, p.cells, true, "Source must be empty or contain one finite nonnegative mg/L/h value per cell.");
    if (flow.empty() || flow.size() > max_transport_frames || flow.front().time_h != 0.0)
        throw std::invalid_argument("Flow schedule must start at zero and have 1..10001 knots.");
    for (std::size_t i = 0; i < flow.size(); ++i) {
        if (!nonnegative(flow[i].time_h) || !nonnegative(flow[i].flow_m3h) ||
            (i > 0 && flow[i].time_h <= flow[i - 1].time_h) ||
            (c.boundary == TransportBoundary::closed_no_flux && flow[i].flow_m3h != 0.0))
            throw std::invalid_argument("Flow knots need increasing finite times and nonnegative rates; closed pipes require zero flow.");
    }
    if (inlet.size() > max_transport_frames || (inlet.empty() && c.boundary != TransportBoundary::closed_no_flux) ||
        (!inlet.empty() && inlet.front().time_h != 0.0))
        throw std::invalid_argument("Open-pipe inlet schedule must start at zero and have 1..10001 knots.");
    for (std::size_t i = 0; i < inlet.size(); ++i)
        if (!nonnegative(inlet[i].time_h) || !nonnegative(inlet[i].concentration_mg_l) ||
            (i > 0 && inlet[i].time_h <= inlet[i - 1].time_h))
            throw std::invalid_argument("Inlet knots need increasing finite times and nonnegative concentrations.");
}
double inlet_value(std::span<const InletKnot> inlet, std::size_t index, double time) {
    if (inlet.empty()) return 0.0;
    while (index + 1 < inlet.size() && time >= inlet[index + 1].time_h) ++index;
    if (index + 1 == inlet.size() || time <= inlet[index].time_h) return inlet[index].concentration_mg_l;
    const auto& left = inlet[index]; const auto& right = inlet[index + 1];
    const double weight = (time - left.time_h) / (right.time_h - left.time_h);
    return (1.0 - weight) * left.concentration_mg_l + weight * right.concentration_mg_l;
}
struct Ledger { Sum inlet, outlet, source, decay; };

double ledger_residual(double inventory, double initial_inventory, const Ledger& ledger) {
    const double terms[] = {inventory, -initial_inventory, -ledger.inlet.value,
                            ledger.outlet.value, -ledger.source.value, ledger.decay.value};
    double scale = 1.0;
    for (const double term : terms) { finite(term, "Nonfinite tracer mass ledger term."); scale = std::max(scale, std::abs(term)); }
    Sum normalized;
    for (const double term : terms) normalized.add(term / scale);
    return normalized.value * scale;
}

void append_frame(TransportResult& r, const std::vector<double>& field, double inventory,
                  double initial_inventory, const Ledger& ledger) {
    r.concentration_mg_l.push_back(field);
    r.inventory_mg.push_back(inventory);
    r.cumulative_inlet_mg.push_back(ledger.inlet.value);
    r.cumulative_outlet_mg.push_back(ledger.outlet.value);
    r.cumulative_source_mg.push_back(ledger.source.value);
    r.cumulative_decay_mg.push_back(ledger.decay.value);
    r.mass_balance_residual_mg.push_back(ledger_residual(inventory, initial_inventory, ledger));
}
}

TransportResult transport(const PipeConfig& p, std::span<const double> initial,
                          std::span<const FlowKnot> flow, std::span<const InletKnot> inlet,
                          TransportConfig config, std::span<const double> source) {
    check_inputs(p, initial, flow, inlet, config, source);
    const double dx = p.length_m / static_cast<double>(p.cells);
    const double inverse_dx = 1.0 / dx;
    const double diffusion_over_dx = p.dispersion_m2_h * inverse_dx;
    const double diffusion_rate = diffusion_over_dx * inverse_dx;
    const double pipe_volume = p.area_m2 * p.length_m;
    const double cell_liters = (p.area_m2 * dx) * 1000.0;
    const double boundary_mass_scale = p.area_m2 * 1000.0;
    for (const double value : {dx, inverse_dx, diffusion_over_dx, diffusion_rate, pipe_volume, cell_liters, boundary_mass_scale})
        finite(value, "Nonfinite derived pipe geometry/diffusion/unit conversion.");
    if (dx <= 0.0 || pipe_volume <= 0.0 || cell_liters <= 0.0 || boundary_mass_scale <= 0.0)
        throw std::runtime_error("Pipe geometry or mass-unit conversion underflowed.");

    TransportResult r;
    const auto intervals = std::max(std::size_t{1}, static_cast<std::size_t>(std::ceil(config.horizon_h / config.output_step_h)));
    r.time_h.reserve(intervals + 1); r.time_h.push_back(0.0);
    for (std::size_t k = 1; k <= intervals; ++k) {
        const double time = k == intervals ? config.horizon_h : std::min(static_cast<double>(k) * config.output_step_h, config.horizon_h);
        if (time > r.time_h.back()) r.time_h.push_back(time);
    }
    r.x_m.reserve(p.cells);
    for (std::size_t j = 0; j < p.cells; ++j) r.x_m.push_back((static_cast<double>(j) + 0.5) * dx);
    r.concentration_mg_l.reserve(r.time_h.size()); r.inventory_mg.reserve(r.time_h.size());
    r.cumulative_inlet_mg.reserve(r.time_h.size()); r.cumulative_outlet_mg.reserve(r.time_h.size());
    r.cumulative_source_mg.reserve(r.time_h.size()); r.cumulative_decay_mg.reserve(r.time_h.size());
    r.mass_balance_residual_mg.reserve(r.time_h.size());
    std::vector<double> field(initial.begin(), initial.end()), next(p.cells), flux(p.cells + 1);
    r.diagnostics.minimum_concentration_mg_l = *std::min_element(field.begin(), field.end());
    const double initial_inventory = mass_sum(field, cell_liters);
    const double source_mass_rate = source.empty() ? 0.0 : mass_sum(source, cell_liters);
    finite(initial_inventory, "Initial tracer inventory is nonfinite.");
    finite(source_mass_rate, "Tracer source mass rate is nonfinite.");
    double inventory = initial_inventory;
    Ledger ledger;
    append_frame(r, field, inventory, initial_inventory, ledger);

    double time = 0.0;
    std::size_t flow_index = 0, inlet_index = 0;
    for (std::size_t frame = 1; frame < r.time_h.size(); ++frame) {
        const double output_time = r.time_h[frame];
        while (time < output_time) {
            if (r.diagnostics.substeps >= config.max_substeps ||
                r.diagnostics.substeps >= max_transport_cell_updates / p.cells)
                throw std::runtime_error("Transport substep/cell-update budget exhausted; refine the request or use another scheme.");
            while (flow_index + 1 < flow.size() && flow[flow_index + 1].time_h <= time) ++flow_index;
            while (inlet_index + 1 < inlet.size() && inlet[inlet_index + 1].time_h <= time) ++inlet_index;
            const double velocity = flow[flow_index].flow_m3h / p.area_m2;
            const bool open = config.boundary == TransportBoundary::dirichlet_inlet;
            const double stability_rate = velocity * inverse_dx + (open ? 3.0 : 2.0) * diffusion_rate + p.decay_per_h;
            finite(velocity, "Transport velocity is nonfinite.");
            finite(stability_rate, "Transport stability rate is nonfinite.");
            double event_time = output_time;
            if (flow_index + 1 < flow.size()) event_time = std::min(event_time, flow[flow_index + 1].time_h);
            if (inlet_index + 1 < inlet.size()) event_time = std::min(event_time, inlet[inlet_index + 1].time_h);
            const double stable_dt = stability_rate > 0.0 ? config.cfl_safety / stability_rate : std::numeric_limits<double>::infinity();
            const double remaining = event_time - time;
            double next_time = remaining <= stable_dt ? event_time : std::min(time + stable_dt, event_time);
            // Adding a small stable_dt to a later time may round upward by an ULP
            // large enough to violate the CFL budget. Round toward the old time;
            // a short subsequent step still lands exactly on the event/output.
            if ((next_time - time) * stability_rate > config.cfl_safety + 16.0 * std::numeric_limits<double>::epsilon())
                next_time = std::nextafter(next_time, time);
            if (!std::isfinite(next_time) || next_time <= time)
                throw std::runtime_error("Transport time step cannot advance at double precision.");
            const double dt = next_time - time;
            const double cfl = dt * stability_rate;
            if (!std::isfinite(cfl) || cfl > config.cfl_safety + 64.0 * std::numeric_limits<double>::epsilon())
                throw std::runtime_error("Rounded transport step exceeds the declared CFL bound.");
            r.diagnostics.max_cfl_number = std::max(r.diagnostics.max_cfl_number, cfl);
            const double inlet_average = 0.5 * inlet_value(inlet, inlet_index, time) + 0.5 * inlet_value(inlet, inlet_index, next_time);
            flux.front() = open ? velocity * inlet_average - 2.0 * diffusion_over_dx * (field.front() - inlet_average) : 0.0;
            for (std::size_t face = 1; face < p.cells; ++face)
                flux[face] = velocity * field[face - 1] - diffusion_over_dx * (field[face] - field[face - 1]);
            flux.back() = open ? velocity * field.back() : 0.0;
            for (const double value : flux) finite(value, "Nonfinite transport face flux.");
            const double decay_factor = dt * p.decay_per_h;
            for (std::size_t j = 0; j < p.cells; ++j) {
                const double transport_change = (dt * inverse_dx) * (flux[j] - flux[j + 1]);
                const double source_change = source.empty() ? 0.0 : dt * source[j];
                const double decay_change = decay_factor * field[j];
                next[j] = field[j] + transport_change - decay_change + source_change;
                finite(next[j], "Nonfinite transport concentration.");
                const double scale = std::max({1.0, std::abs(field[j]), std::abs(transport_change), std::abs(source_change), std::abs(decay_change)});
                finite(scale, "Nonfinite transport roundoff scale.");
                if (next[j] < -5e-12 * scale) throw std::runtime_error("Materially negative concentration; values are not clipped.");
                r.diagnostics.minimum_concentration_mg_l = std::min(r.diagnostics.minimum_concentration_mg_l, next[j]);
            }
            ledger.inlet.add((dt * boundary_mass_scale) * flux.front());
            ledger.outlet.add((dt * boundary_mass_scale) * flux.back());
            ledger.source.add(dt * source_mass_rate);
            ledger.decay.add(decay_factor * inventory);
            inventory = mass_sum(next, cell_liters);
            const double residual = ledger_residual(inventory, initial_inventory, ledger);
            const double mass_scale = std::max({1.0, std::abs(inventory), std::abs(initial_inventory), std::abs(ledger.inlet.value),
                                               std::abs(ledger.outlet.value), std::abs(ledger.source.value), std::abs(ledger.decay.value)});
            finite(inventory, "Nonfinite tracer inventory."); finite(residual, "Nonfinite tracer mass ledger."); finite(mass_scale, "Nonfinite tracer ledger scale.");
            if (std::abs(residual) > 1e-9 * mass_scale) throw std::runtime_error("Tracer mass ledger exceeds numerical tolerance.");
            r.diagnostics.max_mass_balance_residual_mg = std::max(r.diagnostics.max_mass_balance_residual_mg, std::abs(residual));
            field.swap(next); time = next_time; ++r.diagnostics.substeps;
        }
        append_frame(r, field, inventory, initial_inventory, ledger);
    }
    return r;
}

} // namespace breakwater

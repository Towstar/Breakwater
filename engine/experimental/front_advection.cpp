#include "front_advection.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace breakwater::front_experiment {
namespace {
bool nonnegative(double v) { return std::isfinite(v) && v >= 0.0; }
void finite(double v, const char* message) {
    if (!std::isfinite(v)) throw std::runtime_error(message);
}
struct Sum {
    double value = 0.0, correction = 0.0;
    void add(double term) {
        const double adjusted = term - correction, next = value + adjusted;
        correction = (next - value) - adjusted; value = next;
    }
};
double mass(std::span<const double> field, double liters) {
    Sum sum; for (double c : field) sum.add(c * liters);
    finite(sum.value, "Nonfinite experimental inventory."); return sum.value;
}
double residual(double inventory, double initial, double inlet, double outlet) {
    const double scale = std::max({1.0, std::abs(inventory), std::abs(initial), std::abs(inlet), std::abs(outlet)});
    Sum sum; for (double term : {inventory, -initial, -inlet, outlet}) sum.add(term / scale);
    const double value = sum.value * scale;
    finite(value, "Nonfinite experimental mass balance."); return value;
}
std::vector<double> output_grid(const PipeConfig& p, const TransportConfig& c) {
    std::vector<double> grid{0.0};
    for (std::size_t k = 1; k < max_transport_frames; ++k) {
        const double multiple = static_cast<double>(k) * c.output_step_h;
        const double next = multiple < c.horizon_h ? multiple : c.horizon_h;
        if (next <= grid.back()) throw std::runtime_error("Experimental output clock cannot advance.");
        grid.push_back(next);
        if (grid.size() > max_transport_values / p.cells)
            throw std::invalid_argument("Experimental output exceeds four million concentration values.");
        if (next == c.horizon_h) return grid;
    }
    throw std::invalid_argument("Experimental output exceeds 10001 frames.");
}
void validate(const PipeConfig& p, std::span<const double> initial, std::span<const FlowKnot> flow,
              std::span<const InletKnot> inlet, const TransportConfig& c, std::span<const double> source) {
    if (!std::isfinite(p.length_m) || p.length_m <= 0 || !std::isfinite(p.area_m2) || p.area_m2 <= 0 ||
        p.cells < 2 || p.cells > max_transport_cells || p.dispersion_m2_h != 0 || p.decay_per_h != 0)
        throw std::invalid_argument("Experimental pure advection requires positive geometry, 2..4096 cells and zero dispersion/decay.");
    if (!std::isfinite(c.horizon_h) || c.horizon_h <= 0 || c.horizon_h > 24 ||
        !std::isfinite(c.output_step_h) || c.output_step_h <= 0 ||
        !std::isfinite(c.cfl_safety) || c.cfl_safety <= 0 || c.cfl_safety > .5 ||
        c.boundary != TransportBoundary::dirichlet_inlet || c.max_substeps == 0 || c.max_substeps > max_transport_substeps)
        throw std::invalid_argument("Experimental request needs open inlet, CFL in (0,.5], valid clocks and existing step budget.");
    if (initial.size() != p.cells || std::any_of(initial.begin(), initial.end(), [](double v) { return !nonnegative(v); }))
        throw std::invalid_argument("Experimental initial data must be finite nonnegative cell averages.");
    if ((!source.empty() && source.size() != p.cells) ||
        std::any_of(source.begin(), source.end(), [](double v) { return v != 0; }))
        throw std::invalid_argument("Experimental source must be empty or an all-zero cell profile.");
    if (flow.empty() || flow.size() > max_transport_frames || flow.front().time_h != 0 ||
        inlet.empty() || inlet.size() > max_transport_frames || inlet.front().time_h != 0)
        throw std::invalid_argument("Experimental schedules must start at zero with 1..10001 knots.");
    for (std::size_t i = 0; i < flow.size(); ++i)
        if (!nonnegative(flow[i].time_h) || !nonnegative(flow[i].flow_m3h) || (i && flow[i].time_h <= flow[i-1].time_h))
            throw std::invalid_argument("Invalid experimental flow schedule.");
    for (std::size_t i = 0; i < inlet.size(); ++i)
        if (!nonnegative(inlet[i].time_h) || !nonnegative(inlet[i].concentration_mg_l) || (i && inlet[i].time_h <= inlet[i-1].time_h))
            throw std::invalid_argument("Invalid experimental inlet schedule.");
}
double inlet_at(std::span<const InletKnot> inlet, std::size_t index, double t) {
    while (index + 1 < inlet.size() && t >= inlet[index+1].time_h) ++index;
    if (index + 1 == inlet.size() || t <= inlet[index].time_h) return inlet[index].concentration_mg_l;
    const double weight = (t-inlet[index].time_h)/(inlet[index+1].time_h-inlet[index].time_h);
    return (1-weight)*inlet[index].concentration_mg_l + weight*inlet[index+1].concentration_mg_l;
}
void check_field(std::span<const double> field, Result& r) {
    for (double c : field) {
        finite(c, "Nonfinite experimental stage concentration.");
        if (c < -5e-12*std::max(1.0, std::abs(c)))
            throw std::runtime_error("Materially negative experimental stage; values are not clipped.");
        r.transport.diagnostics.minimum_concentration_mg_l = std::min(r.transport.diagnostics.minimum_concentration_mg_l, c);
        r.maximum_concentration_mg_l = std::max(r.maximum_concentration_mg_l, c);
    }
}
void append(Result& r, const std::vector<double>& field, double inventory, double initial, double inlet, double outlet) {
    auto& t = r.transport;
    const double error = residual(inventory, initial, inlet, outlet);
    t.concentration_mg_l.push_back(field); t.inventory_mg.push_back(inventory);
    t.cumulative_inlet_mg.push_back(inlet); t.cumulative_outlet_mg.push_back(outlet);
    t.cumulative_source_mg.push_back(0); t.cumulative_decay_mg.push_back(0);
    t.mass_balance_residual_mg.push_back(error);
}
} // namespace

double mc_slope(double left, double right) {
    finite(left, "Nonfinite MC difference."); finite(right, "Nonfinite MC difference.");
    if (left == 0 || right == 0 || std::signbit(left) != std::signbit(right)) return 0;
    const double a = 2*std::abs(left), b = .5*std::abs(left) + .5*std::abs(right), c = 2*std::abs(right);
    finite(a, "MC difference overflow."); finite(b, "MC difference overflow."); finite(c, "MC difference overflow.");
    return std::copysign(std::min({a,b,c}), left);
}
std::vector<double> face_fluxes(std::span<const double> field, double velocity, double inlet) {
    if (field.size() < 2 || !nonnegative(velocity) || !nonnegative(inlet))
        throw std::invalid_argument("Invalid experimental face data.");
    std::vector<double> flux(field.size()+1);
    flux.front() = velocity*inlet;
    for (std::size_t j = 0; j < field.size(); ++j) {
        finite(field[j], "Nonfinite experimental face concentration.");
        const double slope = (j == 0 || j+1 == field.size()) ? 0 :
            mc_slope(field[j]-field[j-1], field[j+1]-field[j]);
        flux[j+1] = velocity*(field[j]+.5*slope);
    }
    for (double f : flux) finite(f, "Nonfinite experimental face flux.");
    return flux;
}
void check_step_budget(std::size_t completed, std::size_t cells, std::size_t max_steps) {
    if (cells < 2 || cells > max_transport_cells || max_steps == 0 || max_steps > max_transport_substeps)
        throw std::invalid_argument("Invalid experimental budget domain.");
    if (completed >= max_steps || completed >= max_transport_cell_updates/(2*cells))
        throw std::runtime_error("Experimental substep/cell-stage-update budget exhausted.");
}
Result advect(const PipeConfig& p, std::span<const double> initial, std::span<const FlowKnot> flow,
              std::span<const InletKnot> inlet, TransportConfig config, std::span<const double> source) {
    validate(p, initial, flow, inlet, config, source);
    Result r; auto& result = r.transport; result.solver_version = solver_version;
    result.time_h = output_grid(p, config);
    const double dx = p.length_m/static_cast<double>(p.cells), inverse_dx = 1/dx;
    const double liters = (p.area_m2*dx)*1000, boundary_scale = p.area_m2*1000, pipe_volume = p.area_m2*p.length_m;
    for (double v : {dx,inverse_dx,liters,boundary_scale,pipe_volume}) {
        finite(v, "Nonfinite experimental geometry/unit conversion.");
        if (v <= 0) throw std::runtime_error("Experimental geometry/unit conversion underflow.");
    }
    for (std::size_t j = 0; j < p.cells; ++j) result.x_m.push_back((static_cast<double>(j)+.5)*dx);
    std::vector<double> field(initial.begin(), initial.end()), stage(p.cells), second(p.cells), next(p.cells);
    result.diagnostics.minimum_concentration_mg_l = *std::min_element(field.begin(), field.end());
    check_field(field, r);
    const double initial_mass = mass(field, liters);
    Sum input, output;
    append(r, field, initial_mass, initial_mass, 0, 0);
    std::size_t flow_index = 0, inlet_index = 0;
    double t = 0;
    for (std::size_t frame = 1; frame < result.time_h.size(); ++frame) {
        const double output_time = result.time_h[frame];
        while (t < output_time) {
            check_step_budget(result.diagnostics.substeps, p.cells, config.max_substeps);
            while (flow_index+1 < flow.size() && flow[flow_index+1].time_h <= t) ++flow_index;
            while (inlet_index+1 < inlet.size() && inlet[inlet_index+1].time_h <= t) ++inlet_index;
            // Freeze this event interval's velocity in BOTH stages. At a knot the
            // right-continuous next interval starts only after this step ends.
            const double velocity = flow[flow_index].flow_m3h/p.area_m2, rate = velocity*inverse_dx;
            finite(velocity, "Nonfinite experimental velocity."); finite(rate, "Nonfinite experimental stability rate.");
            double event = output_time;
            if (flow_index+1 < flow.size()) event = std::min(event, flow[flow_index+1].time_h);
            if (inlet_index+1 < inlet.size()) event = std::min(event, inlet[inlet_index+1].time_h);
            const double stable_dt = rate > 0 ? config.cfl_safety/rate : std::numeric_limits<double>::infinity();
            double end = event-t <= stable_dt ? event : std::min(event, t+stable_dt);
            if ((end-t)*rate > config.cfl_safety) end = std::nextafter(end,t);
            if (!std::isfinite(end) || end <= t) throw std::runtime_error("Experimental time step cannot advance at double precision.");
            const double dt = end-t, cfl = dt*rate;
            if (!std::isfinite(cfl) || cfl > config.cfl_safety) throw std::runtime_error("Experimental rounded step exceeds CFL.");
            result.diagnostics.max_cfl_number = std::max(result.diagnostics.max_cfl_number, cfl);
            const auto f0 = face_fluxes(field, velocity, inlet_at(inlet,inlet_index,t));
            for (std::size_t j = 0; j < p.cells; ++j) stage[j] = field[j]+(dt*inverse_dx)*(f0[j]-f0[j+1]);
            check_field(stage,r);
            const auto f1 = face_fluxes(stage, velocity, inlet_at(inlet,inlet_index,end));
            for (std::size_t j = 0; j < p.cells; ++j) {
                second[j] = stage[j]+(dt*inverse_dx)*(f1[j]-f1[j+1]);
                next[j] = .5*field[j]+.5*second[j];
            }
            check_field(second,r); check_field(next,r);
            // Weight stage fluxes before accumulating, avoiding a needless f0+f1 overflow.
            input.add((dt*boundary_scale)*(.5*f0.front()+.5*f1.front()));
            output.add((dt*boundary_scale)*(.5*f0.back()+.5*f1.back()));
            finite(input.value, "Nonfinite experimental inlet ledger."); finite(output.value, "Nonfinite experimental outlet ledger.");
            const double inventory = mass(next,liters), error = residual(inventory,initial_mass,input.value,output.value);
            const double scale = std::max({1.0,std::abs(inventory),std::abs(initial_mass),std::abs(input.value),std::abs(output.value)});
            if (std::abs(error) > 1e-9*scale) throw std::runtime_error("Experimental mass ledger exceeds existing tolerance.");
            result.diagnostics.max_mass_balance_residual_mg = std::max(result.diagnostics.max_mass_balance_residual_mg,std::abs(error));
            ++result.diagnostics.substeps; r.stage_evaluations += 2; r.cell_stage_updates += 2*p.cells;
            field.swap(next); t = end;
        }
        append(r,field,mass(field,liters),initial_mass,input.value,output.value);
    }
    return r;
}
} // namespace breakwater::front_experiment

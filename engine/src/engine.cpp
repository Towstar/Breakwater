#include "breakwater/engine.hpp"
#include "forecast_kernels.hpp"
#include "storage_grid.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace breakwater {
namespace {
constexpr double atol = 1e-9;
bool close(double a, double b) {
    return std::isfinite(a) && std::isfinite(b) &&
        std::abs(a - b) <= atol + 1e-10 * std::max(std::abs(a), std::abs(b));
}
const Asset* find_asset(const Network& n, const std::string& id) {
    const auto it = std::find_if(n.assets.begin(), n.assets.end(), [&](const Asset& a) { return a.id == id; });
    return it == n.assets.end() ? nullptr : &*it;
}
double flow(const Network& n, const Route& r) {
    const auto* source = find_asset(n, r.source_id);
    return source && source->kind == AssetKind::source ? source->inflow_m3h - source->curtailment_m3h : r.setpoint_m3h;
}
void check_config(SimulationConfig c) {
    detail::check_storage_config(c);
}
}

std::string kind_name(AssetKind kind) {
    switch (kind) {
    case AssetKind::tank: return "tank";
    case AssetKind::source: return "source";
    case AssetKind::treatment: return "treatment";
    case AssetKind::trucking: return "trucking";
    }
    throw std::invalid_argument("Unknown asset kind.");
}
std::optional<AssetKind> parse_kind(const std::string& name) {
    if (name == "tank") return AssetKind::tank;
    if (name == "source") return AssetKind::source;
    if (name == "treatment") return AssetKind::treatment;
    if (name == "trucking") return AssetKind::trucking;
    return std::nullopt;
}

std::vector<Issue> inspect_network(const Network& n) {
    std::vector<Issue> issues;
    auto add = [&](const std::string& code, const std::string& id, const std::string& msg) { issues.push_back({code, id, msg, {}}); };
    if (n.assets.size() > 128 || n.routes.size() > 256) {
        add("size_limit", "", "Base supports at most 128 assets and 256 routes.");
        return issues;
    }
    std::unordered_set<std::string> ids;
    std::unordered_map<std::string, std::size_t> source_routes;
    std::size_t tanks = 0;
    for (const auto& a : n.assets) {
        if (a.id.empty() || !ids.insert(a.id).second) add("duplicate_id", a.id, "Asset IDs must be nonempty and unique.");
        const double values[] = {a.capacity_m3, a.initial_m3, a.inflow_m3h, a.curtailment_m3h, a.curtailment_max_m3h, a.cost_per_m3};
        if (std::any_of(std::begin(values), std::end(values), [](double v) { return !std::isfinite(v) || v < 0.0; }))
            add("invalid_number", a.id, "Parameters must be finite and nonnegative.");
        if (a.kind == AssetKind::tank) {
            ++tanks;
            if (a.capacity_m3 <= 0.0 || a.initial_m3 > a.capacity_m3) add("invalid_inventory", a.id, "Tank needs positive capacity and initial inventory within capacity.");
        } else if (a.kind == AssetKind::source) {
            if (a.curtailment_max_m3h > a.inflow_m3h || a.curtailment_m3h > a.curtailment_max_m3h)
                add("curtailment_bounds", a.id, "Curtailment must be within its maximum, which cannot exceed production.");
        } else if ((a.kind == AssetKind::treatment || a.kind == AssetKind::trucking) && a.capacity_m3 <= 0.0) {
            add("destination_capacity", a.id, "Destination throughput capacity must be positive.");
        }
    }
    if (tanks == 0) add("no_storage", "", "Add at least one storage tank.");
    for (const auto& r : n.routes) {
        if (r.id.empty() || !ids.insert(r.id).second) add("duplicate_id", r.id, "Route and asset IDs must be globally unique.");
        const auto* s = find_asset(n, r.source_id);
        const auto* d = find_asset(n, r.target_id);
        if (!s || !d) { add("missing_endpoint", r.id, "Connect both endpoints to existing assets."); continue; }
        const bool supported = (s->kind == AssetKind::source && d->kind == AssetKind::tank) ||
            (s->kind == AssetKind::tank && (d->kind == AssetKind::tank || d->kind == AssetKind::treatment || d->kind == AssetKind::trucking));
        if (!supported || r.source_id == r.target_id) add("connection_type", r.id, "Unsupported connection direction or self-loop.");
        if (s->kind == AssetKind::source) {
            ++source_routes[s->id];
            if (r.has_pump || !r.available) add("source_route", r.id, "Source inflow routes cannot contain a pump or be unavailable.");
        }
        if (!std::isfinite(r.capacity_m3h) || !std::isfinite(r.setpoint_m3h) || !std::isfinite(r.cost_per_m3) ||
            r.capacity_m3h < 0.0 || r.setpoint_m3h < 0.0 || r.cost_per_m3 < 0.0)
            add("invalid_number", r.id, "Route parameters must be finite and nonnegative.");
        if (s->kind == AssetKind::tank && d->kind != AssetKind::tank &&
            std::isfinite(r.cost_per_m3) && std::isfinite(d->cost_per_m3) &&
            !std::isfinite(r.cost_per_m3 + d->cost_per_m3))
            add("cost_overflow", r.id, "Combined route and destination unit cost exceeds finite double precision.");
        if (flow(n, r) > r.capacity_m3h + atol) add("route_capacity", r.id, "Flow exceeds this route's capacity.");
        if (!r.available && (!r.has_pump || r.setpoint_m3h != 0.0)) add("unavailable_route", r.id, "Unavailable pumps must have zero requested flow.");
    }
    for (const auto& a : n.assets)
        if (a.kind == AssetKind::source && source_routes[a.id] != 1) add("source_connection", a.id, "Each source must feed exactly one tank.");
    return issues;
}

std::vector<ControlDescriptor> control_catalog(const Network& n) {
    std::vector<ControlDescriptor> controls;
    for (const auto& a : n.assets)
        if (a.kind == AssetKind::source) {
            if (!std::isfinite(a.cost_per_m3))
                throw std::invalid_argument("Control catalog requires a finite source unit cost: " + a.id);
            controls.push_back({"source:" + a.id + ":curtail", a.id, "m3/h", a.curtailment_m3h, 0.0, a.curtailment_max_m3h, a.cost_per_m3});
        }
    for (const auto& r : n.routes) {
        const auto* s = find_asset(n, r.source_id);
        const auto* d = find_asset(n, r.target_id);
        if (s && s->kind == AssetKind::tank) {
            const double unit_cost = r.cost_per_m3 + (d && d->kind != AssetKind::tank ? d->cost_per_m3 : 0.0);
            if (!std::isfinite(unit_cost))
                throw std::invalid_argument("Control catalog requires a finite combined route/destination unit cost: " + r.id);
            controls.push_back({"route:" + r.id + ":flow", r.id, "m3/h", r.setpoint_m3h, 0.0, r.available ? r.capacity_m3h : 0.0,
                unit_cost});
        }
    }
    return controls;
}

CompiledNetwork compile(const Network& input, std::span<const ControlValue> overrides) {
    const auto initial_issues = inspect_network(input);
    if (!initial_issues.empty()) throw std::invalid_argument(initial_issues.front().asset_id + ": " + initial_issues.front().message);
    Network n = input;
    const auto catalog = control_catalog(n);
    std::unordered_set<std::string> seen;
    for (const auto& override_value : overrides) {
        const auto it = std::find_if(catalog.begin(), catalog.end(), [&](const ControlDescriptor& d) { return d.id == override_value.id; });
        if (it == catalog.end() || !seen.insert(override_value.id).second || !std::isfinite(override_value.value) ||
            override_value.value < it->minimum || override_value.value > it->maximum)
            throw std::invalid_argument("Unknown, duplicated, nonfinite or out-of-bounds control: " + override_value.id);
        for (auto& a : n.assets) if (a.kind == AssetKind::source && override_value.id == "source:" + a.id + ":curtail") a.curtailment_m3h = override_value.value;
        for (auto& r : n.routes) if (override_value.id == "route:" + r.id + ":flow") r.setpoint_m3h = override_value.value;
    }
    const auto issues = inspect_network(n);
    if (!issues.empty()) throw std::invalid_argument(issues.front().asset_id + ": " + issues.front().message);
    CompiledNetwork c;
    c.network = n;
    std::unordered_map<std::string, std::size_t> tank_index;
    for (const auto& a : n.assets) {
        if (a.kind == AssetKind::tank) {
            tank_index.emplace(a.id, c.tank_ids.size());
            c.tank_ids.push_back(a.id);
            c.initial_m3.push_back(a.initial_m3);
            c.capacity_m3.push_back(a.capacity_m3);
            c.external_in_m3h.push_back(0.0);
            c.external_out_m3h.push_back(0.0);
        } else if (a.kind == AssetKind::source) c.cost_per_h += a.curtailment_m3h * a.cost_per_m3;
    }
    for (const auto& r : n.routes) {
        const auto& s = *find_asset(n, r.source_id);
        const auto& d = *find_asset(n, r.target_id);
        const double q = flow(n, r);
        c.cost_per_h += q * r.cost_per_m3;
        if (s.kind == AssetKind::source) c.external_in_m3h[tank_index.at(d.id)] += q;
        else if (d.kind == AssetKind::tank) c.edges.push_back({tank_index.at(s.id), tank_index.at(d.id), q});
        else {
            c.external_out_m3h[tank_index.at(s.id)] += q;
            c.cost_per_h += q * d.cost_per_m3;
        }
    }
    return c;
}

void evaluate_endpoints(const CompiledNetwork& c, double horizon_h, std::span<double> output) {
    if (!std::isfinite(horizon_h) || horizon_h < 0.0 || output.size() != c.tank_ids.size())
        throw std::invalid_argument("Endpoint workspace size or horizon is invalid.");
    for (std::size_t i = 0; i < output.size(); ++i)
        output[i] = c.initial_m3[i] + horizon_h * (c.external_in_m3h[i] - c.external_out_m3h[i]);
    for (const auto& e : c.edges) {
        output[e.source] -= horizon_h * e.flow_m3h;
        output[e.target] += horizon_h * e.flow_m3h;
    }
}

SimulationResult simulate(const CompiledNetwork& c, SimulationConfig config, Solver solver) {
    check_config(config);
    SimulationResult r;
    r.tank_ids = c.tank_ids;
    r.solver = solver;
    r.time_h = detail::storage_sample_times(config);
    const auto steps = r.time_h.size() - 1;
    r.volumes_m3.assign(c.tank_ids.size(), std::vector<double>(steps + 1));
    std::vector<double> state = c.initial_m3;
    std::vector<double> rates(state.size());
    std::vector<double> incidence;
    const detail::StorageRatesView rates_view{c.external_in_m3h, c.external_out_m3h, c.edges};
    if (solver == Solver::dense_reference) {
        incidence.resize(state.size() * c.edges.size());
        detail::build_incidence(rates_view, incidence);
    }
    for (std::size_t i = 0; i < state.size(); ++i) r.volumes_m3[i][0] = state[i];
    for (std::size_t k = 1; k <= steps; ++k) {
        if (solver == Solver::affine) evaluate_endpoints(c, r.time_h[k], state);
        else {
            if (solver == Solver::dense_reference) detail::dense_rates(rates_view, incidence, rates);
            else if (solver == Solver::sparse) detail::sparse_rates(rates_view, rates);
            else for (std::size_t i = 0; i < rates.size(); ++i)
                rates[i] = c.external_in_m3h[i] - c.external_out_m3h[i];
            const double dt = r.time_h[k] - r.time_h[k - 1];
            detail::euler_update(state, rates, dt);
        }
        for (std::size_t i = 0; i < state.size(); ++i) {
            if (!std::isfinite(state[i])) throw std::runtime_error("Nonfinite simulated volume.");
            r.volumes_m3[i][k] = state[i];
        }
    }
    r.cost = config.horizon_h * c.cost_per_h;
    return r;
}

ValidationReport validate(const CompiledNetwork& c, const SimulationResult& r, SimulationConfig config) {
    check_config(config);
    ValidationReport report;
    report.minimum_headroom_m3 = std::numeric_limits<double>::infinity();
    auto add = [&](const std::string& code, const std::string& id, const std::string& msg, std::optional<double> time = {}) {
        report.violations.push_back({code, id, msg, time});
    };
    if (r.tank_ids != c.tank_ids || r.time_h.size() < 2 || r.volumes_m3.size() != c.tank_ids.size() || r.time_h.front() != 0.0) {
        add("result_shape", "", "Result shape/identity/time origin does not match the candidate.");
        return report;
    }
    for (const auto& series : r.volumes_m3) if (series.size() != r.time_h.size()) {
        add("result_shape", "", "Trajectory sample counts differ."); return report;
    }
    for (std::size_t k = 0; k < r.time_h.size(); ++k)
        if (!std::isfinite(r.time_h[k]) || (k > 0 && r.time_h[k] <= r.time_h[k - 1])) {
            add("result_time", "", "Sample times must be finite and strictly increasing."); return report;
        }
    const double horizon = config.horizon_h;
    if (r.time_h.back() != horizon) {
        add("forecast_horizon", "", "Result does not cover the requested forecast horizon."); return report;
    }
    // Deliberately recompute rates/cost from ID-based source data, not compiled sparse arrays.
    std::unordered_map<std::string, double> rates;
    std::unordered_map<std::string, double> arrivals;
    double expected_cost_per_h = 0.0;
    for (const auto& a : c.network.assets) {
        if (a.kind == AssetKind::tank) rates[a.id] = 0.0;
        if (a.kind == AssetKind::source) expected_cost_per_h += a.curtailment_m3h * a.cost_per_m3;
    }
    for (const auto& route : c.network.routes) {
        const auto& s = *find_asset(c.network, route.source_id);
        const auto& d = *find_asset(c.network, route.target_id);
        const double q = flow(c.network, route);
        // Independently enforce the catalog's representable unit-price domain,
        // even when a zero/tiny flow would make the actual charge finite.
        if (s.kind == AssetKind::tank && d.kind != AssetKind::tank &&
            !std::isfinite(route.cost_per_m3 + d.cost_per_m3))
            add("cost_overflow", route.id, "Combined route and destination unit cost exceeds finite double precision.");
        expected_cost_per_h += q * route.cost_per_m3;
        if (s.kind == AssetKind::tank) rates[s.id] -= q;
        if (d.kind == AssetKind::tank) rates[d.id] += q;
        else { arrivals[d.id] += q; expected_cost_per_h += q * d.cost_per_m3; }
    }
    for (const auto& a : c.network.assets) {
        if (a.kind == AssetKind::treatment || a.kind == AssetKind::trucking) {
            if (arrivals[a.id] > a.capacity_m3 + atol) add("destination_throughput", a.id, "Combined incoming flow exceeds destination capacity (m3/h).", 0.0);
            continue;
        }
        if (a.kind != AssetKind::tank) continue;
        const double rate = rates[a.id];
        const double end = a.initial_m3 + horizon * rate;
        report.minimum_headroom_m3 = std::min(report.minimum_headroom_m3, a.capacity_m3 - std::max(a.initial_m3, end));
        if (rate > 0.0) {
            const double time = (a.capacity_m3 - a.initial_m3) / rate;
            if (time <= horizon + atol) report.limits.push_back({a.id, "capacity", time});
            if (end > a.capacity_m3 + atol) add("storage_capacity", a.id, "Predicted volume exceeds capacity.", time);
        } else if (rate < 0.0) {
            const double time = -a.initial_m3 / rate;
            if (time <= horizon + atol) report.limits.push_back({a.id, "empty", time});
            if (end < -atol) add("negative_inventory", a.id, "Requested discharge drains the tank below zero.", time);
        }
        const auto it = std::find(r.tank_ids.begin(), r.tank_ids.end(), a.id);
        const auto index = static_cast<std::size_t>(std::distance(r.tank_ids.begin(), it));
        for (std::size_t k = 0; k < r.time_h.size(); ++k)
            if (!std::isfinite(r.volumes_m3[index][k]) || !close(r.volumes_m3[index][k], a.initial_m3 + r.time_h[k] * rate)) {
                add("reference_mismatch", a.id, "Trajectory differs from independent constant-rate reference.", r.time_h[k]); break;
            }
    }
    double external_rate = 0.0;
    for (const auto& [id, rate] : rates) { (void)id; external_rate += rate; }
    for (std::size_t k = 1; k < r.time_h.size(); ++k) {
        double change = 0.0;
        for (const auto& series : r.volumes_m3) change += series[k] - series[k - 1];
        const double residual = std::abs(change - (r.time_h[k] - r.time_h[k - 1]) * external_rate);
        if (!std::isfinite(residual)) { add("nonfinite_result", "", "Nonfinite conservation residual.", r.time_h[k]); break; }
        report.max_conservation_residual_m3 = std::max(report.max_conservation_residual_m3, residual);
    }
    if (report.max_conservation_residual_m3 > 1e-7) add("conservation", "", "Volume balance residual exceeds 1e-7 m3.");
    const double expected_cost = horizon * expected_cost_per_h;
    if (!std::isfinite(expected_cost_per_h) || !std::isfinite(expected_cost))
        add("cost_overflow", "", "Independent hourly or forecast cost exceeds finite double precision.");
    if (!std::isfinite(r.cost) || r.cost < 0.0 || !close(r.cost, expected_cost))
        add("cost_mismatch", "", "Cost differs from independent finite nonnegative calculation.");
    std::sort(report.limits.begin(), report.limits.end(), [](const LimitEvent& a, const LimitEvent& b) { return a.time_h < b.time_h; });
    report.feasible = report.violations.empty();
    return report;
}

} // namespace breakwater

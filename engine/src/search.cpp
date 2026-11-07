#include "breakwater/search.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <unordered_map>

namespace breakwater {
namespace {
constexpr auto absent = std::numeric_limits<std::size_t>::max();
constexpr double constraint_tolerance = 1e-9;

struct AssetRef { AssetKind kind; std::size_t slot; };
struct SourceData {
    double production, curtailment, cost;
    std::size_t feed_route = absent;
};
struct RouteData {
    std::size_t source_tank = absent, target_tank = absent;
    std::size_t source_slot = absent, destination_slot = absent;
    double setpoint = 0.0, capacity = 0.0, route_cost = 0.0, destination_cost = 0.0;
};
enum class ControlKind { curtailment, route_flow };
struct Binding { ControlDescriptor descriptor; ControlKind kind; std::size_t slot; };

struct Prepared {
    CompiledNetwork baseline;
    std::vector<SourceData> sources;
    std::vector<RouteData> routes;
    std::vector<double> destination_capacities;
    std::vector<Binding> bindings;
    std::unordered_map<std::string, std::size_t> control_index;

    explicit Prepared(const Network& input) : baseline(compile(input)) {
        std::unordered_map<std::string, AssetRef> assets;
        std::unordered_map<std::string, std::size_t> route_index;
        std::size_t tank_count = 0;
        for (const auto& a : baseline.network.assets) {
            if (a.kind == AssetKind::tank) assets.emplace(a.id, AssetRef{a.kind, tank_count++});
            else if (a.kind == AssetKind::source) {
                assets.emplace(a.id, AssetRef{a.kind, sources.size()});
                sources.push_back({a.inflow_m3h, a.curtailment_m3h, a.cost_per_m3});
            } else if (a.kind == AssetKind::treatment || a.kind == AssetKind::trucking) {
                assets.emplace(a.id, AssetRef{a.kind, destination_capacities.size()});
                destination_capacities.push_back(a.capacity_m3);
            } else throw std::invalid_argument("Unknown asset kind in search snapshot.");
        }
        for (const auto& r : baseline.network.routes) {
            const auto source = assets.at(r.source_id), target = assets.at(r.target_id);
            RouteData route;
            route.setpoint = r.setpoint_m3h; route.capacity = r.capacity_m3h; route.route_cost = r.cost_per_m3;
            if (source.kind == AssetKind::source) {
                route.source_slot = source.slot;
                sources[source.slot].feed_route = routes.size();
            } else route.source_tank = source.slot;
            if (target.kind == AssetKind::tank) route.target_tank = target.slot;
            else {
                route.destination_slot = target.slot;
                for (const auto& a : baseline.network.assets) if (a.id == r.target_id) route.destination_cost = a.cost_per_m3;
            }
            route_index.emplace(r.id, routes.size()); routes.push_back(route);
        }
        for (const auto& descriptor : control_catalog(baseline.network)) {
            Binding binding;
            binding.descriptor = descriptor;
            const auto asset = assets.find(descriptor.asset_id);
            if (asset != assets.end() && asset->second.kind == AssetKind::source) {
                binding.kind = ControlKind::curtailment; binding.slot = asset->second.slot;
            } else {
                binding.kind = ControlKind::route_flow; binding.slot = route_index.at(descriptor.asset_id);
            }
            control_index.emplace(descriptor.id, bindings.size()); bindings.push_back(std::move(binding));
        }
    }

    std::size_t find_control(const std::string& id) const {
        const auto it = control_index.find(id);
        if (it == control_index.end()) throw std::invalid_argument("Unknown search control: " + id);
        return it->second;
    }
    void check_value(std::size_t index, double value) const {
        const auto& b = bindings[index];
        if (!std::isfinite(value) || value < b.descriptor.minimum || value > b.descriptor.maximum)
            throw std::invalid_argument("Nonfinite, unavailable or out-of-bounds control: " + b.descriptor.id);
        if (b.kind == ControlKind::curtailment) {
            const auto& source = sources[b.slot];
            if (source.production - value > routes[source.feed_route].capacity + constraint_tolerance)
                throw std::invalid_argument("Curtailment value exceeds the connected feed route's capacity: " + b.descriptor.id);
        }
    }
};

void check_options(const SearchOptions& options) {
    const auto c = options.config;
    if (!std::isfinite(c.horizon_h) || !std::isfinite(c.step_h) || c.horizon_h <= 0.0 ||
        c.horizon_h > 24.0 || c.step_h <= 0.0 || std::ceil(c.horizon_h / c.step_h) > 10000.0)
        throw std::invalid_argument("Search requires the engine's supported horizon and sample bounds.");
    if (options.result_limit == 0 || options.result_limit > max_search_recommendations)
        throw std::invalid_argument("Search result_limit must be between 1 and 5.");
}

using Values = std::array<double, max_search_controls>;
struct CandidateSet {
    std::vector<std::size_t> bindings;
    std::vector<Values> values;
};

struct Workspace {
    std::vector<double> curtailments, setpoints, rates, flow_magnitudes, destination_totals;
    explicit Workspace(const Prepared& p) : rates(p.baseline.tank_ids.size()), flow_magnitudes(rates.size()),
        destination_totals(p.destination_capacities.size()) {
        for (const auto& s : p.sources) curtailments.push_back(s.curtailment);
        for (const auto& r : p.routes) setpoints.push_back(r.setpoint);
    }
};
enum class Screen { potential, constraint_failure, numerical_failure };

// Match compile()'s source-then-route cost accumulation order. In particular, source
// curtailment also reduces the cost on its outgoing feed route. No allocation occurs
// in this loop; topology and workspace are reused across every candidate.
Screen screen_candidate(const Prepared& p, const CandidateSet& set, const Values& values,
                        double horizon, Workspace& w, double& cost) {
    for (std::size_t col = 0; col < set.bindings.size(); ++col) {
        const auto& b = p.bindings[set.bindings[col]];
        if (b.kind == ControlKind::curtailment) w.curtailments[b.slot] = values[col];
        else w.setpoints[b.slot] = values[col];
    }
    std::fill(w.rates.begin(), w.rates.end(), 0.0);
    std::fill(w.flow_magnitudes.begin(), w.flow_magnitudes.end(), 0.0);
    std::fill(w.destination_totals.begin(), w.destination_totals.end(), 0.0);
    double hourly_cost = 0.0;
    for (std::size_t s = 0; s < p.sources.size(); ++s) hourly_cost += w.curtailments[s] * p.sources[s].cost;
    for (std::size_t r = 0; r < p.routes.size(); ++r) {
        const auto& route = p.routes[r];
        const double q = route.source_slot != absent ? p.sources[route.source_slot].production - w.curtailments[route.source_slot] : w.setpoints[r];
        hourly_cost += q * route.route_cost;
        if (route.source_tank != absent) { w.rates[route.source_tank] -= q; w.flow_magnitudes[route.source_tank] += q; }
        if (route.target_tank != absent) { w.rates[route.target_tank] += q; w.flow_magnitudes[route.target_tank] += q; }
        else { w.destination_totals[route.destination_slot] += q; hourly_cost += q * route.destination_cost; }
    }
    cost = hourly_cost * horizon;
    if (!std::isfinite(cost)) return Screen::numerical_failure;
    bool bounds_pass = true;
    for (std::size_t i = 0; i < w.rates.size(); ++i) {
        const double end = p.baseline.initial_m3[i] + horizon * w.rates[i];
        const double scale = 1.0 + std::abs(p.baseline.initial_m3[i]) + horizon * w.flow_magnitudes[i];
        if (!std::isfinite(end)) return Screen::numerical_failure;
        // Borderline cases proceed to the independent validator, which retains its
        // original tolerances. This allowance never establishes feasibility itself.
        // An auxiliary absolute-flow sum can overflow even when reciprocal flows
        // cancel to a finite net rate. Defer that tank's bounds to the reference
        // validator rather than discard a potentially valid candidate.
        const double guard = std::isfinite(scale) ?
            constraint_tolerance + 32.0 * std::numeric_limits<double>::epsilon() * scale :
            std::numeric_limits<double>::infinity();
        if (end < -guard || end > p.baseline.capacity_m3[i] + guard) bounds_pass = false;
    }
    for (std::size_t i = 0; i < w.destination_totals.size(); ++i) {
        const double total = w.destination_totals[i];
        if (!std::isfinite(total)) return Screen::numerical_failure;
        const double guard = constraint_tolerance + 32.0 * std::numeric_limits<double>::epsilon() * (1.0 + total);
        if (total > p.destination_capacities[i] + guard) bounds_pass = false;
    }
    return bounds_pass ? Screen::potential : Screen::constraint_failure;
}

struct Survivor { std::size_t index; double cost; };

template<class Materialize>
SearchResult execute(const Prepared& p, const CandidateSet& set, const SearchOptions& options,
                     Materialize materialize) {
    SearchResult result;
    result.diagnostics.candidates_evaluated = set.values.size();
    Workspace workspace(p);
    std::vector<Survivor> survivors;
    survivors.reserve(set.values.size());
    for (std::size_t i = 0; i < set.values.size(); ++i) {
        double cost = 0.0;
        const auto screen = screen_candidate(p, set, set.values[i], options.config.horizon_h, workspace, cost);
        if (screen == Screen::potential) { survivors.push_back({i, cost}); ++result.diagnostics.screen_passes; }
        else if (screen == Screen::constraint_failure) ++result.diagnostics.constraint_screen_rejections;
        else ++result.diagnostics.numerical_screen_rejections;
    }
    std::sort(survivors.begin(), survivors.end(), [](const Survivor& a, const Survivor& b) {
        return a.cost < b.cost || (a.cost == b.cost && a.index < b.index);
    });
    result.recommendations.reserve(options.result_limit);
    for (const auto& survivor : survivors) {
        if (result.recommendations.size() == options.result_limit) break;
        RankedCandidate candidate;
        candidate.candidate_index = survivor.index;
        candidate.overrides = materialize(survivor.index);
        ++result.diagnostics.reference_checks;
        try {
            const auto compiled = compile(p.baseline.network, candidate.overrides);
            candidate.run = simulate(compiled, options.config, Solver::dense_reference);
            candidate.validation = validate(compiled, candidate.run, options.config);
        } catch (const std::runtime_error&) {
            ++result.diagnostics.reference_rejections; continue; // Nonfinite reference trajectory.
        }
        if (!candidate.validation.feasible) { ++result.diagnostics.reference_rejections; continue; }
        // A changed compiler/model cost rule must not silently invalidate pruning by
        // cost. Both implementations deliberately use the same operation ordering.
        if (candidate.run.cost != survivor.cost)
            throw std::logic_error("Search/reference cost disagreement; refusing to rank recommendations.");
        result.recommendations.push_back(std::move(candidate));
    }
    if (!result.recommendations.empty()) result.status = SearchStatus::recommendations_ready;
    return result;
}
}

SearchResult search_candidates(const Network& network, std::span<const SearchAxis> axes, SearchOptions options) {
    check_options(options);
    if (axes.size() > max_search_controls) throw std::invalid_argument("Search supports at most four varying controls.");
    std::size_t count = 1;
    for (const auto& axis : axes) {
        if (axis.values.empty()) throw std::invalid_argument("Every search axis needs explicit candidate values.");
        if (axis.values.size() > max_search_candidates / count) throw std::invalid_argument("Search exceeds 10000 combinations.");
        count *= axis.values.size();
    }
    const Prepared prepared(network);
    CandidateSet set;
    for (const auto& axis : axes) {
        const auto binding = prepared.find_control(axis.control_id);
        if (std::find(set.bindings.begin(), set.bindings.end(), binding) != set.bindings.end()) throw std::invalid_argument("Search axis control IDs must be unique.");
        for (const double value : axis.values) prepared.check_value(binding, value);
        set.bindings.push_back(binding);
    }
    set.values.resize(count);
    for (std::size_t i = 0; i < count; ++i) {
        auto index = i;
        for (std::size_t col = axes.size(); col > 0; --col) {
            const auto& values = axes[col - 1].values;
            set.values[i][col - 1] = values[index % values.size()]; index /= values.size();
        }
    }
    return execute(prepared, set, options, [&](std::size_t index) {
        std::vector<ControlValue> overrides; overrides.reserve(axes.size());
        for (std::size_t col = 0; col < axes.size(); ++col) overrides.push_back({axes[col].control_id, set.values[index][col]});
        return overrides;
    });
}

SearchResult evaluate_candidates(const Network& network, std::span<const CandidateControls> candidates, SearchOptions options) {
    check_options(options);
    if (candidates.empty() || candidates.size() > max_search_candidates) throw std::invalid_argument("Explicit batches need 1..10000 candidates.");
    const Prepared prepared(network);
    CandidateSet set;
    for (const auto& candidate : candidates) {
        if (candidate.overrides.size() > max_search_controls) throw std::invalid_argument("A candidate has more than four controls.");
        std::array<std::size_t, max_search_controls> used{};
        std::size_t used_count = 0;
        for (const auto& value : candidate.overrides) {
            const auto binding = prepared.find_control(value.id);
            if (std::find(used.begin(), used.begin() + static_cast<std::ptrdiff_t>(used_count), binding) != used.begin() + static_cast<std::ptrdiff_t>(used_count))
                throw std::invalid_argument("Candidate overrides must have unique control IDs.");
            used[used_count++] = binding;
            prepared.check_value(binding, value.value);
            if (std::find(set.bindings.begin(), set.bindings.end(), binding) == set.bindings.end()) {
                if (set.bindings.size() == max_search_controls) throw std::invalid_argument("A batch varies more than four distinct controls.");
                set.bindings.push_back(binding);
            }
        }
    }
    set.values.resize(candidates.size());
    for (std::size_t row = 0; row < candidates.size(); ++row) {
        for (std::size_t col = 0; col < set.bindings.size(); ++col) set.values[row][col] = prepared.bindings[set.bindings[col]].descriptor.value;
        for (const auto& value : candidates[row].overrides) {
            const auto binding = prepared.find_control(value.id);
            const auto col = static_cast<std::size_t>(std::distance(set.bindings.begin(), std::find(set.bindings.begin(), set.bindings.end(), binding)));
            set.values[row][col] = value.value;
        }
    }
    return execute(prepared, set, options, [&](std::size_t row) { return candidates[row].overrides; });
}

} // namespace breakwater

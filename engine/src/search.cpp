#include "breakwater/search.hpp"
#include "control_influence.hpp"
#include "search_evaluation.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>

namespace breakwater {
using namespace detail;
namespace {

struct Survivor { std::size_t index; double cost; };

struct RouteWalkPlan {
    const Prepared& prepared;
    const CandidateSet& candidates;
    struct workspace_type : Workspace {
        explicit workspace_type(const RouteWalkPlan& plan) : Workspace(plan.prepared) {}
    };
    Screen screen(const Values& values, double horizon, workspace_type& workspace, double& cost) const {
        return screen_candidate(prepared, candidates, values, horizon, workspace, cost);
    }
};

template<bool Precomputed, class Materialize>
SearchResult execute(const Prepared& p, const CandidateSet& set, const SearchOptions& options,
                     Materialize materialize) {
    SearchResult result;
    result.diagnostics.candidates_evaluated = set.values.size();
    using Plan = std::conditional_t<Precomputed, ControlInfluence, RouteWalkPlan>;
    const Plan plan{p, set};
    typename Plan::workspace_type workspace(plan);
    std::vector<Survivor> survivors;
    survivors.reserve(set.values.size());
    for (std::size_t i = 0; i < set.values.size(); ++i) {
        double cost = 0.0;
        const auto screen = plan.screen(set.values[i], options.config.horizon_h, workspace, cost);
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
    const auto materialize = [&](std::size_t index) {
        std::vector<ControlValue> overrides; overrides.reserve(axes.size());
        for (std::size_t col = 0; col < axes.size(); ++col) overrides.push_back({axes[col].control_id, set.values[index][col]});
        return overrides;
    };
    return use_control_influence(network.routes.size(), count) ?
        execute<true>(prepared, set, options, materialize) :
        execute<false>(prepared, set, options, materialize);
}

namespace {
template<bool Precomputed>
SearchResult evaluate_batch(const Network& network, std::span<const CandidateControls> candidates, SearchOptions options) {
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
    return execute<Precomputed>(prepared, set, options, [&](std::size_t row) { return candidates[row].overrides; });
}
} // namespace

SearchResult evaluate_candidates(const Network& network, std::span<const CandidateControls> candidates, SearchOptions options) {
    return use_control_influence(network.routes.size(), candidates.size()) ?
        evaluate_batch<true>(network, candidates, options) :
        evaluate_batch<false>(network, candidates, options);
}

SearchResult detail::evaluate_candidates_route_walk(const Network& network,
    std::span<const CandidateControls> candidates, SearchOptions options) {
    return evaluate_batch<false>(network, candidates, options);
}

} // namespace breakwater

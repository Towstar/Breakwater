#include "breakwater/search.hpp"
#include "../src/control_influence.hpp"
#include "../src/search_evaluation.hpp"
#include "../benchmarks/workloads.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace breakwater;

namespace {
void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

void near(double actual, double expected, const std::string& context) {
    const double tolerance = 1e-9 + 1e-10 * std::abs(expected);
    require(std::isfinite(actual) && std::isfinite(expected) &&
                std::abs(actual - expected) <= tolerance,
            context + ": expected " + std::to_string(expected) + ", got " + std::to_string(actual));
}

template<class Function> void rejects(Function&& function) {
    bool rejected = false;
    try { function(); } catch (const std::invalid_argument&) { rejected = true; }
    require(rejected, "Invalid request bypassed validation");
}

std::size_t check_numeric_differential(const Network& network,
    const std::vector<CandidateControls>& candidates, double horizon);

// Independent result oracle: every candidate is compiled, sampled with the dense
// engine, and validated. It does not call either screening implementation.
void check_public_oracle(const Network& network, const std::vector<CandidateControls>& candidates,
                         SearchOptions options = {{1.0, 0.037}, 5}) {
    // Force the prepared map even when public performance policy selects route
    // walk for a small fixture. Boundary/cancellation regressions must exercise
    // the optimization itself as well as public dispatch and the dense oracle.
    (void)check_numeric_differential(network, candidates, options.config.horizon_h);
    std::vector<std::pair<double, std::size_t>> expected;
    for (std::size_t row = 0; row < candidates.size(); ++row) {
        const auto compiled = compile(network, candidates[row].overrides);
        try {
            const auto run = simulate(compiled, options.config, Solver::dense_reference);
            if (validate(compiled, run, options.config).feasible) expected.emplace_back(run.cost, row);
        } catch (const std::runtime_error&) {
            // Nonfinite reference predictions are evidence of a rejected row.
        }
    }
    std::sort(expected.begin(), expected.end());
    expected.resize(std::min(expected.size(), options.result_limit));
    const auto result = evaluate_candidates(network, candidates, options);
    require(result.recommendations.size() == expected.size(), "Public recommendation count differs from exhaustive oracle");
    require(result.diagnostics.candidates_evaluated == candidates.size(), "Wrong evaluated candidate count");
    for (std::size_t rank = 0; rank < expected.size(); ++rank) {
        const auto& actual = result.recommendations[rank];
        require(actual.candidate_index == expected[rank].second, "Cost/index ranking differs from exhaustive reference");
        require(actual.run.cost == expected[rank].first, "Returned cost is not the exact reference cost");
        require(actual.run.solver == Solver::dense_reference && actual.validation.feasible,
                "Recommendation lacks independent reference validation");
    }
}

Network source_cost_network() {
    Network network;
    network.assets = {{"store:west", "West", AssetKind::tank, 1000, 100},
                      {"store:east", "East", AssetKind::tank, 1000, 100},
                      {"well:upstream", "Well", AssetKind::source, 0, 0, 10, 2, 10, 1},
                      {"plant:joint", "Plant", AssetKind::treatment, 12, 0, 0, 0, 0, 3}};
    network.routes = {{"feed:priced", "well:upstream", "store:west", 10, 10, false, true, 10},
                      {"move:internal", "store:west", "store:east", 10, 3, true, true, .25},
                      {"out:west", "store:west", "plant:joint", 10, 4, true, true, 2},
                      {"out:east", "store:east", "plant:joint", 10, 5, true, true, 4}};
    return network;
}

Network near_tie_network() {
    Network network;
    network.assets = {{"upstream", "Upstream", AssetKind::tank, 1000, 100},
                      {"downstream", "Downstream", AssetKind::tank, 1000, 100},
                      {"plant", "Plant", AssetKind::treatment, 20}};
    network.routes = {{"fixed", "upstream", "downstream", 1, 1, false, true, 0x1p53},
                      {"variable", "downstream", "plant", 20, 1, true, true, 1}};
    return network;
}

std::vector<CandidateControls> one_control_rows(const std::string& id, const std::vector<double>& values) {
    std::vector<CandidateControls> candidates;
    for (double value : values) candidates.push_back({{{id, value}}});
    return candidates;
}

detail::CandidateSet make_set(const detail::Prepared& prepared,
                             const std::vector<CandidateControls>& candidates) {
    detail::CandidateSet set;
    for (const auto& candidate : candidates) for (const auto& control : candidate.overrides) {
        const auto binding = prepared.find_control(control.id);
        prepared.check_value(binding, control.value);
        if (std::find(set.bindings.begin(), set.bindings.end(), binding) == set.bindings.end())
            set.bindings.push_back(binding);
    }
    require(set.bindings.size() <= max_search_controls, "Test fixture exceeds supported selected controls");
    set.values.resize(candidates.size());
    for (std::size_t row = 0; row < candidates.size(); ++row) {
        for (std::size_t col = 0; col < set.bindings.size(); ++col)
            set.values[row][col] = prepared.bindings[set.bindings[col]].descriptor.value;
        for (const auto& control : candidates[row].overrides) {
            const auto binding = prepared.find_control(control.id);
            const auto found = std::find(set.bindings.begin(), set.bindings.end(), binding);
            set.values[row][static_cast<std::size_t>(found - set.bindings.begin())] = control.value;
        }
    }
    return set;
}

std::size_t check_numeric_differential(const Network& network,
                                      const std::vector<CandidateControls>& candidates,
                                      double horizon = 3.0) {
    const detail::Prepared prepared(network);
    const auto set = make_set(prepared, candidates);
    const detail::ControlInfluence influence(prepared, set);
    detail::InfluenceWorkspace fast(influence);
    detail::Workspace reference(prepared);
    for (std::size_t row = 0; row < candidates.size(); ++row) {
        double old_cost = 0.0, new_cost = 0.0;
        const auto old_screen = detail::screen_candidate(prepared, set, set.values[row], horizon, reference, old_cost);
        const auto new_screen = influence.screen(set.values[row], horizon, fast, new_cost);
        if (old_screen == detail::Screen::potential)
            require(new_screen == detail::Screen::potential, "Influence screen discarded a route-walk potential candidate");
        if (new_screen == detail::Screen::potential) {
            const auto compiled = compile(network, candidates[row].overrides);
            require(new_cost == compiled.cost_per_h * horizon,
                    "Potential candidate cost differs bitwise from independently compiled cost");
            require(std::isfinite(new_cost), "Potential candidate has nonfinite cost");
        }
        if (old_screen == detail::Screen::potential && new_screen == detail::Screen::potential) {
            require(old_cost == new_cost, "Exact ordered cost changed on a screen survivor");
            require(fast.rates.size() == reference.rates.size(), "Wrong precomputed tank dimension");
            require(fast.destination_totals.size() == reference.destination_totals.size(), "Wrong destination dimension");
            for (std::size_t i = 0; i < reference.rates.size(); ++i)
                near(fast.rates[i], reference.rates[i], "Tank rate");
            for (std::size_t i = 0; i < reference.destination_totals.size(); ++i)
                near(fast.destination_totals[i], reference.destination_totals[i], "Shared destination total");
        }
    }
    return fast.fallback_count;
}

void check_public_implementations(const Network& network, const std::vector<CandidateControls>& candidates,
                                  SearchOptions options = {}) {
    const auto fast = evaluate_candidates(network, candidates, options);
    const auto old = detail::evaluate_candidates_route_walk(network, candidates, options);
    require(fast.status == old.status && fast.recommendations.size() == old.recommendations.size(),
            "Optimized and route-walk public evaluators disagree on result count/status");
    for (std::size_t i = 0; i < fast.recommendations.size(); ++i) {
        require(fast.recommendations[i].candidate_index == old.recommendations[i].candidate_index,
                "Optimized and route-walk public evaluators disagree on candidate rank");
        require(fast.recommendations[i].run.cost == old.recommendations[i].run.cost,
                "Optimized and route-walk evaluator costs differ");
    }
}
}

int main() {
    const std::vector<std::pair<std::string, std::function<void()>>> tests = {
        {"large public batch and Cartesian search retain exhaustive reference rankings", [] {
            const auto workload = bench::workloads().at(1);
            const auto batch = bench::candidates(workload, 2048);
            require(detail::use_control_influence(workload.network.routes.size(), batch.size()),
                    "Large-request fixture does not exercise the public influence dispatch");
            check_public_implementations(workload.network, batch);

            const std::vector<SearchAxis> axes = {
                {workload.varying_control_ids[0], {0, 1, 2, 3, 4, 5, 6, 7}},
                {workload.varying_control_ids[1], {0, 1, 2, 3, 4, 5, 6, 7}},
                {workload.varying_control_ids[2], {0, .5, 1, 1.5, 2, 2.5, 3, 3.5}},
                {workload.varying_control_ids[3], {0, 1, 2, 4}}};
            std::vector<CandidateControls> grid;
            for (double ring : axes[0].values) for (double treatment : axes[1].values)
                for (double trucking : axes[2].values) for (double curtailment : axes[3].values)
                    grid.push_back({{{axes[0].control_id, ring}, {axes[1].control_id, treatment},
                                     {axes[2].control_id, trucking}, {axes[3].control_id, curtailment}}});
            require(grid.size() == 2048, "Cartesian fixture has the wrong candidate count");
            const SearchOptions options{{3.0, .025}, 5};
            // Dense simulation of these 2,048 distinct settings is an independent
            // oracle for the batch route. Grid enumeration must retain that same
            // last-axis-fastest indexing and validated top-five order.
            check_public_oracle(workload.network, grid, options);
            const auto expected = detail::evaluate_candidates_route_walk(workload.network, grid, options);
            const auto actual = search_candidates(workload.network, axes, options);
            require(actual.diagnostics.candidates_evaluated == grid.size(), "Cartesian search lost candidates");
            require(actual.recommendations.size() == expected.recommendations.size() &&
                        actual.status == expected.status,
                    "Cartesian search differs from exhaustive reference result count/status");
            for (std::size_t i = 0; i < expected.recommendations.size(); ++i) {
                require(actual.recommendations[i].candidate_index == expected.recommendations[i].candidate_index &&
                            actual.recommendations[i].run.cost == expected.recommendations[i].run.cost &&
                            actual.recommendations[i].validation.feasible,
                        "Optimized Cartesian rank/cost differs from the native reference");
            }
        }},
        {"precomputed rates and destination sums match route traversal across actual network shapes", [] {
            for (const auto& workload : bench::workloads()) {
                const auto candidates = bench::candidates(workload, 1000);
                const auto fallback_count = check_numeric_differential(workload.network, candidates);
                require(fallback_count < candidates.size(), "Ordinary workload never exercises precomputed arithmetic");
                check_public_implementations(workload.network, candidates);
            }
        }},
        {"nonzero source feed cost and partial defaults agree at the numeric screen", [] {
            const auto network = source_cost_network();
            const std::vector<CandidateControls> candidates = {{},
                {{{"source:well:upstream:curtail", 8}}}, {{{"route:out:west:flow", 1}}},
                {{{"route:out:east:flow", 7}}}, {{{"route:move:internal:flow", 8}}},
                {{{"source:well:upstream:curtail", 0}, {"route:out:east:flow", 3}}}, {}};
            check_numeric_differential(network, candidates, .37);
            check_public_implementations(network, candidates, {{.37, .07}, 5});
        }},
        {"near-boundary predictions exercise the route-walk fallback", [] {
            Network network;
            network.assets = {{"tank", "Tank", AssetKind::tank, 1, 0},
                              {"source", "Source", AssetKind::source, 0, 0, 2, 1, 2}};
            network.routes = {{"feed", "source", "tank", 2, 0}};
            const auto candidates = one_control_rows("source:source:curtail", {1.0, 1.0 - 5e-10, 1.0 + 5e-10});
            require(check_numeric_differential(network, candidates, 1.0) > 0,
                    "Borderline capacity result did not use the general fallback");
        }},
        {"canonical and custom batch rankings agree with exhaustive native oracle", [] {
            for (const auto& workload : bench::workloads())
                check_public_oracle(workload.network, bench::candidates(workload, 127), {{3.0, .025}, 5});
        }},
        {"partial overrides keep other route and source settings fixed", [] {
            const auto network = source_cost_network();
            const std::vector<CandidateControls> candidates = {
                {}, {{{"source:well:upstream:curtail", 10}}}, {{{"route:out:west:flow", 0}}},
                {{{"route:move:internal:flow", 6}, {"route:out:east:flow", 3}}},
                {{{"source:well:upstream:curtail", 0}, {"route:out:west:flow", 8}}}, {},
                {{{"source:well:upstream:curtail", 5}, {"route:out:east:flow", 7}}}};
            check_public_oracle(network, candidates);
            require(network.assets[2].curtailment_m3h == 2 && network.routes[2].setpoint_m3h == 4,
                    "Search mutated immutable defaults");
        }},
        {"close reference costs and ties preserve exact ranking across the result cutoff", [] {
            const auto network = near_tie_network();
            auto candidates = one_control_rows("route:variable:flow", {7, 5, 6, 3, 4, 1, 2, 0, 1, 0, 2});
            // The captured M+1 rounds to M at M=2^53. A naive rebased cost
            // would then price u=2 as M instead of canonical M+2 and reorder ties.
            require(check_numeric_differential(network, candidates, 1.0) == 0,
                    "Cost-tie fixture did not exercise ordinary influence evaluation");
            check_public_oracle(network, candidates);
            check_public_oracle(network, candidates, {{3.0, .07}, 3});
        }},
        {"shared destination equality and near-boundary rows match independent validation", [] {
            for (auto kind : {AssetKind::treatment, AssetKind::trucking}) {
                auto network = source_cost_network();
                network.assets[3].kind = kind;
                network.assets[3].capacity_m3 = 9;
                const auto candidates = one_control_rows("route:out:west:flow", {
                    4.0 - 2e-9, 4.0 - 5e-10, 4.0, 4.0 + 5e-10, 4.0 + 2e-9, 8.0, 0.0});
                check_public_oracle(network, candidates);
            }
        }},
        {"storage equality and narrowed horizon preserve the reference feasibility decision", [] {
            Network network;
            network.assets = {{"tank", "Tank", AssetKind::tank, 1, 0},
                              {"source", "Source", AssetKind::source, 0, 0, 2, 1, 2}};
            network.routes = {{"feed", "source", "tank", 2, 0}};
            auto candidates = one_control_rows("source:source:curtail", {
                1.0 - 2e-9, 1.0 - 5e-10, 1.0, 1.0 + 5e-10, 1.0 + 2e-9, 0.0, 2.0});
            check_public_oracle(network, candidates);
            check_public_oracle(network, candidates, {{.37, .07}, 5});
        }},
        {"nonfinite baseline cost does not hide a finite overridden candidate", [] {
            Network network;
            network.assets = {{"tank", "Tank", AssetKind::tank, 100, 50},
                              {"plant", "Plant", AssetKind::treatment, 20}};
            network.routes = {{"priced", "tank", "plant", 20, 10, true, true, std::numeric_limits<double>::max()}};
            const auto candidates = one_control_rows("route:priced:flow", {10.0, 0.0, 1e-309, .25, 1.0});
            require(check_numeric_differential(network, candidates, 1.0) > 0,
                    "Overflow repair did not exercise the influence fallback");
            check_public_oracle(network, candidates);
        }},
        {"automatic influence dispatch rejects cost overflow but preserves finite repairs", [] {
            Network network;
            network.assets = {{"tank", "Tank", AssetKind::tank, 100, 50},
                              {"plant", "Plant", AssetKind::treatment, 20}};
            network.routes = {{"priced", "tank", "plant", 20, 10, true, true,
                               std::numeric_limits<double>::max()}};
            // Unpriced zero-capacity routes cross the real public dispatch threshold
            // without changing inventory, destination throughput, or baseline cost.
            for (std::size_t index = 1; index < 32; ++index)
                network.routes.push_back({"idle:" + std::to_string(index), "tank", "plant", 0, 0});
            const auto rows = one_control_rows("route:priced:flow", {10.0, 0.0, .25, .5});
            check_public_oracle(network, rows, {{1.0, .1}, 5});
            std::vector<CandidateControls> candidates;
            candidates.reserve(2048);
            for (std::size_t index = 0; index < 2048; ++index) candidates.push_back(rows[index % rows.size()]);
            require(detail::use_control_influence(network.routes.size(), candidates.size()),
                    "Cost-overflow regression did not exercise automatic influence dispatch");
            const SearchOptions options{{1.0, .1}, 5};
            check_public_implementations(network, candidates, options);
            const auto result = evaluate_candidates(network, candidates, options);
            require(result.diagnostics.numerical_screen_rejections == 512 &&
                        result.diagnostics.screen_passes == 1536 && result.recommendations.size() == 5,
                    "Influence dispatch lost finite repairs or failed to count cost-overflow rows");
            for (std::size_t rank = 0; rank < result.recommendations.size(); ++rank) {
                const auto& recommendation = result.recommendations[rank];
                require(recommendation.candidate_index == 1 + 4 * rank && recommendation.run.cost == 0.0 &&
                            recommendation.validation.feasible,
                        "Influence dispatch changed exact zero-cost/index ordering after overflow fallback");
            }
        }},
        {"reciprocal huge flows and zeroed overrides do not lose finite cancellation", [] {
            Network network;
            network.assets = {{"a", "A", AssetKind::tank, 100, 50}, {"b", "B", AssetKind::tank, 100, 50}};
            network.routes = {{"ab", "a", "b", 1e308, 1e308, true}, {"ba", "b", "a", 1e308, 1e308, true}};
            const std::vector<CandidateControls> candidates = {{},
                {{{"route:ab:flow", 0}, {"route:ba:flow", 0}}},
                {{{"route:ab:flow", 1e300}, {"route:ba:flow", 1e300}}},
                {{{"route:ab:flow", 1e308}, {"route:ba:flow", 0}}}};
            require(check_numeric_differential(network, candidates, 3.0) > 0,
                    "Huge reciprocal flows did not exercise the influence fallback");
            check_public_oracle(network, candidates, {{3.0, .025}, 5});
        }},
        {"changed costs topology and availability are rebuilt for each captured network", [] {
            auto network = source_cost_network();
            const std::vector<CandidateControls> candidates = {{},
                {{{"source:well:upstream:curtail", 8}, {"route:out:west:flow", 0}}},
                {{{"source:well:upstream:curtail", 0}, {"route:out:west:flow", 7}}}};
            check_public_oracle(network, candidates);
            network.routes[0].cost_per_m3 = .5;
            network.routes[1].target_id = "plant:joint";
            network.assets[3].capacity_m3 = 25;
            check_public_oracle(network, candidates);
            network.routes[2].available = false;
            network.routes[2].setpoint_m3h = 0;
            check_public_oracle(network, std::vector<CandidateControls>{{}, {{{"route:out:west:flow", 0}}}});
            rejects([&] { (void)evaluate_candidates(network, std::vector<CandidateControls>{{{{"route:out:west:flow", 1}}}}); });
        }},
        {"invalid controls still fail before the optimized screen", [] {
            const auto network = main_pump_failure(canonical_scenario());
            for (const auto& value : std::vector<ControlValue>{{"unknown", 0}, {"route:main_pump:flow", 1},
                    {"route:bypass:flow", -1}, {"route:bypass:flow", 26},
                    {"route:bypass:flow", std::numeric_limits<double>::quiet_NaN()}}) {
                const std::vector<CandidateControls> candidates{{{value}}};
                rejects([&] { (void)evaluate_candidates(network, candidates); });
            }
            rejects([&] { (void)evaluate_candidates(network,
                std::vector<CandidateControls>{{{{"route:bypass:flow", 1}, {"route:bypass:flow", 2}}}}); });
        }},
        {"no selected controls still evaluates one immutable candidate", [] {
            check_public_oracle(source_cost_network(), std::vector<CandidateControls>{{}});
            check_public_oracle(main_pump_failure(canonical_scenario()), std::vector<CandidateControls>{{}}, {{3.0, .025}, 5});
        }}
    };
    int failures = 0;
    for (const auto& [name, test] : tests) {
        try { test(); std::cout << "PASS " << name << '\n'; }
        catch (const std::exception& error) { ++failures; std::cerr << "FAIL " << name << ": " << error.what() << '\n'; }
    }
    std::cout << tests.size() - static_cast<std::size_t>(failures) << '/' << tests.size()
              << " control-influence cases passed\n";
    return failures == 0 ? 0 : 1;
}

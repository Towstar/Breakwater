#include "breakwater/search.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <future>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <utility>

using namespace breakwater;

namespace {
void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}
void near(double actual, double expected, double tolerance = 1e-8) {
    require(std::isfinite(actual) && std::abs(actual - expected) <= tolerance,
            "Expected " + std::to_string(expected) + ", got " + std::to_string(actual));
}
template<class Function> void rejects(Function&& function) {
    bool rejected = false;
    try { function(); } catch (const std::invalid_argument&) { rejected = true; }
    require(rejected, "Malformed request was accepted");
}
Network small_network(double route_cost = 0.0) {
    Network network;
    network.assets = {{"vessel:north", "North", AssetKind::tank, 100.0, 50.0},
                      {"plant:out", "Plant", AssetKind::treatment, 20.0}};
    network.routes = {{"pump:out", "vessel:north", "plant:out", 20.0, 0.0, true, true, route_cost}};
    return network;
}
Network feed_network() {
    Network network;
    network.assets = {{"storage:west", "Storage", AssetKind::tank, 100.0, 0.0},
                      {"well:a", "Well", AssetKind::source, 0.0, 0.0, 10.0, 0.0, 10.0, 1.0}};
    network.routes = {{"feed:main", "well:a", "storage:west", 10.0, 10.0, false, true, 10.0}};
    return network;
}
void check_result(const Network& network, const SearchResult& result, SearchOptions options = {}) {
    const auto& diagnostics = result.diagnostics;
    require(diagnostics.screen_passes + diagnostics.constraint_screen_rejections + diagnostics.numerical_screen_rejections ==
                diagnostics.candidates_evaluated, "Screen diagnostics do not account for all candidates");
    require(diagnostics.reference_checks <= diagnostics.screen_passes, "Too many reference checks");
    require(diagnostics.reference_checks >= result.recommendations.size(), "Recommendation lacks reference check");
    require(result.recommendations.size() <= options.result_limit, "Result limit exceeded");
    require((result.status == SearchStatus::recommendations_ready) == !result.recommendations.empty(), "Wrong search status");
    for (std::size_t i = 0; i < result.recommendations.size(); ++i) {
        const auto& candidate = result.recommendations[i];
        require(candidate.validation.feasible, "Returned an unvalidated or infeasible recommendation");
        require(candidate.run.solver == Solver::dense_reference, "Recommendation is not a dense reference run");
        require(candidate.candidate_index < diagnostics.candidates_evaluated, "Invalid candidate index");
        near(candidate.run.time_h.front(), 0.0); near(candidate.run.time_h.back(), options.config.horizon_h);
        require(std::isfinite(candidate.run.cost), "Nonfinite recommended cost");
        const auto compiled = compile(network, candidate.overrides);
        require(validate(compiled, candidate.run, options.config).feasible, "Stored run does not validate against its own overrides");
        if (i > 0) {
            const auto& previous = result.recommendations[i - 1];
            require(previous.run.cost < candidate.run.cost ||
                        (previous.run.cost == candidate.run.cost && previous.candidate_index < candidate.candidate_index),
                    "Results are not deterministically ordered by cost then input index");
        }
    }
}
}

int main() {
    const std::vector<std::pair<std::string, std::function<void()>>> tests = {
        {"canonical explicit batch rejects bad plans and returns real mixed reference", [] {
            const auto network = main_pump_failure(canonical_scenario());
            const std::vector<CandidateControls> candidates = {{incident_controls("bad")}, {incident_controls("mixed")}, {}};
            const auto result = evaluate_candidates(network, candidates);
            check_result(network, result);
            require(result.diagnostics.candidates_evaluated == 3, "Wrong batch count");
            require(result.recommendations.size() == 1 && result.recommendations[0].candidate_index == 1, "Wrong surviving candidate");
            near(result.recommendations[0].run.volumes_m3[0].back(), 80.0);
            near(result.recommendations[0].run.volumes_m3[1].back(), 117.0);
            near(result.recommendations[0].run.cost, 12780.0);
        }},
        {"small canonical grid agrees with exhaustive independent reference oracle", [] {
            const auto network = main_pump_failure(canonical_scenario());
            const std::vector<SearchAxis> axes = {{"route:bypass:flow", {15, 20, 25}},
                {"route:truck_route:flow", {18, 22}}, {"source:source_a:curtail", {5, 10, 15}}, {"route:backup_pump:flow", {30, 35}}};
            const auto result = search_candidates(network, axes);
            check_result(network, result);
            std::vector<std::pair<double, std::size_t>> oracle;
            std::size_t index = 0;
            for (double bypass : axes[0].values) for (double truck : axes[1].values)
                for (double curtail : axes[2].values) for (double backup : axes[3].values) {
                    const std::vector<ControlValue> controls = {{axes[0].control_id, bypass}, {axes[1].control_id, truck},
                        {axes[2].control_id, curtail}, {axes[3].control_id, backup}};
                    const auto compiled = compile(network, controls);
                    const auto run = simulate(compiled, {}, Solver::dense_reference);
                    if (validate(compiled, run).feasible) oracle.emplace_back(run.cost, index);
                    ++index;
                }
            std::sort(oracle.begin(), oracle.end());
            require(result.diagnostics.candidates_evaluated == index, "Wrong Cartesian product size");
            require(result.recommendations.size() == std::min(oracle.size(), max_search_recommendations), "Wrong result count");
            for (std::size_t i = 0; i < result.recommendations.size(); ++i) {
                near(result.recommendations[i].run.cost, oracle[i].first);
                require(result.recommendations[i].candidate_index == oracle[i].second, "Grid index/order differs from oracle");
            }
        }},
        {"curtailment ranking includes savings on nonzero-cost source feed route", [] {
            const auto network = feed_network();
            const std::vector<SearchAxis> axes = {{"source:well:a:curtail", {0, 5, 10}}};
            const SearchOptions options{{1.0, 0.1}, 3};
            const auto result = search_candidates(network, axes, options); check_result(network, result, options);
            require(result.recommendations.size() == 3, "Expected three feasible source settings");
            require(result.recommendations[0].candidate_index == 2 && result.recommendations[2].candidate_index == 0,
                    "Ignored avoided feed-route cost");
            near(result.recommendations[0].run.cost, 10.0); near(result.recommendations[1].run.cost, 55.0); near(result.recommendations[2].run.cost, 100.0);
        }},
        {"renamed colon IDs and unspecified settings remain intact", [] {
            auto network = small_network(2.0);
            network.routes[0].setpoint_m3h = 4.0;
            const std::vector<SearchAxis> axes = {{"route:pump:out:flow", {3.0, 5.0}}};
            const SearchOptions options{{0.5, 0.07}, 2};
            const auto result = search_candidates(network, axes, options); check_result(network, result, options);
            require(result.recommendations.size() == 2, "Colon control ID did not resolve");
            near(result.recommendations[0].run.volumes_m3[0].back(), 48.5);
            near(network.routes[0].setpoint_m3h, 4.0); near(network.assets[0].initial_m3, 50.0);
            const auto no_overrides = search_candidates(network, {}, options); check_result(network, no_overrides, options);
            near(no_overrides.recommendations[0].run.volumes_m3[0].back(), 48.0);
        }},
        {"shared treatment and trucking limits screen aggregate arrivals", [] {
            for (auto kind : {AssetKind::treatment, AssetKind::trucking}) {
                Network network;
                network.assets = {{"left", "Left", AssetKind::tank, 200, 100}, {"right", "Right", AssetKind::tank, 200, 100},
                                  {"joint", "Joint destination", kind, 15, 0, 0, 0, 0, 1}};
                network.routes = {{"one", "left", "joint", 10, 10}, {"two", "right", "joint", 10, 10}};
                const std::vector<SearchAxis> axes = {{"route:two:flow", {10, 5, 0}}};
                const auto result = search_candidates(network, axes); check_result(network, result);
                require(result.recommendations.size() == 2, "Wrong aggregate throughput result");
                require(result.recommendations[0].candidate_index == 2 && result.recommendations[1].candidate_index == 1, "Wrong throughput ranking");
                near(network.routes[0].setpoint_m3h, 10.0);
            }
        }},
        {"capacity equality and validator tolerance are respected", [] {
            for (double capacity : {1.0, 1.0 - 5e-10, 1.0 - 2e-9}) {
                auto network = feed_network(); network.assets[0].capacity_m3 = capacity;
                network.assets[1].inflow_m3h = 1; network.assets[1].curtailment_max_m3h = 1;
                const SearchOptions options{{1, 0.05}, 1};
                const auto result = search_candidates(network, {}, options); check_result(network, result, options);
                const auto compiled = compile(network); const auto run = simulate(compiled, options.config, Solver::dense_reference);
                const auto oracle = validate(compiled, run, options.config);
                require(!result.recommendations.empty() == oracle.feasible, "Screening changed the reference boundary decision");
            }
        }},
        {"no axes evaluate one unchanged candidate and infeasible search is explicit", [] {
            const auto network = main_pump_failure(canonical_scenario());
            const auto result = search_candidates(network, {}); check_result(network, result);
            require(result.diagnostics.candidates_evaluated == 1, "Empty grid must contain unchanged candidate");
            require(result.status == SearchStatus::no_feasible_plan_found && result.recommendations.empty(), "Baseline must not become a feasible plan");
        }},
        {"valid decimal and subnormal grids do not become false no-plan results", [] {
            const auto network = small_network();
            for (const auto config : std::vector<SimulationConfig>{{.9000000000000001, .1},
                    {.30000000000000004, .1}, {std::numeric_limits<double>::denorm_min(), 24.0}}) {
                const SearchOptions options{config, 1};
                const auto result = search_candidates(network, {}, options);
                check_result(network, result, options);
                require(result.recommendations.size() == 1 && result.diagnostics.reference_rejections == 0,
                        "Supported forecast grid was mistaken for an exhausted feasible search");
                const auto& times = result.recommendations[0].run.time_h;
                require(times.back() == config.horizon_h, "Recommendation ends short of its requested horizon");
                for (std::size_t i = 1; i < times.size(); ++i)
                    require(times[i] > times[i - 1], "Recommendation has duplicate final time");
                const std::vector<CandidateControls> batch(1);
                const auto explicit_result = evaluate_candidates(network, batch, options);
                check_result(network, explicit_result, options);
                require(explicit_result.recommendations.size() == 1, "Explicit batch rejected a supported forecast grid");
            }
        }},
        {"one-ULP final overflow never receives a recommendation", [] {
            const double q = std::ldexp(1.0, 50);
            Network network;
            network.assets = {{"tank", "Tank", AssetKind::tank, .9 * q, 0.0},
                              {"source", "Source", AssetKind::source, 0.0, 0.0, q}};
            network.routes = {{"feed", "source", "tank", q, 0.0}};
            const SearchOptions options{{.9000000000000001, .1}, 1};
            const auto result = search_candidates(network, {}, options);
            check_result(network, result, options);
            require(result.recommendations.empty() && result.status == SearchStatus::no_feasible_plan_found,
                    "Search recommended the one-ULP-short false-feasibility case");
            const std::vector<CandidateControls> batch(1);
            const auto explicit_result = evaluate_candidates(network, batch, options);
            check_result(network, explicit_result, options);
            require(explicit_result.recommendations.empty(), "Explicit batch recommended the false-feasibility case");
        }},
        {"forecast work-limit errors precede candidate screening", [] {
            const auto stationary = small_network();
            const auto infeasible = main_pump_failure(canonical_scenario());
            const SimulationConfig maximum{1.0, .0001};
            const SearchOptions maximum_options{maximum, 1};
            const auto accepted = search_candidates(stationary, {}, maximum_options);
            check_result(stationary, accepted, maximum_options);
            require(accepted.recommendations.size() == 1 && accepted.recommendations[0].run.time_h.size() == 10001,
                    "Search rejected or truncated the exact update cap");
            const SearchOptions rounded_cap{{.0082, 8.2e-7}, 1};
            const auto rounded_result = search_candidates(stationary, {}, rounded_cap);
            check_result(stationary, rounded_result, rounded_cap);
            require(rounded_result.recommendations.size() == 1 && rounded_result.recommendations[0].run.time_h.size() == 10001,
                    "Search used rounded division instead of the actual interval cap");
            const SearchOptions excessive{{std::nextafter(1.0, std::numeric_limits<double>::infinity()), .0001}, 1};
            const std::vector<CandidateControls> batch(1);
            for (const auto& network : {stationary, infeasible}) {
                rejects([&] { (void)search_candidates(network, {}, excessive); });
                rejects([&] { (void)evaluate_candidates(network, batch, excessive); });
            }
        }},
        {"equal cost results retain input order", [] {
            const auto network = small_network();
            const std::vector<SearchAxis> axes = {{"route:pump:out:flow", {5, 1, 3, 0}}};
            const SearchOptions options{{3, 0.025}, 3};
            const auto result = search_candidates(network, axes, options); check_result(network, result, options);
            require(result.recommendations.size() == 3, "Wrong tie result count");
            for (std::size_t i = 0; i < 3; ++i) require(result.recommendations[i].candidate_index == i, "Equal cost order is not stable by index");
        }},
        {"exactly 10000 candidates accepted and larger requests rejected", [] {
            const auto network = small_network();
            std::vector<SearchAxis> axes = {{"route:pump:out:flow", std::vector<double>(10000, 0.0)}};
            const auto result = search_candidates(network, axes); check_result(network, result);
            require(result.diagnostics.candidates_evaluated == 10000, "Exact candidate cap rejected or truncated");
            require(result.recommendations.size() == 5, "Expected bounded five recommendations");
            for (std::size_t i = 0; i < 5; ++i) require(result.recommendations[i].candidate_index == i, "Repeated value ordering changed");
            axes[0].values.push_back(0.0); rejects([&] { (void)search_candidates(network, axes); });
            std::vector<CandidateControls> batch(10001); rejects([&] { (void)evaluate_candidates(network, batch); });
            batch.resize(10000); const auto explicit_result = evaluate_candidates(network, batch); check_result(network, explicit_result);
            require(explicit_result.diagnostics.candidates_evaluated == 10000, "Exact explicit batch cap rejected");
        }},
        {"grid product limit and four-control limit are enforced", [] {
            const auto network = main_pump_failure(canonical_scenario());
            const std::vector<SearchAxis> product = {{"route:bypass:flow", std::vector<double>(101, 0)}, {"route:backup_pump:flow", std::vector<double>(100, 16)}};
            rejects([&] { (void)search_candidates(network, product); });
            const std::vector<SearchAxis> five = {{"source:source_a:curtail", {0}}, {"source:source_b:curtail", {0}},
                {"route:main_pump:flow", {0}}, {"route:backup_pump:flow", {0}}, {"route:bypass:flow", {0}}};
            rejects([&] { (void)search_candidates(network, five); });
            std::vector<CandidateControls> distinct;
            for (const auto& axis : five) distinct.push_back({{{axis.control_id, 0}}});
            rejects([&] { (void)evaluate_candidates(network, distinct); });
            CandidateControls too_many;
            for (const auto& axis : five) too_many.overrides.push_back({axis.control_id, 0});
            rejects([&] { const std::vector<CandidateControls> batch{too_many}; (void)evaluate_candidates(network, batch); });
        }},
        {"unknown duplicate nonfinite out-of-bounds and unavailable controls rejected", [] {
            const auto network = main_pump_failure(canonical_scenario());
            for (const auto& axis : std::vector<SearchAxis>{{"unknown", {0}}, {"route:bypass:flow", {}},
                 {"route:bypass:flow", {-1}}, {"route:bypass:flow", {26}}, {"route:main_pump:flow", {1}},
                 {"route:bypass:flow", {std::numeric_limits<double>::quiet_NaN()}},
                 {"route:bypass:flow", {std::numeric_limits<double>::infinity()}},
                 {"route:bypass:flow", {-std::numeric_limits<double>::infinity()}}}) {
                const std::vector<SearchAxis> axes{axis}; rejects([&] { (void)search_candidates(network, axes); });
            }
            const std::vector<SearchAxis> duplicate = {{"route:bypass:flow", {0}}, {"route:bypass:flow", {1}}};
            rejects([&] { (void)search_candidates(network, duplicate); });
            const std::vector<CandidateControls> duplicated_controls = {{{{"route:bypass:flow", 0}, {"route:bypass:flow", 1}}}};
            rejects([&] { (void)evaluate_candidates(network, duplicated_controls); });
            for (const auto& control : std::vector<ControlValue>{{"unknown", 0}, {"route:bypass:flow", -1},
                 {"route:bypass:flow", std::numeric_limits<double>::quiet_NaN()}, {"route:main_pump:flow", 1}}) {
                const std::vector<CandidateControls> batch{{{control}}}; rejects([&] { (void)evaluate_candidates(network, batch); });
            }
            const std::vector<SearchAxis> unavailable_zero = {{"route:main_pump:flow", {0}}};
            const auto valid_zero = search_candidates(network, unavailable_zero); check_result(network, valid_zero);
        }},
        {"source control cannot exceed its connected feed capacity", [] {
            auto network = feed_network(); network.assets[1].curtailment_m3h = 5; network.routes[0].capacity_m3h = 5;
            const std::vector<SearchAxis> axes = {{"source:well:a:curtail", {5, 0}}};
            rejects([&] { (void)search_candidates(network, axes); });
            const std::vector<CandidateControls> batch = {{{{"source:well:a:curtail", 0}}}};
            rejects([&] { (void)evaluate_candidates(network, batch); });
        }},
        {"malformed requests and options are rejected before evaluation", [] {
            const auto network = small_network();
            rejects([&] { (void)evaluate_candidates(network, {}); });
            for (auto options : std::vector<SearchOptions>{{{}, 0}, {{}, 6}, {{0, 0.025}, 1}, {{3, 0}, 1},
                 {{3, 1e-9}, 1}, {{25, 0.1}, 1}, {{std::numeric_limits<double>::quiet_NaN(), 0.1}, 1}})
                rejects([&] { (void)search_candidates(network, {}, options); });
            rejects([&] { (void)search_candidates(Network{}, {}); });
        }},
        {"candidate batches and concurrent searches do not mutate snapshots", [] {
            const auto network = main_pump_failure(canonical_scenario());
            const std::vector<CandidateControls> mixed{{incident_controls("mixed")}};
            const std::vector<CandidateControls> bad{{incident_controls("bad")}};
            auto first = std::async(std::launch::async, [&] { return evaluate_candidates(network, mixed); });
            auto second = std::async(std::launch::async, [&] { return evaluate_candidates(network, bad); });
            const auto a = first.get(), b = second.get(); check_result(network, a); check_result(network, b);
            require(a.recommendations.size() == 1 && b.recommendations.empty(), "Concurrent candidate leakage");
            near(network.assets[2].curtailment_m3h, 0.0); near(network.routes[3].setpoint_m3h, 16.0);
            near(network.routes[4].setpoint_m3h, 0.0); require(!network.routes[2].available, "Pump availability changed");
        }},
        {"overflow in an auxiliary screening scale defers to reference validation", [] {
            Network network;
            network.assets = {{"left", "Left", AssetKind::tank, 100, 50},
                              {"right", "Right", AssetKind::tank, 100, 50}};
            network.routes = {{"forward", "left", "right", 1e308, 1e308},
                              {"reverse", "right", "left", 1e308, 1e308}};
            const auto result = search_candidates(network, {}); check_result(network, result);
            require(result.recommendations.size() == 1, "Auxiliary overflow hid a reference-valid candidate");
            require(result.diagnostics.reference_checks == 1 && result.diagnostics.numerical_screen_rejections == 0,
                    "Uncertain screen was not checked by the reference engine");
            near(result.recommendations[0].run.volumes_m3[0].back(), 50);
            near(result.recommendations[0].run.volumes_m3[1].back(), 50);
        }},
        {"nonfinite arithmetic is counted and never recommended", [] {
            auto network = small_network(std::numeric_limits<double>::max()); network.routes[0].setpoint_m3h = 10;
            const std::vector<CandidateControls> batch = {{{{"route:pump:out:flow", 0}}}, {{{"route:pump:out:flow", 10}}}};
            const auto result = evaluate_candidates(network, batch); check_result(network, result);
            require(result.recommendations.size() == 1 && result.recommendations[0].candidate_index == 0, "Finite candidate lost or overflow candidate returned");
            require(result.diagnostics.numerical_screen_rejections == 1, "Cost overflow was not reported");
            Network overflow;
            overflow.assets = {{"reservoir", "Reservoir", AssetKind::tank, 1e308, 0},
                {"a", "A", AssetKind::source, 0, 0, 1e308}, {"b", "B", AssetKind::source, 0, 0, 1e308}};
            overflow.routes = {{"feed_a", "a", "reservoir", 1e308, 0}, {"feed_b", "b", "reservoir", 1e308, 0}};
            const auto bad = search_candidates(overflow, {}); check_result(overflow, bad);
            require(bad.recommendations.empty() && bad.diagnostics.numerical_screen_rejections == 1, "Rate overflow became a feasible recommendation");
        }}
    };
    int failures = 0;
    for (const auto& [name, test] : tests) {
        try { test(); std::cout << "PASS " << name << '\n'; }
        catch (const std::exception& error) { ++failures; std::cerr << "FAIL " << name << ": " << error.what() << '\n'; }
    }
    std::cout << tests.size() - static_cast<std::size_t>(failures) << '/' << tests.size() << " search cases passed\n";
    return failures == 0 ? 0 : 1;
}

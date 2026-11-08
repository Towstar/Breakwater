#include "breakwater/engine.hpp"
#include <algorithm>
#include <cmath>
#include <functional>
#include <future>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>

using namespace breakwater;
namespace {
void require(bool condition, const std::string& message) { if (!condition) throw std::runtime_error(message); }
void near(double actual, double expected, double tolerance = 1e-8) {
    require(std::isfinite(actual) && std::abs(actual - expected) <= tolerance, "Expected " + std::to_string(expected) + ", got " + std::to_string(actual));
}
template<class Fn> void rejects(Fn fn) {
    bool rejected = false;
    try { fn(); } catch (const std::invalid_argument&) { rejected = true; }
    require(rejected, "Invalid input was accepted");
}
bool has(const ValidationReport& r, const std::string& code, const std::string& asset) {
    return std::any_of(r.violations.begin(), r.violations.end(), [&](const Issue& i) { return i.code == code && i.asset_id == asset; });
}
Network stationary_network() {
    return {{{"still", "Stationary tank", AssetKind::tank, 1.0, 0.5}}, {}};
}
Network priced_outlet_network(double flow = 1.0, double route_price = 0.0, double destination_price = 0.0) {
    Network network;
    network.assets = {{"tank", "Tank", AssetKind::tank, 100.0, 50.0},
        {"plant", "Plant", AssetKind::treatment, 20.0, 0.0, 0.0, 0.0, 0.0, destination_price}};
    network.routes = {{"priced", "tank", "plant", 20.0, flow, true, true, route_price}};
    return network;
}
void require_cost_overflow(const CompiledNetwork& compiled, SimulationResult run, SimulationConfig config) {
    // Exercise supplied finite/nonfinite costs independently of the cached cost.
    for (const double supplied : {0.0, 42.0, std::numeric_limits<double>::max(),
         std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(),
         std::numeric_limits<double>::quiet_NaN()}) {
        run.cost = supplied;
        const auto validation = validate(compiled, run, config);
        require(!validation.feasible && has(validation, "cost_overflow", ""),
                "Nonfinite independently derived cost passed or lacked an explicit overflow issue");
        require(has(validation, "cost_mismatch", ""),
                "Tolerant cost comparison accepted a nonfinite independently derived value");
    }
}
void require_covered_grid(const SimulationResult& run, SimulationConfig config) {
    require(run.time_h.size() >= 2 && run.time_h.size() <= 10001, "Sample grid exceeds the update bounds");
    require(run.time_h.front() == 0.0 && run.time_h.back() == config.horizon_h,
            "Accepted forecast does not exactly cover its requested horizon");
    for (std::size_t k = 1; k < run.time_h.size(); ++k)
        require(std::isfinite(run.time_h[k]) && run.time_h[k] > run.time_h[k - 1],
                "Accepted forecast contains duplicate or nonincreasing timestamps");
}
}

int main() {
    int failures = 0;
    const std::vector<std::pair<std::string, std::function<void()>>> tests = {
        {"healthy preview accumulates explicitly", [] {
            const auto c = compile(canonical_scenario());
            const auto run = simulate(c);
            require(validate(c, run).feasible, "Healthy bounded preview should pass");
            near(run.volumes_m3[0].back(), 104.0); near(run.volumes_m3[1].back(), 114.0);
        }},
        {"baseline exact limit time and unclipped prediction", [] {
            const auto c = compile(main_pump_failure(canonical_scenario()));
            const auto run = simulate(c); const auto v = validate(c, run);
            require(has(v, "storage_capacity", "tank_a"), "Expected A overflow");
            near(v.limits.front().time_h, 40.0 / 52.0);
            near(run.volumes_m3[0].back(), 236.0); near(run.volumes_m3[1].back(), 114.0);
            require(v.max_conservation_residual_m3 < 1e-9, "Conservation failed");
        }},
        {"isolated downstream failure", [] {
            const auto c = compile(main_pump_failure(canonical_scenario()), incident_controls("bad"));
            const auto run = simulate(c); const auto v = validate(c, run);
            require(has(v, "storage_capacity", "tank_b") && !has(v, "storage_capacity", "tank_a"), "Wrong downstream diagnosis");
            near(v.limits.front().time_h, 16.0 / 6.0); near(run.volumes_m3[0].back(), 80.0);
        }},
        {"mixed plan cost and inventory", [] {
            const auto c = compile(main_pump_failure(canonical_scenario()), incident_controls("mixed"));
            const auto run = simulate(c); require(validate(c, run).feasible, "Mixed plan should pass nominal volume constraints");
            near(run.volumes_m3[0].back(), 80.0); near(run.volumes_m3[1].back(), 117.0); near(run.cost, 12780.0);
        }},
        {"capacity equality is permitted", [] {
            auto n = canonical_scenario(); n.assets[0].initial_m3 = 96.0;
            const auto c = compile(n); const auto run = simulate(c);
            const auto v = validate(c, run); require(v.feasible, "Equality should pass");
            near(run.volumes_m3[0].back(), 120.0); near(v.limits.front().time_h, 3.0);
        }},
        {"generic renamed network and shared throughput", [] {
            Network n;
            n.assets = {{"x", "X", AssetKind::tank, 200.0, 100.0}, {"y", "Y", AssetKind::tank, 200.0, 100.0}, {"plant", "Plant", AssetKind::treatment, 15.0}};
            n.routes = {{"one", "x", "plant", 10.0, 10.0}, {"two", "y", "plant", 10.0, 10.0}};
            const auto c = compile(n); require(has(validate(c, simulate(c)), "destination_throughput", "plant"), "Missing aggregate throughput failure");
        }},
        {"negative inventories remain visible", [] {
            Network n; n.assets = {{"x", "X", AssetKind::tank, 20.0, 1.0}, {"sink", "Sink", AssetKind::treatment, 10.0}};
            n.routes = {{"pipe", "x", "sink", 10.0, 2.0}};
            const auto c = compile(n); const auto run = simulate(c);
            near(run.volumes_m3[0].back(), -5.0); require(has(validate(c, run), "negative_inventory", "x"), "Negative inventory was hidden");
        }},
        {"unknown nonfinite duplicate and unavailable controls rejected", [] {
            const auto n = main_pump_failure(canonical_scenario());
            for (const auto& controls : std::vector<std::vector<ControlValue>>{
                {{"route:bypass:flow", -1.0}}, {{"route:bypass:flow", 100.0}}, {{"invented", 1.0}},
                {{"route:main_pump:flow", 1.0}}, {{"route:bypass:flow", std::numeric_limits<double>::quiet_NaN()}},
                {{"route:bypass:flow", 1.0}, {"route:bypass:flow", 2.0}}}) rejects([&] { (void)compile(n, controls); });
        }},
        {"draft and malformed topology rejected", [] {
            auto n = canonical_scenario(); n.routes.erase(n.routes.begin()); require(!inspect_network(n).empty(), "Unconnected source passed");
            rejects([&] { (void)compile(n); });
            n = canonical_scenario(); n.routes[0].target_id = "missing"; rejects([&] { (void)compile(n); });
            n = canonical_scenario(); n.routes[0].id = n.assets[0].id; rejects([&] { (void)compile(n); });
            n = canonical_scenario(); n.routes[4].target_id = "tank_a"; rejects([&] { (void)compile(n); });
        }},
        {"independent validator catches corrupted trajectories and cost", [] {
            const auto c = compile(canonical_scenario()); auto run = simulate(c);
            run.volumes_m3[0][15] += 1.0; require(has(validate(c, run), "reference_mismatch", "tank_a"), "Corruption passed");
            run = simulate(c); run.cost = 123.0; require(has(validate(c, run), "cost_mismatch", ""), "False cost passed");
            run.time_h[3] = -1.0; require(!validate(c, run).feasible, "Malformed time passed");
            run = simulate(c, {1.0, 0.025}); require(!validate(c, run).feasible, "Shortened forecast passed a 3 h validation");
        }},
        {"nonfinite derived catalog prices are explicit invalid network parameters", [] {
            const double largest = std::numeric_limits<double>::max();
            for (const double flow : {0.0, 1.0}) {
                const auto network = priced_outlet_network(flow, largest, largest);
                const auto issues = inspect_network(network);
                require(std::any_of(issues.begin(), issues.end(), [](const Issue& issue) {
                    return issue.code == "cost_overflow" && issue.asset_id == "priced";
                }), "Overflowing route-plus-destination unit price was not identified");
                rejects([&] { (void)control_catalog(network); });
                rejects([&] { (void)compile(network); });
            }
        }},
        {"independent cost overflow rejects finite infinite and NaN result costs", [] {
            const double largest = std::numeric_limits<double>::max();
            const SimulationConfig one_hour{1.0, .25};

            // Even a tiny flow with a finite total cannot make an infinite
            // catalog unit price valid. Check this independently of compile.
            auto unrepresentable_price = compile(priced_outlet_network(.25));
            auto finite_charge = simulate(unrepresentable_price, one_hour);
            unrepresentable_price.network.routes[0].cost_per_m3 = largest;
            unrepresentable_price.network.assets[1].cost_per_m3 = largest;
            finite_charge.cost = .25 * largest + .25 * largest;
            require(std::isfinite(finite_charge.cost), "Expected a finite charge despite overflowing unit-price sum");
            const auto invalid_price = validate(unrepresentable_price, finite_charge, one_hour);
            require(!invalid_price.feasible && has(invalid_price, "cost_overflow", "priced"),
                    "Independent validator trusted an unrepresentable catalog unit price");

            // Compile and simulate safe zero-price inputs, then change only the
            // source-of-truth prices. A validator trusting cached cost_per_h or
            // the supplied result cost would accept these forged zero costs.
            auto route_product = compile(priced_outlet_network(2.0));
            auto route_run = simulate(route_product, one_hour);
            route_product.network.routes[0].cost_per_m3 = largest;
            require(route_product.cost_per_h == 0.0, "Route-product fixture lost its independent cached cost");
            require_cost_overflow(route_product, route_run, one_hour);

            auto destination_product = compile(priced_outlet_network(2.0));
            auto destination_run = simulate(destination_product, one_hour);
            destination_product.network.assets[1].cost_per_m3 = largest;
            require_cost_overflow(destination_product, destination_run, one_hour);

            auto summed_prices = compile(priced_outlet_network());
            auto summed_run = simulate(summed_prices, one_hour);
            summed_prices.network.routes[0].cost_per_m3 = largest;
            summed_prices.network.assets[1].cost_per_m3 = largest;
            require_cost_overflow(summed_prices, summed_run, one_hour);

            auto multiple_routes = priced_outlet_network();
            auto second_route = multiple_routes.routes[0]; second_route.id = "second-priced";
            multiple_routes.routes.push_back(second_route);
            auto summed_terms = compile(multiple_routes);
            auto terms_run = simulate(summed_terms, one_hour);
            for (auto& route : summed_terms.network.routes) route.cost_per_m3 = .75 * largest;
            require(std::isfinite(summed_terms.network.routes[0].cost_per_m3), "Individual term must remain finite");
            require_cost_overflow(summed_terms, terms_run, one_hour);

            Network source_network;
            source_network.assets = {{"tank", "Tank", AssetKind::tank, 100.0, 50.0},
                {"well", "Well", AssetKind::source, 0.0, 0.0, 4.0, 2.0, 4.0}};
            source_network.routes = {{"feed", "well", "tank", 4.0, 0.0}};
            auto source_product = compile(source_network);
            auto source_run = simulate(source_product, one_hour);
            source_product.network.assets[1].cost_per_m3 = largest;
            require_cost_overflow(source_product, source_run, one_hour);

            const SimulationConfig three_hours{3.0, .25};
            auto horizon_product = compile(priced_outlet_network());
            auto horizon_run = simulate(horizon_product, three_hours);
            horizon_product.network.routes[0].cost_per_m3 = largest / 2.0;
            require(std::isfinite(horizon_product.network.routes[0].cost_per_m3), "Hourly cost must remain finite");
            require_cost_overflow(horizon_product, horizon_run, three_hours);
        }},
        {"simulated cost overflow remains visible and fails independent validation", [] {
            const double largest = std::numeric_limits<double>::max();
            // Finite catalog prices must not make a repairable operating setting
            // un-compilable: search can reduce this flow to a finite-cost value.
            const auto network = priced_outlet_network(2.0, largest);
            const auto compiled = compile(network);
            const SimulationConfig config{1.0, .25};
            require(std::isfinite(control_catalog(network)[0].cost_per_m3), "Catalog unit cost should be finite");
            require(std::isinf(compiled.cost_per_h), "Expected unrounded hourly overflow evidence");
            for (const auto solver : {Solver::dense_reference, Solver::sparse, Solver::affine}) {
                auto run = simulate(compiled, config, solver);
                require(std::isinf(run.cost), "Simulated cost overflow was clipped or replaced with zero");
                require_cost_overflow(compiled, run, config);
            }
            const auto finite_hourly = compile(priced_outlet_network(1.0, largest / 2.0));
            require(std::isfinite(finite_hourly.cost_per_h), "Expected a finite hourly cost before horizon scaling");
            const SimulationConfig longer{3.0, .25};
            const auto run = simulate(finite_hourly, longer);
            require(std::isinf(run.cost), "Expected unrounded horizon-product overflow evidence");
            require_cost_overflow(finite_hourly, run, longer);
        }},
        {"finite huge and zero costs validate independently of cached pricing", [] {
            const double largest = std::numeric_limits<double>::max();
            const SimulationConfig config{1.0, .25};
            for (const auto network : {priced_outlet_network(1.0, largest),
                 priced_outlet_network(0.0, largest), priced_outlet_network(1.0)}) {
                auto compiled = compile(network);
                const auto run = simulate(compiled, config);
                require(std::isfinite(run.cost) && validate(compiled, run, config).feasible,
                        "Representable huge/zero cost was incorrectly rejected");
                const auto expected = run.cost;
                for (const double cached : {0.0, std::numeric_limits<double>::infinity(),
                     std::numeric_limits<double>::quiet_NaN()}) {
                    compiled.cost_per_h = cached;
                    require(validate(compiled, run, config).feasible,
                            "Independent validation trusted corrupted cached pricing");
                }
                require(run.cost == expected, "Validation changed the supplied result cost");
            }
            auto zero_compiled = compile(priced_outlet_network(0.0));
            auto negative_zero_cost = simulate(zero_compiled, config);
            negative_zero_cost.cost = -1e-12;
            const auto negative_report = validate(zero_compiled, negative_zero_cost, config);
            require(!negative_report.feasible && has(negative_report, "cost_mismatch", ""),
                    "Tolerance accepted a negative cost for a zero-cost operation");
            const auto compiled = compile(priced_outlet_network(1.0, 7.0));
            auto run = simulate(compiled, config);
            require(run.cost == 7.0 && validate(compiled, run, config).feasible, "Finite priced reference failed");
            for (const double forged : {0.0, 8.0, std::numeric_limits<double>::infinity(),
                 -std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
                run.cost = forged;
                const auto report = validate(compiled, run, config);
                require(!report.feasible && has(report, "cost_mismatch", ""), "Altered/nonfinite finite-reference cost passed");
            }
        }},
        {"decimal horizon edge cases preserve interior samples and the exact endpoint", [] {
            const auto c = compile(stationary_network());
            for (const auto solver : {Solver::dense_reference, Solver::sparse, Solver::affine}) {
                const SimulationConfig rounded_third{.225, .075};
                const auto rounded_run = simulate(c, rounded_third, solver);
                require(rounded_run.time_h == std::vector<double>{0, .075, .15, .22499999999999998, .225},
                        "Rounded third interior multiple or original endpoint was lost");
                require(validate(c, rounded_run, rounded_third).feasible, "Exact rounded-third grid failed validation");
                const SimulationConfig short_cfg{.9000000000000001, .1};
                const auto short_run = simulate(c, short_cfg, solver);
                require_covered_grid(short_run, short_cfg);
                require(short_run.time_h.size() == 11 && short_run.time_h[9] == .9,
                        "One-ULP final interval dropped or shifted the preceding requested sample");
                require(validate(c, short_run, short_cfg).feasible, "Stationary one-ULP-tail forecast should validate");
                const SimulationConfig duplicate_cfg{.30000000000000004, .1};
                const auto duplicate_run = simulate(c, duplicate_cfg, solver);
                require_covered_grid(duplicate_run, duplicate_cfg);
                require(duplicate_run.time_h.size() == 4 && duplicate_run.time_h[1] == .1 &&
                            duplicate_run.time_h[2] == .2,
                        "Duplicate-final fix changed preceding requested samples");
                require(validate(c, duplicate_run, duplicate_cfg).feasible, "Valid decimal endpoint was rejected");
            }
        }},
        {"subnormal and neighboring horizons remain strictly covered", [] {
            const auto c = compile(stationary_network());
            std::vector<SimulationConfig> configs = {{std::numeric_limits<double>::denorm_min(), 24.0},
                {1e-308, 1e308}, {.25, 1.0}, {1.1, .3}};
            for (const double horizon : {.1, .3, .9, 1.0}) {
                configs.push_back({std::nextafter(horizon, 0.0), .1});
                configs.push_back({horizon, .1});
                configs.push_back({std::nextafter(horizon, std::numeric_limits<double>::infinity()), .1});
            }
            for (const auto config : configs) for (const auto solver : {Solver::dense_reference, Solver::sparse, Solver::affine}) {
                const auto run = simulate(c, config, solver);
                require_covered_grid(run, config);
                require(validate(c, run, config).feasible, "Valid stationary edge-grid forecast was rejected");
                if (config.step_h > config.horizon_h)
                    require(run.time_h.size() == 2, "Step larger than horizon must retain both interval endpoints");
            }
        }},
        {"one-ULP shortened and extended results never satisfy requested-horizon validation", [] {
            const auto c = compile(stationary_network());
            const SimulationConfig config{.9000000000000001, .1};
            auto short_run = simulate(c, config);
            short_run.time_h.pop_back();
            short_run.volumes_m3[0].pop_back();
            const auto short_validation = validate(c, short_run, config);
            require(!short_validation.feasible && has(short_validation, "forecast_horizon", ""),
                    "One-ULP-short result passed the original requested horizon");
            auto extended_run = simulate(c, config);
            extended_run.time_h.back() = std::nextafter(config.horizon_h, std::numeric_limits<double>::infinity());
            const auto extended_validation = validate(c, extended_run, config);
            require(!extended_validation.feasible && has(extended_validation, "forecast_horizon", ""),
                    "One-ULP-extended result passed exact requested-horizon validation");
        }},
        {"requested-horizon overflow cannot be hidden by one-ULP-short sampling", [] {
            const double q = std::ldexp(1.0, 50);
            Network network;
            network.assets = {{"tank", "Tank", AssetKind::tank, .9 * q, 0.0},
                              {"source", "Source", AssetKind::source, 0.0, 0.0, q}};
            network.routes = {{"feed", "source", "tank", q, 0.0}};
            const auto c = compile(network);
            const SimulationConfig config{.9000000000000001, .1};
            double requested_endpoint = 0.0;
            evaluate_endpoints(c, config.horizon_h, std::span(&requested_endpoint, 1));
            require(requested_endpoint - c.capacity_m3[0] == .125, "False-feasibility fixture lost its requested-end overflow");
            for (const auto solver : {Solver::dense_reference, Solver::sparse, Solver::affine}) {
                const auto run = simulate(c, config, solver);
                require_covered_grid(run, config);
                require(run.volumes_m3[0].back() > c.capacity_m3[0], "Overflowing endpoint was clipped or omitted");
                const auto validation = validate(c, run, config);
                require(!validation.feasible && has(validation, "storage_capacity", "tank"),
                        "Requested-end overflow incorrectly validated as feasible");
            }
        }},
        {"actual interval count enforces the 10000-update boundary", [] {
            const auto c = compile(stationary_network());
            const SimulationConfig maximum{1.0, .0001};
            for (const auto solver : {Solver::dense_reference, Solver::sparse, Solver::affine}) {
                const auto run = simulate(c, maximum, solver);
                require_covered_grid(run, maximum);
                require(run.time_h.size() == 10001, "Exact update cap was truncated or rejected");
                require(validate(c, run, maximum).feasible, "Exact update-cap forecast did not validate");
                // The quotient rounds above 10000 although the 10000th rounded
                // multiple is exactly the requested endpoint.
                const SimulationConfig rounded_cap{.0082, 8.2e-7};
                const auto rounded_run = simulate(c, rounded_cap, solver);
                require_covered_grid(rounded_run, rounded_cap);
                require(rounded_run.time_h.size() == 10001 && validate(c, rounded_run, rounded_cap).feasible,
                        "Rounded division falsely rejected a valid exact-cap grid");
                const SimulationConfig tail{std::nextafter(1.0, std::numeric_limits<double>::infinity()), .0001};
                rejects([&] { (void)simulate(c, tail, solver); });
                rejects([&] { (void)validate(c, run, tail); });
            }
        }},
        {"random graph dense sparse affine agreement and internal conservation", [] {
            std::mt19937 random(42); std::uniform_real_distribution<double> q(0.0, 0.2);
            for (int trial = 0; trial < 25; ++trial) {
                Network n;
                for (int i = 0; i < 12; ++i) n.assets.push_back({"t" + std::to_string(i), "Tank", AssetKind::tank, 1000.0, 500.0});
                for (int i = 0; i < 30; ++i) {
                    const auto s = static_cast<int>(random() % 12); const auto d = (s + 1 + static_cast<int>(random() % 11)) % 12;
                    n.routes.push_back({"e" + std::to_string(i), "t" + std::to_string(s), "t" + std::to_string(d), 1.0, q(random)});
                }
                const auto c = compile(n); const auto dense = simulate(c, {3.0, 0.07}, Solver::dense_reference);
                const auto sparse = simulate(c, {3.0, 0.07}); const auto affine = simulate(c, {3.0, 0.07}, Solver::affine);
                require(validate(c, sparse).feasible, "Random graph validation failed");
                std::vector<double> endpoints(c.tank_ids.size()); evaluate_endpoints(c, 3.0, endpoints);
                for (std::size_t k = 0; k < dense.time_h.size(); ++k) {
                    double total = 0.0;
                    for (std::size_t i = 0; i < dense.tank_ids.size(); ++i) {
                        near(dense.volumes_m3[i][k], sparse.volumes_m3[i][k]); near(dense.volumes_m3[i][k], affine.volumes_m3[i][k]);
                        total += sparse.volumes_m3[i][k];
                    }
                    near(total, 6000.0);
                }
                for (std::size_t i = 0; i < endpoints.size(); ++i) near(endpoints[i], sparse.volumes_m3[i].back());
                near(dense.time_h.back(), 3.0);
            }
        }},
        {"candidate snapshots are independent under concurrency", [] {
            const auto n = main_pump_failure(canonical_scenario());
            auto first = std::async(std::launch::async, [&] { return simulate(compile(n, incident_controls("mixed"))); });
            auto second = std::async(std::launch::async, [&] { return simulate(compile(n)); });
            near(first.get().volumes_m3[0].back(), 80.0); near(second.get().volumes_m3[0].back(), 236.0);
            near(n.assets[2].curtailment_m3h, 0.0); near(n.routes[4].setpoint_m3h, 0.0);
        }},
        {"bounded configuration and workspace validation", [] {
            const auto c = compile(canonical_scenario());
            rejects([&] { (void)simulate(c, {3.0, 0.0}); }); rejects([&] { (void)simulate(c, {3.0, 1e-9}); });
            rejects([&] { std::vector<double> wrong; evaluate_endpoints(c, 3.0, wrong); });
            rejects([&] { (void)simulate(c, {std::numeric_limits<double>::infinity(), 0.1}); });
        }}
    };
    for (const auto& [name, fn] : tests) {
        try { fn(); std::cout << "PASS " << name << '\n'; }
        catch (const std::exception& e) { ++failures; std::cerr << "FAIL " << name << ": " << e.what() << '\n'; }
    }
    std::cout << tests.size() - static_cast<std::size_t>(failures) << '/' << tests.size() << " cases passed\n";
    return failures ? 1 : 0;
}

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
        {"decimal horizon edge cases preserve interior samples and the exact endpoint", [] {
            const auto c = compile(stationary_network());
            for (const auto solver : {Solver::dense_reference, Solver::sparse, Solver::affine}) {
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

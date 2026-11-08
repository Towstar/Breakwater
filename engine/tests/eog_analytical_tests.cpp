#include "breakwater/eog_analytical.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>

using namespace breakwater::eog;
namespace {
void require(bool test, const std::string& message) {
    if (!test) throw std::runtime_error(message);
}
void near(double actual, double expected, double tolerance = 1e-9) {
    require(std::isfinite(actual) && std::abs(actual - expected) <= tolerance,
        "expected " + std::to_string(expected) + ", got " + std::to_string(actual));
}
template<class Fn> void rejects(Fn fn) {
    bool rejected = false;
    try { fn(); } catch (const std::invalid_argument&) { rejected = true; }
    require(rejected, "unsupported input was accepted");
}
bool has(const Assessment& result, const std::string& code) {
    return std::any_of(result.violations.begin(), result.violations.end(),
        [&](const auto& issue) { return issue.code == code; });
}
void invalid(const Assessment& result, const std::string& code) {
    require(!result.valid() && !result.avoids_shutdown() && has(result, code), "missing action violation: " + code);
    require(!result.total_cost && !result.truck_cost && !result.lost_revenue,
        "invalid response acquired a completed cost");
}
FailureFixture small() {
    // 10 bbl/h, $30/h; 50 bbl of headroom => trip at 5 h, repair at 6 h.
    return {{24.0, 0.0}, 20.0, 7.0, {{"system", 2}},
        {{"facility", "system", 100.0, 30.0, 80.0, 6.0, 60.0, 120.0, false,
            {{"cheap", 120.0, 10.0, 0.0, true}, {"dear", 120.0, 20.0, 0.0, true}}}}};
}
Response truck(double call = 0.0, std::size_t count = 1) {
    return {{}, {{"facility", 0, call, count}}};
}
FailureFixture opening() {
    // Public README/network/ops example; no answer key is loaded or encoded.
    return {{60.0, 3.0}, 150.0, 905.0, {{"opening-system", 2}},
        {{"opening-facility", "opening-system", 800.0, 50.0, 92.0, 6.0, 75.0, 230.0, false,
            {{"high", 450.0, 560.0, 2000.0}, {"low", 700.0, 170.0, 600.0},
             {"middle", 650.0, 390.0, 1400.0}}}}};
}
void duplicate_facility(FailureFixture& f, const std::string& id, const std::string& system = "system") {
    auto copy = f.facilities.front();
    copy.id = id; copy.system_id = system;
    for (auto& w : copy.wells) w.id = id + ":" + w.id;
    f.facilities.push_back(copy);
}
}

int main() {
    const std::vector<std::pair<std::string, std::function<void()>>> tests = {
        {"barrel and daily units", [] {
            near(volume_bbl(800.0, 50.0), 400.0);
            near(volume_bbl(800.0, 92.0), 736.0);
            near(hourly_water_bbl(1800.0), 75.0);
            near(hourly_revenue(170.0, 600.0, 60.0, 3.0), 500.0);
        }},
        {"finite raw unusual values are not clipped", [] {
            near(volume_bbl(100.0, -3.0), -3.0);
            near(volume_bbl(100.0, 103.0), 103.0);
            near(hourly_water_bbl(-24.0), -1.0);
            auto f = small(); f.facilities[0].initial_level_pct = -0.1;
            rejects([&] { assess_failure_fixture(f); });
            f.facilities[0].initial_level_pct = 100.1;
            rejects([&] { assess_failure_fixture(f); });
        }},
        {"normal stopped state and hysteretic cycle", [] {
            auto state = PumpState::stopped;
            for (const double level : {30.0, 40.0, 69.0}) {
                state = hysteresis(state, level, 70.0, 30.0, false);
                require(state == PumpState::stopped, "normal stopped pump became failure");
            }
            state = hysteresis(state, 70.0, 70.0, 30.0, false);
            require(state == PumpState::running, "start equality missed");
            state = hysteresis(state, 50.0, 70.0, 30.0, false);
            require(state == PumpState::running, "pump lost hysteretic memory");
            state = hysteresis(state, 30.0, 70.0, 30.0, false);
            require(state == PumpState::stopped, "stop equality missed");
        }},
        {"fault feedback and unknown restart", [] {
            require(hysteresis(PumpState::running, 50.0, 70.0, 30.0, true) == PumpState::fault, "fault ignored");
            rejects([] { hysteresis(PumpState::fault, 50.0, 70.0, 30.0, false); });
            rejects([] { hysteresis(PumpState::stopped, 50.0, 30.0, 70.0, false); });
            rejects([] { hysteresis(PumpState::stopped, 50.0, 70.0, 70.0, false); });
        }},
        {"analytical countdown uses shutdown threshold", [] {
            near(*countdown_h(400.0, 736.0, 75.0), 4.48);
            near(*countdown_h(736.0, 736.0, 0.0), 0.0);
            near(*countdown_h(740.0, 736.0, -1.0), 0.0);
            require(!countdown_h(400.0, 736.0, 0.0), "zero inflow fabricated a countdown");
            require(!countdown_h(400.0, 736.0, -1.0), "negative net rate fabricated a countdown");
        }},
        {"remaining repair is based on assumed onset", [] {
            near(repair_remaining_h(2.0, 3.5, 6.0), 4.5);
            near(repair_remaining_h(-2.0, 1.0, 6.0), 3.0);
            near(repair_remaining_h(2.0, 8.0, 6.0), 0.0);
            near(repair_remaining_h(2.0, 9.0, 6.0), 0.0);
            rejects([] { repair_remaining_h(4.0, 3.0, 6.0); });
        }},
        {"observation fixture uses an explicit historical clock and policy", [] {
            const ObservationFixture o{Sample{50.0, 10.0}, Sample{1800.0, 10.0}, Sample{0.0, 10.0},
                10.0, 10.0 / 60.0, 5.0 / 60.0};
            const auto result = observed_countdown_fixture(800.0, 92.0, o);
            require(result.status == CountdownStatus::available, "fresh causal fixture unavailable");
            near(*result.hours, 4.48);
        }},
        {"missing stale future and incoherent evidence abstains", [] {
            const ObservationFixture fresh{Sample{50.0, 10.0}, Sample{1800.0, 10.0}, Sample{0.0, 10.0},
                10.0, 0.25, 0.05};
            auto missing = fresh; missing.pump_flow_bpd.reset();
            auto stale = fresh; stale.level_pct->sampled_at_h = 9.0;
            auto future = fresh; future.pump_flow_bpd->sampled_at_h = 10.01;
            auto skew = fresh; skew.level_pct->sampled_at_h = 9.9;
            for (const auto& [o, reason] : std::vector<std::pair<ObservationFixture, std::string>>{
                {missing, "missing_observation"}, {stale, "stale_observation"},
                {future, "future_observation"}, {skew, "incoherent_observations"}}) {
                const auto result = observed_countdown_fixture(800.0, 92.0, o);
                require(result.status == CountdownStatus::unavailable && !result.hours && result.reason == reason,
                    "unavailable evidence fabricated a valid countdown: " + reason);
            }
        }},
        {"no-crossing and reached are distinct from unavailable", [] {
            ObservationFixture o{Sample{50.0, 0.0}, Sample{0.0, 0.0}, Sample{0.0, 0.0}, 0.0, 0.0, 0.0};
            require(observed_countdown_fixture(800.0, 92.0, o).status == CountdownStatus::no_crossing,
                "valid zero-rate assumption confused with missing data");
            o.level_pct->value = 92.0;
            const auto reached = observed_countdown_fixture(800.0, 92.0, o);
            require(reached.status == CountdownStatus::already_reached, "reached threshold lost");
            near(*reached.hours, 0.0);
            o.level_pct->value = 101.0;
            const auto unsupported = observed_countdown_fixture(800.0, 92.0, o);
            require(unsupported.status == CountdownStatus::unavailable && !unsupported.hours,
                "out-of-domain raw sample became a supported initial state");
        }},
        {"opening no-action trip and loss", [] {
            const auto result = assess_failure_fixture(opening());
            require(result.valid() && !result.avoids_shutdown(), "trip is a valid no-action outcome");
            near(*result.facilities[0].shutdown_after_h, 4.48);
            near(result.facilities[0].trajectory.back().inventory_bbl, 736.0);
            near(*result.total_cost, 5016.0);
            near(*result.truck_cost, 0.0);
        }},
        {"opening full-load truck and return", [] {
            const auto result = assess_failure_fixture(opening(), {{}, {{"opening-facility", 0, 0.0, 1}}});
            require(result.avoids_shutdown(), "opening truck should prevent trip");
            near(*result.total_cost, 905.0);
            near(result.facilities[0].trajectory.back().inventory_bbl, 700.0);
            near(result.calendars[0].arrivals_h[0], 1.25);
            near(result.calendars[0].free_after_h, 1.25 + 230.0 / 60.0);
        }},
        {"truck removal keeps both sides of the jump", [] {
            const auto result = assess_failure_fixture(small(), truck());
            require(result.avoids_shutdown(), "one full load should prevent shutdown");
            near(*result.total_cost, 7.0);
            const auto& trace = result.facilities[0].trajectory;
            near(trace[1].time_h, 1.0); near(trace[1].inventory_bbl, 40.0);
            near(trace[2].time_h, 1.0); near(trace[2].inventory_bbl, 20.0);
            near(trace.back().inventory_bbl, 70.0);
        }},
        {"repeat trip arrivals and return", [] {
            const auto result = assess_failure_fixture(small(), truck(0.0, 3));
            require(result.avoids_shutdown(), "repeat job rejected");
            const auto& c = result.calendars[0];
            require(c.arrivals_h == std::vector<double>({1.0, 3.0, 5.0}), "wrong cycle arithmetic");
            near(c.free_after_h, 7.0); near(*result.total_cost, 21.0);
            near(result.facilities[0].trajectory.back().inventory_bbl, 30.0);
        }},
        {"truck too late cannot restart shut wells", [] {
            const auto result = assess_failure_fixture(small(), truck(4.5));
            invalid(result, "arrival_at_or_after_shutdown");
            near(*result.facilities[0].shutdown_after_h, 5.0);
            require(!result.facilities[0].complete, "invalid partial trace marked complete");
        }},
        {"threshold wins exact truck tie", [] {
            const auto result = assess_failure_fixture(small(), truck(4.0));
            invalid(result, "arrival_at_or_after_shutdown");
            near(*result.facilities[0].shutdown_after_h, 5.0);
        }},
        {"full fixed load cannot exceed inventory", [] {
            auto f = small(); f.truck_capacity_bbl = 41.0;
            invalid(assess_failure_fixture(f, truck()), "insufficient_full_load_inventory");
            f.truck_capacity_bbl = 40.0;
            const auto result = assess_failure_fixture(f, truck());
            require(result.valid(), "exact inventory full load should pass");
            near(result.facilities[0].trajectory[2].inventory_bbl, 0.0);
        }},
        {"simultaneous full loads are checked together", [] {
            auto f = small(); f.truck_capacity_bbl = 21.0;
            Response r{{}, {{"facility", 0, 0.0, 1}, {"facility", 1, 0.0, 1}}};
            invalid(assess_failure_fixture(f, r), "insufficient_full_load_inventory");
            f.truck_capacity_bbl = 20.0;
            const auto result = assess_failure_fixture(f, r);
            require(result.valid(), "two exact simultaneous loads rejected");
            near(result.facilities[0].trajectory[2].inventory_bbl, 0.0);
            near(*result.total_cost, 14.0);
        }},
        {"every load must precede repair strictly", [] {
            auto f = small(); f.facilities[0].remaining_repair_h = 1.0;
            invalid(assess_failure_fixture(f, truck()), "arrival_not_before_repair");
            f.facilities[0].remaining_repair_h = std::nextafter(1.0, 2.0);
            require(assess_failure_fixture(f, truck()).valid(), "one ULP before repair rejected");
            f.facilities[0].remaining_repair_h = std::nextafter(1.0, 0.0);
            invalid(assess_failure_fixture(f, truck()), "arrival_not_before_repair");
        }},
        {"last repeat arrival also respects repair", [] {
            invalid(assess_failure_fixture(small(), truck(0.0, 4)), "arrival_not_before_repair");
        }},
        {"immediate well shut-in cost", [] {
            const auto result = assess_failure_fixture(small(), {{{"facility", "cheap", 0.0}}, {}});
            require(result.avoids_shutdown(), "immediate shut-in did not prevent trip");
            near(*result.total_cost, 60.0);
            near(result.facilities[0].trajectory.back().inventory_bbl, 60.0);
        }},
        {"delayed well shut-in cost", [] {
            const auto result = assess_failure_fixture(small(), {{{"facility", "cheap", 2.0}}, {}});
            require(result.avoids_shutdown(), "delayed shut-in did not prevent trip");
            near(*result.total_cost, 40.0);
            near(result.facilities[0].trajectory.back().inventory_bbl, 70.0);
        }},
        {"each well has its own start and revenue", [] {
            const auto result = assess_failure_fixture(small(),
                {{{"facility", "cheap", 1.0}, {"facility", "dear", 3.0}}, {}});
            require(result.avoids_shutdown(), "separate shut-ins rejected");
            near(*result.total_cost, 50.0 + 60.0);
            near(result.facilities[0].trajectory.back().inventory_bbl, 50.0);
        }},
        {"mixed response costs are itemized", [] {
            const auto result = assess_failure_fixture(small(), {{{"facility", "cheap", 2.0}}, truck().truck_jobs});
            require(result.avoids_shutdown(), "mixed response rejected");
            near(*result.truck_cost, 7.0); near(*result.lost_revenue, 40.0);
            near(*result.total_cost, 47.0);
            near(result.facilities[0].trajectory.back().inventory_bbl, 50.0);
        }},
        {"automatic loss does not double count deliberate loss", [] {
            auto f = small(); f.facilities[0].remaining_repair_h = 12.0;
            const auto result = assess_failure_fixture(f, {{{"facility", "cheap", 2.0}}, {}});
            require(result.valid(), "no-truck mixed shut-in model failed");
            near(*result.facilities[0].shutdown_after_h, 8.0);
            near(*result.total_cost, 10.0 * 10.0 + 20.0 * 4.0);
        }},
        {"threshold wins a simultaneous well shut-in", [] {
            const auto result = assess_failure_fixture(small(), {{{"facility", "cheap", 5.0}}, {}});
            require(result.valid() && !result.avoids_shutdown(), "late shut-in undid trip");
            near(*result.facilities[0].shutdown_after_h, 5.0); near(*result.total_cost, 30.0);
        }},
        {"threshold at repair reports trip with zero loss", [] {
            auto f = small(); f.facilities[0].remaining_repair_h = 5.0;
            const auto result = assess_failure_fixture(f);
            require(result.valid() && !result.avoids_shutdown(), "repair tie policy changed");
            near(*result.facilities[0].shutdown_after_h, 5.0); near(*result.total_cost, 0.0);
        }},
        {"already shut-in exposure remains until repair", [] {
            auto f = small(); f.facilities[0].initial_level_pct = 80.0;
            f.facilities[0].already_shut_in = true;
            const auto result = assess_failure_fixture(f);
            require(result.valid() && !result.avoids_shutdown(), "already-shut state discarded");
            near(*result.total_cost, 180.0);
            near(result.facilities[0].trajectory.back().inventory_bbl, 80.0);
            invalid(assess_failure_fixture(f, truck()), "arrival_at_or_after_shutdown");
        }},
        {"above-threshold finite inventory is preserved", [] {
            auto f = small(); f.facilities[0].initial_level_pct = 85.0;
            const auto result = assess_failure_fixture(f);
            require(result.valid(), "bounded already-reached state should be explicit");
            near(result.facilities[0].trajectory.front().inventory_bbl, 85.0);
            near(result.facilities[0].trajectory.back().inventory_bbl, 85.0);
            near(*result.facilities[0].shutdown_after_h, 0.0);
        }},
        {"preexisting unproductive wells add no exposure", [] {
            auto f = small(); f.facilities[0].wells[1].productive = false;
            const auto result = assess_failure_fixture(f);
            require(result.avoids_shutdown(), "unproductive inflow was included");
            near(result.facilities[0].trajectory.back().inventory_bbl, 60.0);
            near(*result.total_cost, 0.0);
            invalid(assess_failure_fixture(f, {{{"facility", "dear", 0.0}}, {}}), "unknown_or_unproductive_well");
        }},
        {"two failures cannot double book shared truck", [] {
            auto f = small(); f.fleets[0].size = 1; duplicate_facility(f, "other");
            invalid(assess_failure_fixture(f, {{}, {{"facility", 0, 0.0, 1}, {"other", 0, 0.0, 1}}}),
                "truck_resource_overlap");
        }},
        {"three failures compete for two trucks", [] {
            auto f = small(); duplicate_facility(f, "other"); duplicate_facility(f, "third");
            invalid(assess_failure_fixture(f, {{}, {{"facility", 0, 0.0, 1}, {"other", 1, 0.0, 1},
                {"third", 0, 0.0, 1}}}), "truck_resource_overlap");
        }},
        {"return occupancy prevents early reassignment", [] {
            auto f = small(); f.fleets[0].size = 1; duplicate_facility(f, "other");
            invalid(assess_failure_fixture(f, {{}, {{"facility", 0, 0.0, 1}, {"other", 0, 2.0, 1}}}),
                "truck_resource_overlap");
        }},
        {"sequential jobs restart from yard", [] {
            auto f = small(); f.fleets[0].size = 1; duplicate_facility(f, "other");
            const auto result = assess_failure_fixture(f,
                {{}, {{"facility", 0, 0.0, 1}, {"other", 0, 3.0, 1}}});
            require(result.avoids_shutdown(), "return-boundary reassignment should pass");
            near(result.calendars[0].free_after_h, 3.0);
            near(result.calendars[1].arrivals_h[0], 4.0);
            near(result.calendars[1].free_after_h, 6.0);
            near(*result.total_cost, 14.0);
        }},
        {"one ULP overlapping return is rejected", [] {
            auto f = small(); duplicate_facility(f, "other");
            invalid(assess_failure_fixture(f, {{}, {{"facility", 0, 0.0, 1},
                {"other", 0, std::nextafter(3.0, 0.0), 1}}}), "truck_resource_overlap");
        }},
        {"fleet identity is local to each system", [] {
            auto f = small(); f.fleets.push_back({"different-system", 1});
            duplicate_facility(f, "other", "different-system");
            const auto result = assess_failure_fixture(f,
                {{}, {{"facility", 0, 0.0, 1}, {"other", 0, 0.0, 1}}});
            require(result.avoids_shutdown(), "separate systems incorrectly share trucks");
            near(*result.total_cost, 14.0);
        }},
        {"response order does not change simultaneous result", [] {
            auto f = small(); duplicate_facility(f, "other");
            Response r{{{"facility", "cheap", 1.0}}, {{"facility", 0, 0.0, 1}, {"other", 1, 0.0, 1}}};
            const auto a = assess_failure_fixture(f, r);
            std::reverse(r.truck_jobs.begin(), r.truck_jobs.end());
            std::reverse(f.facilities.begin(), f.facilities.end());
            const auto b = assess_failure_fixture(f, r);
            require(a.valid() && b.valid(), "permuted response failed");
            near(*a.total_cost, *b.total_cost);
        }},
        {"unknown IDs are explicit violations", [] {
            invalid(assess_failure_fixture(small(), {{}, {{"missing", 0, 0.0, 1}}}), "unknown_facility");
            invalid(assess_failure_fixture(small(), {{{"facility", "missing", 0.0}}, {}}), "unknown_or_unproductive_well");
            invalid(assess_failure_fixture(small(), {{}, {{"facility", 2, 0.0, 1}}}), "truck_outside_fleet");
        }},
        {"duplicate shut-ins and invalid action times", [] {
            invalid(assess_failure_fixture(small(), {{{"facility", "cheap", 0.0},
                {"facility", "cheap", 1.0}}, {}}), "duplicate_shut_in");
            for (const double t : {-1.0, 6.0, std::numeric_limits<double>::infinity(),
                std::numeric_limits<double>::quiet_NaN()})
                invalid(assess_failure_fixture(small(), {{{"facility", "cheap", t}}, {}}), "shut_in_outside_repair");
            invalid(assess_failure_fixture(small(), truck(-1.0)), "invalid_or_oversized_job");
            invalid(assess_failure_fixture(small(), truck(0.0, 0)), "invalid_or_oversized_job");
        }},
        {"bounded work and malformed fixtures", [] {
            invalid(assess_failure_fixture(small(), truck(0.0, max_total_trips + 1)), "invalid_or_oversized_job");
            auto f = small(); f.facilities[0].remaining_repair_h = 169.0;
            rejects([&] { assess_failure_fixture(f); });
            f = small(); f.facilities[0].cycle_min = 0.0;
            rejects([&] { assess_failure_fixture(f); });
            f = small(); f.facilities[0].system_id = "unregistered";
            rejects([&] { assess_failure_fixture(f); });
            f = small(); f.facilities[0].wells.push_back(f.facilities[0].wells[0]);
            rejects([&] { assess_failure_fixture(f); });
            f = small(); f.facilities.push_back(f.facilities[0]);
            rejects([&] { assess_failure_fixture(f); });
        }},
        {"nonfinite observations and arithmetic overflow", [] {
            for (const double x : {std::numeric_limits<double>::quiet_NaN(),
                std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()}) {
                rejects([&] { volume_bbl(100.0, x); });
                rejects([&] { countdown_h(30.0, 80.0, x); });
                auto f = small(); f.facilities[0].wells[0].water_bpd = x;
                rejects([&] { assess_failure_fixture(f); });
            }
            auto f = small(); f.prices.oil_per_bbl = std::numeric_limits<double>::max();
            rejects([&] { assess_failure_fixture(f); });
            rejects([] { countdown_h(0.0, 1.0, std::numeric_limits<double>::denorm_min()); });
        }},
        {"analytical random no-action crossing and revenue oracle", [] {
            std::mt19937_64 rng(20261108);
            std::uniform_real_distribution<double> level(5.0, 70.0), rate(1.0, 500.0), repair(0.1, 12.0);
            for (int k = 0; k < 250; ++k) {
                auto f = small(); auto& a = f.facilities[0];
                a.initial_level_pct = level(rng); a.remaining_repair_h = repair(rng);
                for (auto& w : a.wells) w.water_bpd = rate(rng);
                const double water = (a.wells[0].water_bpd + a.wells[1].water_bpd) / 24.0;
                const double crossing = (80.0 - a.initial_level_pct) / water;
                const auto result = assess_failure_fixture(f);
                require(result.valid(), "random valid model failed");
                if (crossing <= a.remaining_repair_h) {
                    require(result.facilities[0].shutdown_after_h.has_value(), "missed random threshold");
                    near(*result.facilities[0].shutdown_after_h, crossing);
                    near(*result.total_cost, 30.0 * (a.remaining_repair_h - crossing));
                    near(result.facilities[0].trajectory.back().inventory_bbl, 80.0);
                } else {
                    require(result.avoids_shutdown(), "early random threshold");
                    near(*result.total_cost, 0.0);
                    near(result.facilities[0].trajectory.back().inventory_bbl,
                        a.initial_level_pct + water * a.remaining_repair_h);
                }
            }
        }},
        {"random scheduled responses satisfy separate integral ledgers", [] {
            std::mt19937_64 rng(44);
            std::uniform_real_distribution<double> start(0.0, 5.5), flow(60.0, 180.0);
            for (int k = 0; k < 300; ++k) {
                auto f = small(); auto& a = f.facilities[0];
                a.wells[0].water_bpd = flow(rng); a.wells[1].water_bpd = flow(rng);
                const double t0 = start(rng), t1 = start(rng);
                Response r{{{"facility", "cheap", t0}, {"facility", "dear", t1}}, {}};
                if (k % 2 == 0) r.truck_jobs.push_back({"facility", 0, 0.0, 1});
                const auto result = assess_failure_fixture(f, r);
                require(result.valid(), "random supported response failed");
                const auto& outcome = result.facilities[0];
                const double trip = outcome.shutdown_after_h.value_or(6.0);
                for (const auto& p : outcome.trajectory) {
                    double expected = 30.0;
                    expected += a.wells[0].water_bpd / 24.0 * std::min({p.time_h, trip, t0});
                    expected += a.wells[1].water_bpd / 24.0 * std::min({p.time_h, trip, t1});
                    if (!r.truck_jobs.empty() && (p.time_h > 1.0 ||
                        (p.time_h == 1.0 && p.phase == "after_actions"))) expected -= 20.0;
                    near(p.inventory_bbl, expected, 1e-8);
                }
                const double expected_lost = 10.0 * (6.0 - std::min(t0, trip)) +
                                            20.0 * (6.0 - std::min(t1, trip));
                near(*result.lost_revenue, expected_lost);
                near(*result.total_cost, expected_lost + (r.truck_jobs.empty() ? 0.0 : 7.0));
            }
        }}
    };
    int failures = 0;
    for (const auto& [name, run] : tests) {
        try { run(); std::cout << "PASS " << name << '\n'; }
        catch (const std::exception& error) { ++failures; std::cerr << "FAIL " << name << ": " << error.what() << '\n'; }
    }
    std::cout << tests.size() - static_cast<std::size_t>(failures) << "/" << tests.size() << " EOG analytical cases passed\n";
    return failures == 0 ? 0 : 1;
}

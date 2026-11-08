#include "breakwater/eog_simulation.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>

using namespace breakwater::eog::synthetic;
namespace {
std::size_t checks{};
void check(bool value, const char* message) { ++checks; if (!value) throw std::runtime_error(message); }
void near(double value, double expected, double tolerance, const char* message) { check(std::abs(value - expected) <= tolerance, message); }
Scenario base(double duration = 180, double initial = 50) {
    Scenario s; s.duration_s = duration; s.oil_per_bbl = 1; s.gas_per_mcf = 0;
    s.truck_capacity_bbl = 20; s.cost_per_trip = 3; s.fleet_sizes = {1};
    s.wells = {{86400, 172800, 0, true}};
    s.facilities = {{0, 0, 1, 100, initial, 70, 30, 90, 259200, 300000, 2.0 / 60, 20.0 / 60, initial > 70}};
    return s;
}
std::size_t count(const Simulator& sim, Kind kind) {
    return static_cast<std::size_t>(std::count_if(sim.trace().begin(), sim.trace().end(), [=](const auto& p) { return p.kind == kind; }));
}
double first(const Simulator& sim, Kind kind) {
    for (const auto& p : sim.trace()) if (p.kind == kind) return p.elapsed_s;
    throw std::runtime_error("Missing expected event.");
}
void healthy_and_failure() {
    auto s = base(); Simulator sim(s); check(sim.advance(180), "healthy completion");
    const auto a = sim.snapshot(); check(a.status == Status::completed, "completed status");
    near(a.facilities[0].volume_bbl, 50, 0, "analytical healthy final volume");
    near(a.facilities[0].produced_bbl, 180, 0, "healthy production ledger");
    near(a.facilities[0].pumped_bbl, 180, 0, "healthy pump ledger");
    check(a.total_cost == 0, "no healthy loss");
    check(first(sim, Kind::pump_start) == 20 && first(sim, Kind::pump_stop) == 40, "analytical hysteresis crossings");
    check(count(sim, Kind::pump_start) == 3 && count(sim, Kind::pump_stop) == 3, "healthy cycle count");
    s.failures = {{0, 10, 100}}; Simulator broken(s); check(broken.advance(180), "failure/repair completion");
    const auto b = broken.snapshot();
    check(first(broken, Kind::automatic_shut_in) == 40 && first(broken, Kind::repair) == 110, "analytical trip/repair");
    check(b.lost_revenue == 140 && b.wells[0].lost_revenue == 140, "70 seconds union loss at 2/s");
    check(b.facilities[0].volume_bbl == 70 && b.facilities[0].pump_status == 1 && b.wells[0].producing, "repair resumes normal cycling");
    check(b.facilities[0].balance_residual_bbl == 0, "failure mass balance");
}
void discrete_ties_and_repeat_loads() {
    auto s = base(20, 80); s.failures = {{0, 0, 20}};
    s.truck_capacity_bbl = 10; s.facilities[0].response_min = 10.0 / 60;
    Simulator tied(s); check(tied.dispatch(0, 0, 1).accepted, "tie dispatch accepted");
    check(tied.advance(20), "tie run completed"); const auto a = tied.snapshot();
    check(first(tied, Kind::automatic_shut_in) == 10, "threshold wins pickup tie");
    check(a.lost_revenue == 20 && a.truck_cost == 3, "late pickup cannot undo automatic loss");
    check(a.completed_full_loads == 1 && a.facilities[0].trucked_bbl == 10, "full tie load");
    check(a.wells[0].producing && a.trucks[0].busy, "repair releases well, not busy truck");
    auto repeated = base(20, 80); repeated.failures = {{0, 0, 20}}; repeated.facilities[0].cycle_min = 5.0 / 60;
    Simulator loads(repeated); check(loads.dispatch(0, 0, 3).accepted, "three repeat loads accepted");
    check(loads.advance(20), "repeat completed"); const auto b = loads.snapshot();
    check(b.completed_full_loads == 3 && b.truck_cost == 9, "repeat count and cost");
    check(b.facilities[0].volume_bbl == 40 && b.facilities[0].trucked_bbl == 60, "repeat inventory ledger");
    check(b.lost_revenue == 0 && !b.trucks[0].busy && count(loads, Kind::truck_return) == 1, "single final yard return");
    auto invalid = base(20, 10); invalid.truck_capacity_bbl = 50; invalid.failures = {{0, 0, 20}};
    Simulator short_stock(invalid); check(short_stock.dispatch(0, 0, 1).accepted, "dispatch does not borrow future inventory");
    check(!short_stock.advance(3), "short full pickup invalid"); const auto c = short_stock.snapshot();
    check(c.status == Status::invalid_action && !c.total_cost, "invalid action has no certified cost");
    check(c.facilities[0].volume_bbl == 12 && c.completed_full_loads == 0, "no partial load or clipping");
    auto deadline = base(20, 80); deadline.failures = {{0, 0, 20}}; deadline.facilities[0].response_min = 20.0 / 60;
    Simulator rejected(deadline); check(!rejected.dispatch(0, 0, 1).accepted, "arrival at repair rejected");
    check(!rejected.snapshot().trucks[0].busy && rejected.snapshot().truck_cost == 0, "rejected dispatch leaves fleet/cost intact");
}
void persistent_fleet_and_repeated_failures() {
    auto s = base(50, 80); s.wells.push_back(s.wells[0]); auto f = s.facilities[0]; f.first_well = 1; s.facilities.push_back(f);
    s.failures = {{0, 0, 10}, {1, 12, 20}, {0, 30, 10}};
    Simulator sim(s); check(sim.dispatch(0, 0, 1).accepted, "first shared job accepted");
    check(sim.advance(12), "advance across first repair/new failure");
    check(sim.snapshot().trucks[0].busy && sim.snapshot().trucks[0].available_again_s == 22, "repair preserves final-return commitment");
    check(!sim.dispatch(1, 0, 1).accepted, "concurrent later failure cannot steal busy truck");
    check(sim.advance(22), "advance to yard return"); check(!sim.snapshot().trucks[0].busy, "half-open calendar at exact return");
    check(sim.dispatch(1, 0, 1).accepted, "sequential job starts from yard");
    check(sim.advance(32), "second repair"); check(sim.snapshot().trucks[0].available_again_s == 44, "new response and final return");
    check(!sim.dispatch(0, 0, 1).accepted, "repeat incident respects existing reservation");
    check(sim.advance(50), "persistent fleet run complete");
    check(sim.snapshot().completed_full_loads == 2 && count(sim, Kind::repair) == 3, "multiple incident resource ledger");
}
void well_union_and_rate_forcing() {
    auto s = base(20, 80); s.failures = {{0, 0, 20}};
    s.wells = {{86400, 8640, 0, true}, {86400, 864000, 0, true}}; s.facilities[0].well_count = 2;
    s.cost_per_trip = 1;
    Simulator mixed(s); check(mixed.dispatch(0, 0, 1).accepted, "mixed dispatch");
    check(mixed.advance(10), "delayed stop clock"); check(mixed.shut_in(0).accepted, "delayed cheap well stop");
    check(mixed.advance(20), "mixed completed"); near(*mixed.snapshot().total_cost, 2, 1e-14, "mixed known cost");
    Simulator overlap(s); check(overlap.shut_in(0).accepted, "voluntary cheap well stop");
    check(overlap.advance(20), "overlap completed"); near(*overlap.snapshot().lost_revenue, 102, 1e-12, "voluntary/automatic union has no double counting");
    auto changing = base(20, 80); changing.failures = {{0, 0, 20}}; changing.rate_changes = {{0, 5, 172800, 518400, 0}};
    Simulator forced(changing); check(forced.advance(3), "forcing early stop clock"); check(forced.shut_in(0).accepted, "forcing well stop");
    check(forced.advance(20), "forcing repair complete"); const auto a = forced.snapshot();
    check(a.lost_revenue == 94 && a.wells[0].lost_revenue == 94, "piecewise economics loss ledger");
    check(a.wells[0].producing && a.wells[0].water_bpd == 172800, "repair resumes current forced rates");
}
void refinement_and_long_run() {
    std::mt19937_64 random(0x5e0f2046);
    for (int trial = 0; trial < 100; ++trial) {
        auto s = base(7200, 35 + static_cast<double>(random() % 20));
        const double q = 0.2 + static_cast<double>(random() % 70) / 100;
        s.wells[0].water_bpd = q * 86400; s.wells[0].oil_bpd = (0.3 + q) * 86400;
        s.failures = {{0, 100.25, 400.75}, {0, 2000.125, 600.875}};
        s.rate_changes = {{0, 1500.5, q * 86400, (0.3 + q) * 86400, 0}}; // zero-effect phase refinement
        Simulator single(s), stepped(s); check(single.advance(7200), "long single advance");
        for (double t = 17.25; t < 7200; t += 17.25) check(stepped.advance(t), "long stepped advance");
        check(stepped.advance(7200), "long stepped completion");
        const auto a = single.snapshot(), b = stepped.snapshot();
        check(a.facilities[0].volume_bbl == b.facilities[0].volume_bbl && a.total_cost == b.total_cost,
              "observation partition cannot change physical result");
        check(a.facilities[0].produced_bbl == b.facilities[0].produced_bbl && a.facilities[0].pumped_bbl == b.facilities[0].pumped_bbl,
              "observation partition cannot change mass ledger");
        near(a.facilities[0].balance_residual_bbl, 0, 5e-11, "long mass residual bound");
        near(*a.lost_revenue, a.wells[0].lost_revenue, 2e-10, "independent well/facility revenue ledger agreement");
        for (const auto& p : single.trace()) check(p.volume_bbl >= 0 && p.volume_bbl <= 100, "long raw inventory stays physical");
    }
}
void bounds_reset_and_rejections() {
    auto s = base(); Simulator sim(s);
    check(!sim.advance(180, [] { return true; }), "cooperative cancellation");
    check(sim.snapshot().status == Status::cancelled && !sim.snapshot().total_cost, "cancelled has no completion cost");
    sim.reset(); check(sim.snapshot().elapsed_s == 0 && sim.advance(180), "reset is a fresh deterministic plant");
    s.point_limit = 3; Simulator bounded(s); check(!bounded.advance(180), "trace bound stops explicitly");
    check(bounded.snapshot().status == Status::limit && bounded.trace().size() == 3 && !bounded.snapshot().total_cost, "bounded trace no decimation/cost");
    s = base(); s.facilities[0].initial_bbl = -1;
    try { Simulator invalid(s); check(false, "invalid volume was accepted"); } catch (const std::invalid_argument&) { check(true, "negative volume rejected"); }
    s = base(); s.failures = {{0, 1, 10}, {0, 5, 20}};
    try { Simulator invalid(s); check(false, "overlapping failures accepted"); } catch (const std::invalid_argument&) { check(true, "overlap rejected"); }
    s = base(); s.rate_changes = {{0, 10, 1, 2, 0}, {0, 10, 2, 3, 0}};
    try { Simulator invalid(s); check(false, "duplicate forcing accepted"); } catch (const std::invalid_argument&) { check(true, "duplicate forcing rejected"); }
    s = base(); s.wells[0].water_bpd = 864000; s.facilities[0].pump_bpd = 86400;
    Simulator overflowing(s); check(!overflowing.advance(180), "healthy capacity overload explicit");
    check(overflowing.snapshot().status == Status::unavailable && !overflowing.snapshot().total_cost, "overflow is not silently clamped or costed");
}

void month_scale_independent_cycle_oracle() {
    auto s = base(31 * 86400.0, 31.25);
    const double q = 0.37, p = 1.91;
    s.wells[0].water_bpd = q * 86400; s.facilities[0].pump_bpd = p * 86400;
    Simulator sim(s); check(sim.advance(s.duration_s), "31-day synthetic run completed");
    const double down = 40 / (p - q), up = 40 / q;
    const double remainder = std::fmod(s.duration_s - (70 - 31.25) / q, down + up);
    const double expected = remainder < down ? 70 - (p - q) * remainder : 30 + q * (remainder - down);
    const auto a = sim.snapshot();
    near(a.facilities[0].volume_bbl, expected, 1e-5, "31-day independent periodic-cycle oracle");
    near(a.facilities[0].produced_bbl, q * s.duration_s, 1e-6, "31-day independent production integral");
    near(a.facilities[0].balance_residual_bbl, 0, 1e-6, "31-day mass residual");
    check(a.total_cost == 0 && sim.trace().size() < s.point_limit, "31-day cost/output bound");
    sim.reset(); check(sim.advance(s.duration_s), "31-day deterministic reset");
    check(sim.snapshot().facilities[0].volume_bbl == a.facilities[0].volume_bbl && sim.snapshot().facilities[0].produced_bbl == a.facilities[0].produced_bbl,
          "31-day exact reset repeatability");
}
}
int main() {
    try {
        healthy_and_failure(); discrete_ties_and_repeat_loads(); persistent_fleet_and_repeated_failures();
        well_union_and_rate_forcing(); refinement_and_long_run(); bounds_reset_and_rejections(); month_scale_independent_cycle_oracle();
        std::cout << checks << " synthetic-event checks passed (100 long-run observation-partition comparisons)\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << "FAIL after " << checks << ": " << error.what() << '\n'; return 1; }
}

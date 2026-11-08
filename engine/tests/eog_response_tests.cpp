#include "breakwater/eog_response.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
uint32_t checks = 0;
void check(bool value, const char* why) { ++checks; if (!value) throw std::runtime_error(why); }
void near(double a, double b, double tolerance = 1e-10) {
    check(std::abs(a - b) <= tolerance * std::max(1.0, std::abs(b)), "numerical mismatch");
}
struct Result {
    bweog_result* handle = nullptr;
    bweog_result_view v{};
    int code;
    Result(const bweog_snapshot& s, const bweog_plan& p) : code(bweog_evaluate(&s, &p, &handle)) {
        if (!code) check(bweog_get_result(handle, &v) == 0, "result accessor");
    }
    ~Result() { bweog_destroy(handle); }
    Result(const Result&) = delete;
    Result& operator=(const Result&) = delete;
    bool has(uint32_t code_) const {
        for (uint32_t i = 0; i < v.violation_count; ++i) if (v.violations[i].code == code_) return true;
        return false;
    }
};
struct Fixture {
    std::vector<bweog_fleet> fleets{{2}};
    std::vector<bweog_facility> facilities{{0, 0, 1, 0, 1000, 400, 920, 7200, 30, 60}};
    std::vector<bweog_well> wells{{2400, 24, 240, 1}};
    std::vector<bweog_horizon> horizons{{0, 7200}};
    std::vector<bweog_stop> stops;
    std::vector<bweog_job> jobs;
    std::vector<bweog_trip> trips;
    bweog_snapshot s{};
    bweog_plan p{};
    Fixture() { s.oil_per_bbl = 60; s.gas_per_mcf = 4; s.truck_capacity_bbl = 150; s.cost_per_trip = 100; }
    void sync() {
        s.abi_version = p.abi_version = BWEOG_ABI_VERSION;
        s.struct_size = sizeof(s); p.struct_size = sizeof(p);
        s.fleets = fleets.data(); s.facilities = facilities.data(); s.wells = wells.data();
        s.fleet_count = static_cast<uint32_t>(fleets.size());
        s.facility_count = static_cast<uint32_t>(facilities.size()); s.well_count = static_cast<uint32_t>(wells.size());
        p.horizons = horizons.data(); p.stops = stops.data(); p.jobs = jobs.data(); p.trips = trips.data();
        p.horizon_count = static_cast<uint32_t>(horizons.size()); p.stop_count = static_cast<uint32_t>(stops.size());
        p.job_count = static_cast<uint32_t>(jobs.size()); p.trip_count = static_cast<uint32_t>(trips.size());
    }
    void add_job(uint32_t fi, uint32_t truck, double call, uint32_t count) {
        const auto& f = facilities[fi]; const double first = call + f.response_min * 60, cycle = f.cycle_min * 60;
        jobs.push_back({fi, f.system, truck, static_cast<uint32_t>(trips.size()), count, call,
                        f.response_min, f.cycle_min, first + static_cast<double>(count - 1) * cycle + cycle});
        for (uint32_t k = 0; k < count; ++k)
            trips.push_back({first + static_cast<double>(k) * cycle, s.truck_capacity_bbl, s.cost_per_trip});
    }
    void second_facility() {
        auto f = facilities[0]; f.first_well = 1; facilities.push_back(f); wells.push_back(wells[0]);
        horizons.push_back({1, f.repair_s});
    }
    void invalid(uint32_t violation) {
        sync(); Result r(s, p); check(r.code == 0, "structured invalid plan");
        check(r.v.status == BWEOG_INVALID && r.has(violation), "expected plan violation");
        check(std::isnan(r.v.total_cost), "invalid plan must not expose cost");
    }
};

void golden_cases() {
    Fixture f; f.sync(); Result baseline(f.s, f.p);
    check(!baseline.code && baseline.v.status == BWEOG_AVAILABLE, "no action available");
    near(baseline.v.facilities[0].final_bbl, 600); near(baseline.v.total_cost, 0);
    f.facilities[0].initial_bbl = 800; f.sync(); Result stopped(f.s, f.p);
    check(stopped.v.automatic_shutdown == 1, "automatic option remains admissible");
    near(stopped.v.facilities[0].shutdown_s, 4320); near(stopped.v.total_cost, 80);
    near(stopped.v.facilities[0].balance_residual_bbl, 0);
    f.add_job(0, 0, 0, 2); f.sync(); Result hauled(f.s, f.p);
    check(hauled.v.status == BWEOG_AVAILABLE && !hauled.v.automatic_shutdown, "full repeat trips");
    near(hauled.v.total_cost, 200); near(hauled.v.facilities[0].final_bbl, 700);
    check(hauled.v.point_count == 6, "before/after jumps retained without triple timestamps");
    f.jobs.clear(); f.trips.clear(); f.stops.push_back({0, 0, 3600, 7200}); f.sync(); Result well(f.s, f.p);
    near(well.v.total_cost, 100); near(well.v.facilities[0].final_bbl, 900);
    check(!well.v.automatic_shutdown, "delayed well-only prevention");
}
void exact_ordering() {
    Fixture f; f.facilities[0] = {0, 0, 1, 0, 1000, 0, 100, 200, 1, 1};
    f.wells[0] = {86400, 86400, 0, 1}; f.s.oil_per_bbl = 1; f.horizons[0].repair_s = 200;
    f.s.truck_capacity_bbl = 50;
    f.stops = {{0, 0, 100, 200}}; f.sync(); Result tie(f.s, f.p);
    check(tie.v.status == BWEOG_AVAILABLE && tie.v.automatic_shutdown, "threshold precedes well tie");
    near(tie.v.total_cost, 100); near(tie.v.facilities[0].shutdown_s, 100);
    f.stops[0].start_s = std::nextafter(100.0, 0.0); f.sync(); Result before(f.s, f.p);
    check(before.v.status == BWEOG_AVAILABLE && !before.v.automatic_shutdown, "one ULP before crossing");
    f.stops[0].start_s = std::nextafter(100.0, 200.0); f.sync(); Result after(f.s, f.p);
    check(after.v.status == BWEOG_AVAILABLE && after.v.automatic_shutdown, "one ULP after crossing");
    f.stops.clear(); f.facilities[0].repair_s = f.horizons[0].repair_s = 100; f.sync(); Result repair(f.s, f.p);
    check(repair.v.automatic_shutdown == 1, "threshold at repair recorded"); near(repair.v.total_cost, 0);
    f.facilities[0].response_min = 1; f.facilities[0].cycle_min = 1;
    f.add_job(0, 0, 40, 1); f.invalid(BWEOG_TRIP); // arrival exactly at repair
    f.jobs.clear(); f.trips.clear(); f.facilities[0].repair_s = f.horizons[0].repair_s = 200;
    f.add_job(0, 0, 40, 1); f.sync(); Result late(f.s, f.p);
    check(late.v.status == BWEOG_AVAILABLE && late.v.automatic_shutdown, "pickup cannot undo threshold tie");
    near(late.v.facilities[0].final_bbl, 50); near(late.v.total_cost, 200);
    // A zero-rate well action at a fractional time does not re-anchor the flow.
    f.jobs.clear(); f.trips.clear(); f.wells.push_back({0, 0, 0, 1}); f.facilities[0].well_count = 2;
    f.stops = {{0, 1, 17.123456789, 200}}; f.sync(); Result noop(f.s, f.p);
    near(noop.v.facilities[0].shutdown_s, 100, 0);
    // At this scale adding a sub-ULP volume increment rounds up to threshold,
    // although the retained crossing lies later: expose ambiguity, not clipping.
    Fixture ambiguous; ambiguous.facilities[0] = {0, 0, 1, 0, 1000, 400, 500, 200, 1, 1};
    ambiguous.wells[0].water_bpd = 86400; ambiguous.horizons[0].repair_s = 200;
    ambiguous.stops = {{0, 0, std::nextafter(100.0, 0.0), 200}}; ambiguous.sync();
    Result uncertain(ambiguous.s, ambiguous.p);
    check(uncertain.v.status == BWEOG_UNAVAILABLE && uncertain.has(BWEOG_NUMERICAL), "rounding ambiguity exposed");
}
void proposal_checks() {
    { Fixture f; f.horizons.clear(); f.invalid(BWEOG_HORIZONS); }
    { Fixture f; f.horizons[0].repair_s = std::nextafter(7200.0, 8000.0); f.invalid(BWEOG_HORIZONS); }
    { Fixture f; f.horizons.push_back(f.horizons[0]); f.invalid(BWEOG_HORIZONS); }
    { Fixture f; f.horizons[0].facility = 9; f.invalid(BWEOG_HORIZONS); }
    { Fixture f; f.stops = {{0, 0, -1, 7200}}; f.invalid(BWEOG_STOP); }
    { Fixture f; f.stops = {{0, 0, 0, 7201}}; f.invalid(BWEOG_STOP); }
    { Fixture f; f.stops = {{0, 0, 0, 7200}, {0, 0, 1, 7200}}; f.invalid(BWEOG_STOP); }
    { Fixture f; f.stops = {{8, 0, 0, 7200}}; f.invalid(BWEOG_STOP); }
    { Fixture f; f.add_job(0, 0, 0, 1); f.trips[0].volume_bbl = 149; f.invalid(BWEOG_TRIP); }
    { Fixture f; f.add_job(0, 0, 0, 1); f.trips[0].cost = 99; f.invalid(BWEOG_TRIP); }
    { Fixture f; f.add_job(0, 0, 0, 1); f.trips[0].arrival_s += 1; f.invalid(BWEOG_TRIP); }
    { Fixture f; f.add_job(0, 0, 0, 1); f.jobs[0].response_min = 29; f.invalid(BWEOG_JOB); }
    { Fixture f; f.add_job(0, 0, 0, 1); f.jobs[0].cycle_min = 59; f.invalid(BWEOG_JOB); }
    { Fixture f; f.add_job(0, 0, 0, 1); f.jobs[0].available_again_s -= 1; f.invalid(BWEOG_JOB); }
    { Fixture f; f.add_job(0, 8, 0, 1); f.invalid(BWEOG_JOB); }
    { Fixture f; f.add_job(0, 0, 0, 1); f.jobs[0].system = 1; f.invalid(BWEOG_JOB); }
    { Fixture f; f.add_job(0, 0, 0, 1); f.jobs[0].trip_count = 0; f.invalid(BWEOG_JOB); }
    { Fixture f; f.trips.push_back({100, 150, 100}); f.invalid(BWEOG_TRIP); }
    { Fixture f; f.add_job(0, 0, 0, 1); f.add_job(0, 0, 1, 1); f.invalid(BWEOG_FLEET); }
    { Fixture f; f.second_facility(); f.add_job(0, 0, 0, 1); f.add_job(1, 0, 0, 1); f.invalid(BWEOG_FLEET); }
    { Fixture f; f.facilities[0].initial_bbl = 0; f.add_job(0, 0, 0, 1); f.invalid(BWEOG_INVENTORY); }
    { Fixture f; f.facilities[0].initial_bbl = 200; f.add_job(0, 0, 0, 1); f.add_job(0, 1, 0, 1);
      f.invalid(BWEOG_INVENTORY); } // separately affordable, combined not affordable
    { Fixture f; f.facilities[0].initial_bbl = 150; f.facilities[0].response_min = 0; f.add_job(0, 0, 0, 1);
      f.sync(); Result r(f.s, f.p); check(r.v.status == BWEOG_AVAILABLE, "exact full-load inventory");
      near(r.v.points[1].volume_bbl, 0); }
}
void calendars_and_costs() {
    Fixture f; f.facilities[0].repair_s = f.horizons[0].repair_s = 12000;
    f.add_job(0, 0, 0, 1); f.add_job(0, 0, 5400, 1); f.sync(); Result sequential(f.s, f.p);
    check(sequential.v.status == BWEOG_AVAILABLE, "job may start at previous yard return");
    near(sequential.v.truck_cost, 200);
    f.jobs[1].called_s = std::nextafter(5400.0, 0.0);
    f.trips[1].arrival_s = f.jobs[1].called_s + 1800; f.jobs[1].available_again_s = f.trips[1].arrival_s + 3600;
    f.invalid(BWEOG_FLEET);
    Fixture g; g.second_facility(); g.facilities[1].system = 1; g.fleets.push_back({1});
    g.add_job(0, 0, 0, 1); g.add_job(1, 0, 0, 1); g.sync(); Result separate(g.s, g.p);
    check(separate.v.status == BWEOG_AVAILABLE, "same truck number in separate systems");
    Fixture h; h.facilities[0].initial_bbl = 800;
    h.wells = {{1200, 12, 120, 1}, {1200, 12, 120, 1}}; h.facilities[0].well_count = 2;
    h.stops = {{0, 0, 3600, 7200}}; h.sync(); Result mixed(h.s, h.p);
    check(mixed.v.automatic_shutdown == 1, "other well eventually shuts automatically");
    near(mixed.v.facilities[0].shutdown_s, 5040); near(mixed.v.lost_revenue, 80);
    h.facilities[0].already_shut_in = 1; h.facilities[0].initial_bbl = 930; h.sync(); Result pretrip(h.s, h.p);
    near(pretrip.v.facilities[0].final_bbl, 930); near(pretrip.v.lost_revenue, 200);
    h.wells[1] = {0, 999, 999, 0}; h.sync(); Result inactive(h.s, h.p);
    near(inactive.v.lost_revenue, 100); // inactive pre-trip well carries no opportunity cost
}
void boundary_and_overflow() {
    Fixture f; f.sync(); bweog_result* h = reinterpret_cast<bweog_result*>(1);
    check(bweog_evaluate(nullptr, &f.p, &h) == 1 && h == nullptr, "null snapshot");
    check(bweog_evaluate(&f.s, &f.p, nullptr) == 1, "null output");
    check(bweog_get_result(nullptr, nullptr) == 1, "null result"); bweog_destroy(nullptr);
    f.s.abi_version = 99; Result bad_abi(f.s, f.p); check(bad_abi.code == 1, "ABI rejection");
    f.sync(); f.s.wells = nullptr; Result bad_buffer(f.s, f.p); check(bad_buffer.code == 1, "null buffer");
    f.sync(); f.facilities[0].initial_bbl = -1; Result negative(f.s, f.p);
    check(negative.code == 1, "invalid initial volume not clipped");
    f.facilities[0].initial_bbl = 1001; Result overfull(f.s, f.p); check(overfull.code == 1, "overfull not clipped");
    f.facilities[0].initial_bbl = 400; f.s.oil_per_bbl = std::numeric_limits<double>::infinity();
    Result nonfinite(f.s, f.p); check(nonfinite.code == 1, "nonfinite rejected");
    f.s.oil_per_bbl = 1e308; f.facilities[0].already_shut_in = 1; Result overflow(f.s, f.p);
    check(!overflow.code && overflow.v.status == BWEOG_UNAVAILABLE && std::isnan(overflow.v.total_cost),
          "derived cost overflow unavailable");
    Fixture empty; empty.facilities.clear(); empty.wells.clear(); empty.horizons.clear(); empty.sync();
    Result nothing(empty.s, empty.p); check(nothing.v.status == BWEOG_AVAILABLE, "empty active failure set");
}
void random_independent_noaction_oracle() {
    std::mt19937_64 rng(2026110804);
    std::uniform_real_distribution<double> unit(0.0, 1.0);
    for (uint32_t i = 0; i < 600; ++i) {
        Fixture f;
        const double initial = 200 + unit(rng) * 500, q = 0.001 + unit(rng) * 0.2;
        const double horizon = 100 + unit(rng) * 20000;
        f.facilities[0].initial_bbl = initial; f.facilities[0].repair_s = f.horizons[0].repair_s = horizon;
        f.wells[0].water_bpd = q * 86400; f.sync(); Result r(f.s, f.p);
        check(!r.code && r.v.status == BWEOG_AVAILABLE, "random no-action available");
        // Independently use long double volume/rate/economic equations.
        const long double rate = static_cast<long double>(f.wells[0].water_bpd) / 86400;
        const long double crossing = (920 - static_cast<long double>(initial)) / rate;
        const long double active = std::min(static_cast<long double>(horizon), crossing);
        const long double volume = static_cast<long double>(initial) + rate * active;
        const long double lost = crossing < horizon ? (horizon - crossing) * 100 / 3600 : 0;
        near(r.v.facilities[0].final_bbl, static_cast<double>(volume));
        near(r.v.total_cost, static_cast<double>(lost));
        check(r.v.automatic_shutdown == static_cast<uint32_t>(crossing <= horizon), "random shutdown outcome");
        near(r.v.facilities[0].balance_residual_bbl, 0);
    }
}
void random_independent_well_oracle() {
    std::mt19937_64 rng(2026110805);
    std::uniform_real_distribution<double> unit(0.0, 1.0);
    for (uint32_t i = 0; i < 600; ++i) {
        Fixture f;
        const double initial = 200 + unit(rng) * 500, q = 0.001 + unit(rng) * 0.2;
        const double horizon = 100 + unit(rng) * 20000, stop = horizon * unit(rng);
        f.facilities[0].initial_bbl = initial; f.facilities[0].repair_s = f.horizons[0].repair_s = horizon;
        f.wells[0].water_bpd = q * 86400; f.stops = {{0, 0, stop, horizon}};
        f.sync(); Result r(f.s, f.p);
        check(!r.code && r.v.status == BWEOG_AVAILABLE, "random well plan available");
        const long double rate = static_cast<long double>(f.wells[0].water_bpd) / 86400;
        const long double crossing = (920 - static_cast<long double>(initial)) / rate;
        const long double active = std::min(static_cast<long double>(stop), crossing);
        near(r.v.facilities[0].final_bbl, static_cast<double>(initial + rate * active));
        near(r.v.total_cost, static_cast<double>((horizon - active) * 100 / 3600));
        check(r.v.automatic_shutdown == static_cast<uint32_t>(crossing <= stop), "random well shutdown ordering");
    }
}
void trajectory_budget() {
    Fixture f; f.facilities[0] = {0, 0, 1, 0, 1e9, 9e8, 9.2e8, 10000, 0, 1.0 / 60};
    f.wells[0].water_bpd = 0; f.horizons[0].repair_s = 10000;
    f.add_job(0, 0, 0, 4096); f.sync(); Result r(f.s, f.p);
    check(!r.code && r.v.status == BWEOG_UNAVAILABLE && r.has(BWEOG_LIMIT), "trajectory bound explicit");
    check(std::isnan(r.v.total_cost), "no cost for truncated trajectory");
}
}
int main() {
    try {
        check(bweog_abi_version() == BWEOG_ABI_VERSION, "ABI version");
        golden_cases(); exact_ordering(); proposal_checks(); calendars_and_costs();
        boundary_and_overflow(); random_independent_noaction_oracle(); random_independent_well_oracle(); trajectory_budget();
        std::cout << checks << " response checks passed (600 independent no-action and 600 well-stop randomized cases)\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << "FAIL after " << checks << ": " << e.what() << '\n'; return 1; }
}

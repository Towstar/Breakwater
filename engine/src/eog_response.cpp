#include "breakwater/eog_response.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

struct bweog_result {
    uint32_t status = BWEOG_AVAILABLE;
    bool automatic = false;
    double truck_cost = 0, lost_revenue = 0, total = 0;
    std::vector<bweog_facility_result> facilities;
    std::vector<bweog_point> points;
    std::vector<bweog_violation> violations;
};

namespace {
thread_local std::string last_error;
constexpr uint32_t max_facilities = 256, max_wells = 32768, max_jobs = 256;
constexpr uint32_t max_trips = 16384, max_points = 262144, max_facility_points = 8192;
constexpr double infinity = std::numeric_limits<double>::infinity();
bool nonnegative(double x) { return std::isfinite(x) && x >= 0; }
bool positive(double x) { return std::isfinite(x) && x > 0; }
void require(bool ok, const char* message) { if (!ok) throw std::invalid_argument(message); }
template<class T> bool buffer(const T* p, uint32_t n) { return n == 0 || p != nullptr; }

void violation(bweog_result& r, uint32_t code, uint32_t facility = BWEOG_NO_INDEX,
               uint32_t action = BWEOG_NO_INDEX, double time = -1) {
    if (r.violations.size() < 1024)
        r.violations.push_back({code, facility, action, time >= 0 ? 1u : 0u, time >= 0 ? time : 0});
    if (code == BWEOG_NUMERICAL || code == BWEOG_LIMIT) r.status = BWEOG_UNAVAILABLE;
    else if (r.status == BWEOG_AVAILABLE) r.status = BWEOG_INVALID;
}

void check_snapshot(const bweog_snapshot& s) {
    require(s.abi_version == BWEOG_ABI_VERSION && s.struct_size == sizeof(s), "snapshot ABI mismatch");
    require(s.fleet_count <= 64 && s.facility_count <= max_facilities && s.well_count <= max_wells,
            "snapshot size limit");
    require(buffer(s.fleets, s.fleet_count) && buffer(s.facilities, s.facility_count) &&
            buffer(s.wells, s.well_count), "null snapshot buffer");
    require(nonnegative(s.oil_per_bbl) && nonnegative(s.gas_per_mcf) &&
            positive(s.truck_capacity_bbl) && nonnegative(s.cost_per_trip), "invalid operations/economics");
    for (uint32_t i = 0; i < s.fleet_count; ++i) require(s.fleets[i].trucks <= 256, "fleet size limit");
    uint32_t next = 0;
    for (uint32_t i = 0; i < s.facility_count; ++i) {
        const auto& f = s.facilities[i];
        require(f.system < s.fleet_count && f.first_well == next && f.well_count <= 128 &&
                f.well_count <= s.well_count - next && f.already_shut_in <= 1, "invalid facility topology");
        next += f.well_count;
        require(positive(f.capacity_bbl) && nonnegative(f.initial_bbl) && f.initial_bbl <= f.capacity_bbl &&
                positive(f.shutdown_bbl) && f.shutdown_bbl <= f.capacity_bbl &&
                positive(f.repair_s) && f.repair_s <= 604800 && nonnegative(f.response_min) &&
                f.response_min <= 10080 && positive(f.cycle_min) && f.cycle_min <= 10080,
                "invalid facility state or repair/travel horizon");
    }
    require(next == s.well_count, "unassigned wells");
    for (uint32_t i = 0; i < s.well_count; ++i) {
        const auto& w = s.wells[i];
        require(nonnegative(w.water_bpd) && nonnegative(w.oil_bpd) && nonnegative(w.gas_mcfd) &&
                w.productive <= 1 && (w.productive || w.water_bpd == 0), "invalid well state");
    }
}

void check_plan(const bweog_snapshot& s, const bweog_plan& p, bweog_result& r) {
    require(p.abi_version == BWEOG_ABI_VERSION && p.struct_size == sizeof(p), "plan ABI mismatch");
    require(p.horizon_count <= max_facilities && p.stop_count <= max_jobs && p.job_count <= max_jobs &&
            p.trip_count <= max_trips, "plan size limit");
    require(buffer(p.horizons, p.horizon_count) && buffer(p.stops, p.stop_count) &&
            buffer(p.jobs, p.job_count) && buffer(p.trips, p.trip_count), "null plan buffer");
    std::vector<bool> seen(s.facility_count, false), stopped(s.well_count, false);
    if (p.horizon_count != s.facility_count) violation(r, BWEOG_HORIZONS);
    for (uint32_t i = 0; i < p.horizon_count; ++i) {
        const auto& h = p.horizons[i];
        if (h.facility >= s.facility_count) { violation(r, BWEOG_HORIZONS); continue; }
        if (seen[h.facility] || h.repair_s != s.facilities[h.facility].repair_s)
            violation(r, BWEOG_HORIZONS, h.facility, i);
        seen[h.facility] = true;
    }
    for (uint32_t i = 0; i < p.stop_count; ++i) {
        const auto& a = p.stops[i];
        if (a.facility >= s.facility_count || a.well >= s.well_count) {
            violation(r, BWEOG_STOP, a.facility, i); continue;
        }
        const auto& f = s.facilities[a.facility];
        if (a.well < f.first_well || a.well >= f.first_well + f.well_count || stopped[a.well] ||
            !s.wells[a.well].productive || !nonnegative(a.start_s) || a.start_s > f.repair_s ||
            a.end_s != f.repair_s) violation(r, BWEOG_STOP, a.facility, i);
        stopped[a.well] = true;
    }
    uint32_t next = 0;
    for (uint32_t i = 0; i < p.job_count; ++i) {
        const auto& a = p.jobs[i];
        if (a.facility >= s.facility_count || a.first_trip != next || a.trip_count == 0 ||
            a.trip_count > p.trip_count - next) { violation(r, BWEOG_JOB, a.facility, i); return; }
        next += a.trip_count;
        const auto& f = s.facilities[a.facility];
        if (a.system != f.system || a.truck >= s.fleets[f.system].trucks || !nonnegative(a.called_s) ||
            a.response_min != f.response_min || a.cycle_min != f.cycle_min)
            violation(r, BWEOG_JOB, a.facility, i);
        const double response = f.response_min * 60, cycle = f.cycle_min * 60;
        const double first = a.called_s + response;
        double previous = -1;
        for (uint32_t k = 0; k < a.trip_count; ++k) {
            const auto& t = p.trips[a.first_trip + k];
            const double expected = first + static_cast<double>(k) * cycle;
            if (!nonnegative(t.arrival_s) || t.arrival_s != expected || t.arrival_s <= previous ||
                t.arrival_s >= f.repair_s || t.volume_bbl != s.truck_capacity_bbl || t.cost != s.cost_per_trip)
                violation(r, BWEOG_TRIP, a.facility, i);
            previous = t.arrival_s;
        }
        if (!std::isfinite(a.available_again_s) || a.available_again_s != previous + cycle ||
            a.available_again_s <= previous) violation(r, BWEOG_JOB, a.facility, i);
    }
    if (next != p.trip_count) violation(r, BWEOG_TRIP);
    if (r.status != BWEOG_AVAILABLE) return;
    std::vector<uint32_t> order(p.job_count);
    for (uint32_t i = 0; i < p.job_count; ++i) order[i] = i;
    std::sort(order.begin(), order.end(), [&](uint32_t x, uint32_t y) {
        const auto& a = p.jobs[x]; const auto& b = p.jobs[y];
        return std::tie(a.system, a.truck, a.called_s, x) < std::tie(b.system, b.truck, b.called_s, y);
    });
    for (uint32_t i = 1; i < p.job_count; ++i) {
        const auto& a = p.jobs[order[i - 1]]; const auto& b = p.jobs[order[i]];
        if (a.system == b.system && a.truck == b.truck && b.called_s < a.available_again_s)
            violation(r, BWEOG_FLEET, b.facility, order[i], b.called_s);
    }
}

void check_commitments(const bweog_snapshot& s, const bweog_commitments& c) {
    require(c.abi_version == 1 && c.struct_size == sizeof(c), "committed-state ABI mismatch");
    require(c.occupied_count <= 16384 && c.pickup_count <= max_trips && c.stopped_count <= max_wells,
            "committed-state size limit");
    require(buffer(c.occupied, c.occupied_count) && buffer(c.pickups, c.pickup_count) &&
            buffer(c.stopped_wells, c.stopped_count), "null committed-state buffer");
    std::vector<double> reserved(64 * 256, -1);
    for (uint32_t i = 0; i < c.occupied_count; ++i) {
        const auto& b = c.occupied[i];
        require(b.system < s.fleet_count && b.truck < s.fleets[b.system].trucks && positive(b.available_s),
                "invalid occupied slot");
        const auto key = b.system * 256 + b.truck;
        require(reserved[key] < 0, "duplicate occupied slot"); reserved[key] = b.available_s;
    }
    std::vector<bool> stopped(s.well_count, false);
    for (uint32_t i = 0; i < c.stopped_count; ++i) {
        const auto w = c.stopped_wells[i];
        require(w < s.well_count && s.wells[w].productive && !stopped[w], "invalid prior well stop");
        stopped[w] = true;
    }
    std::vector<uint32_t> order(c.pickup_count), owner(64 * 256, BWEOG_NO_INDEX);
    for (uint32_t i = 0; i < c.pickup_count; ++i) {
        const auto& t = c.pickups[i];
        require(t.facility < s.facility_count && t.system == s.facilities[t.facility].system &&
                t.truck < s.fleets[t.system].trucks && positive(t.arrival_s) &&
                t.arrival_s < s.facilities[t.facility].repair_s, "invalid pending pickup");
        const auto key = t.system * 256 + t.truck;
        require(reserved[key] > t.arrival_s, "pending pickup lacks an occupied return reservation");
        require(owner[key] == BWEOG_NO_INDEX || owner[key] == t.facility, "one occupied job names multiple facilities");
        owner[key] = t.facility; order[i] = i;
    }
    std::sort(order.begin(), order.end(), [&](uint32_t a, uint32_t b) {
        const auto& x = c.pickups[a]; const auto& y = c.pickups[b];
        return std::tie(x.system, x.truck, x.arrival_s) < std::tie(y.system, y.truck, y.arrival_s);
    });
    for (uint32_t i = 1; i < order.size(); ++i) {
        const auto& a = c.pickups[order[i - 1]]; const auto& b = c.pickups[order[i]];
        require(a.system != b.system || a.truck != b.truck || a.arrival_s < b.arrival_s,
                "simultaneous pending pickups on one truck");
    }
}

void check_committed_plan(const bweog_plan& p, const bweog_commitments& c, bweog_result& r) {
    if (p.trip_count > max_trips - c.pickup_count) { violation(r, BWEOG_LIMIT); return; }
    std::vector<double> free(64 * 256, 0);
    for (uint32_t i = 0; i < c.occupied_count; ++i) free[c.occupied[i].system * 256 + c.occupied[i].truck] = c.occupied[i].available_s;
    for (uint32_t j = 0; j < p.job_count; ++j) {
        const auto& a = p.jobs[j];
        if (a.called_s < free[a.system * 256 + a.truck]) violation(r, BWEOG_FLEET, a.facility, j, a.called_s);
    }
    for (uint32_t a = 0; a < p.stop_count; ++a) for (uint32_t i = 0; i < c.stopped_count; ++i)
        if (p.stops[a].well == c.stopped_wells[i]) violation(r, BWEOG_STOP, p.stops[a].facility, a);
}

struct Arrival { double time; uint32_t job; };
void evaluate_facility(const bweog_snapshot& s, const bweog_plan& p, uint32_t fi, bweog_result& r,
                       const bweog_commitments* committed = nullptr) {
    const auto& f = s.facilities[fi];
    bweog_facility_result out{};
    out.facility = fi; out.first_point = static_cast<uint32_t>(r.points.size());
    std::vector<double> stops(f.well_count, infinity), times{0, f.repair_s};
    std::vector<Arrival> arrivals;
    if (committed) {
        for (uint32_t i = 0; i < committed->stopped_count; ++i) {
            const auto w = committed->stopped_wells[i];
            if (w >= f.first_well && w < f.first_well + f.well_count) stops[w - f.first_well] = 0;
        }
        for (uint32_t i = 0; i < committed->pickup_count; ++i) if (committed->pickups[i].facility == fi) {
            const auto t = committed->pickups[i].arrival_s;
            arrivals.push_back({t, BWEOG_NO_INDEX}); times.push_back(t);
        }
    }
    for (uint32_t i = 0; i < p.stop_count; ++i) if (p.stops[i].facility == fi) {
        stops[p.stops[i].well - f.first_well] = p.stops[i].start_s;
        times.push_back(p.stops[i].start_s);
    }
    for (uint32_t j = 0; j < p.job_count; ++j) if (p.jobs[j].facility == fi)
        for (uint32_t k = 0; k < p.jobs[j].trip_count; ++k) {
            const auto& t = p.trips[p.jobs[j].first_trip + k];
            arrivals.push_back({t.arrival_s, j}); times.push_back(t.arrival_s);
        }
    std::sort(times.begin(), times.end()); times.erase(std::unique(times.begin(), times.end()), times.end());
    std::sort(arrivals.begin(), arrivals.end(), [](const auto& a, const auto& b) {
        return std::tie(a.time, a.job) < std::tie(b.time, b.job);
    });
    auto point = [&](double t, double v, uint32_t kind) {
        if (r.points.size() >= max_points || r.points.size() - out.first_point >= max_facility_points) {
            violation(r, BWEOG_LIMIT, fi); return false;
        }
        if (!std::isfinite(t) || !std::isfinite(v)) { violation(r, BWEOG_NUMERICAL, fi); return false; }
        r.points.push_back({t, v, kind}); return true;
    };
    auto rate_at = [&](double t) {
        double q = 0;
        if (!out.shut_in) for (uint32_t k = 0; k < f.well_count; ++k)
            if (s.wells[f.first_well + k].productive && stops[k] > t) {
                const double water = s.wells[f.first_well + k].water_bpd;
                const double rate = water / 86400;
                if (water > 0 && rate == 0) return infinity;
                q += rate;
            }
        return q;
    };
    double anchor_t = 0, anchor_v = f.initial_bbl, q = 0, previous_t = 0, previous_v = f.initial_bbl;
    std::size_t ai = 0;
    if (f.already_shut_in || f.initial_bbl >= f.shutdown_bbl) {
        out.shut_in = 1; out.shutdown_s = 0; r.automatic = true;
    }
    // Initial crossing precedes an action at zero. Initial evidence is never reset to threshold.
    if (!point(0, f.initial_bbl, out.shut_in ? 3u : 0u)) return;
    // Stops at zero apply after the initial threshold decision and before positive-time flow.
    q = rate_at(0);
    for (double t : times) {
        if (!std::isfinite(q)) { violation(r, BWEOG_NUMERICAL, fi); return; }
        if (!out.shut_in && q > 0 && t > previous_t) {
            const double crossing = anchor_t + (f.shutdown_bbl - anchor_v) / q;
            if (!std::isfinite(crossing) && !std::isinf(crossing)) {
                violation(r, BWEOG_NUMERICAL, fi); return;
            }
            if (crossing <= t) {
                if (crossing <= previous_t) { violation(r, BWEOG_NUMERICAL, fi); return; }
                out.produced_bbl += q * (crossing - previous_t);
                out.shut_in = 1; out.shutdown_s = crossing; r.automatic = true;
                anchor_t = crossing; anchor_v = f.shutdown_bbl; q = 0;
                if (!point(crossing, anchor_v, 3)) return;
                previous_t = crossing; previous_v = anchor_v;
            }
        }
        double v = anchor_v + q * (t - anchor_t);
        if (!std::isfinite(v) || (!out.shut_in && v >= f.shutdown_bbl)) {
            // Rounded volume and rounded crossing disagree: don't invent feasibility.
            violation(r, BWEOG_NUMERICAL, fi, BWEOG_NO_INDEX, t); return;
        }
        out.produced_bbl += q * (t - previous_t);
        if (t != 0 && (r.points.back().elapsed_s != t || r.points.back().volume_bbl != v))
            if (!point(t, v, 0)) return;
        previous_t = t; previous_v = v;
        const std::size_t begin = ai;
        while (ai < arrivals.size() && arrivals[ai].time == t) ++ai;
        if (ai != begin) {
            const double demand = static_cast<double>(ai - begin) * s.truck_capacity_bbl;
            if (!std::isfinite(demand) || v < demand) {
                violation(r, BWEOG_INVENTORY, fi, arrivals[begin].job, t); return;
            }
            if (r.points.back().elapsed_s == t && r.points.back().volume_bbl == v) {
                if (r.points.back().kind == 0) r.points.back().kind = 1;
            } else if (!point(t, v, 1)) return;
            v -= demand; out.trucked_bbl += demand;
            if (!point(t, v, 2)) return;
            anchor_t = t; anchor_v = v;
        }
        const double next_q = rate_at(t);
        // Preserve an affine anchor across zero-effect breakpoints to avoid
        // changing a crossing merely because a harmless action was inserted.
        if (next_q != q) { anchor_t = t; anchor_v = v; q = next_q; }
        previous_v = v;
    }
    out.final_bbl = previous_v;
    out.balance_residual_bbl = (f.initial_bbl + out.produced_bbl - out.trucked_bbl) - out.final_bbl;
    for (uint32_t k = 0; k < f.well_count; ++k) {
        const auto& w = s.wells[f.first_well + k];
        if (!w.productive) continue;
        const double lost_from = std::min(stops[k], out.shut_in ? out.shutdown_s : infinity);
        if (lost_from < f.repair_s) {
            const double oil = w.oil_bpd * s.oil_per_bbl, gas = w.gas_mcfd * s.gas_per_mcf;
            const double daily = oil + gas, revenue_s = daily / 86400;
            if ((oil == 0 && w.oil_bpd > 0 && s.oil_per_bbl > 0) ||
                (gas == 0 && w.gas_mcfd > 0 && s.gas_per_mcf > 0) || (daily > 0 && revenue_s == 0)) {
                violation(r, BWEOG_NUMERICAL, fi); return;
            }
            const double duration = f.repair_s - lost_from;
            const double loss = revenue_s * duration;
            if (revenue_s > 0 && duration > 0 && loss == 0) {
                violation(r, BWEOG_NUMERICAL, fi); return;
            }
            out.lost_revenue += loss;
        }
    }
    if (!nonnegative(out.lost_revenue) || !std::isfinite(out.produced_bbl) ||
        !std::isfinite(out.trucked_bbl) || !std::isfinite(out.balance_residual_bbl)) {
        violation(r, BWEOG_NUMERICAL, fi); return;
    }
    out.point_count = static_cast<uint32_t>(r.points.size()) - out.first_point;
    r.lost_revenue += out.lost_revenue; r.facilities.push_back(out);
}
} // namespace

uint32_t bweog_abi_version(void) { return BWEOG_ABI_VERSION; }
const char* bweog_method(void) { return "eog-native-seconds-0.1.1"; }
const char* bweog_last_error(void) { return last_error.c_str(); }
uint32_t bweog_layout_size(uint32_t id) {
    const uint32_t sizes[] = {0, sizeof(bweog_fleet), sizeof(bweog_well), sizeof(bweog_facility),
        sizeof(bweog_snapshot), sizeof(bweog_horizon), sizeof(bweog_stop), sizeof(bweog_trip),
        sizeof(bweog_job), sizeof(bweog_plan), sizeof(bweog_violation), sizeof(bweog_point),
        sizeof(bweog_facility_result), sizeof(bweog_result_view)};
    return id < sizeof(sizes) / sizeof(sizes[0]) ? sizes[id] : 0;
}
int bweog_evaluate(const bweog_snapshot* s, const bweog_plan* p, bweog_result** out) {
    if (!out) { last_error = "null output"; return 1; }
    *out = nullptr;
    try {
        last_error.clear(); require(s && p, "null input"); check_snapshot(*s);
        auto r = std::make_unique<bweog_result>(); check_plan(*s, *p, *r);
        if (r->status == BWEOG_AVAILABLE) {
            for (uint32_t i = 0; i < s->facility_count && r->status == BWEOG_AVAILABLE; ++i)
                evaluate_facility(*s, *p, i, *r);
            r->truck_cost = static_cast<double>(p->trip_count) * s->cost_per_trip;
            r->total = r->truck_cost + r->lost_revenue;
            if (!nonnegative(r->total)) violation(*r, BWEOG_NUMERICAL);
        }
        *out = r.release(); return 0;
    } catch (const std::invalid_argument& e) { last_error = e.what(); return 1; }
    catch (...) { last_error = "allocation or internal evaluation failure"; return 2; }
}
int bweog_get_result(const bweog_result* r, bweog_result_view* out) {
    if (!r || !out) return 1;
    const double absent = std::numeric_limits<double>::quiet_NaN();
    *out = {r->status, r->automatic ? 1u : 0u,
        r->status == BWEOG_AVAILABLE ? r->truck_cost : absent,
        r->status == BWEOG_AVAILABLE ? r->lost_revenue : absent,
        r->status == BWEOG_AVAILABLE ? r->total : absent,
        r->facilities.data(), r->points.data(), r->violations.data(),
        static_cast<uint32_t>(r->facilities.size()), static_cast<uint32_t>(r->points.size()),
        static_cast<uint32_t>(r->violations.size())};
    return 0;
}
void bweog_destroy(bweog_result* r) { delete r; }

const char* bweog_committed_method(void) { return "eog-native-committed-seconds-0.1.0"; }
uint32_t bweog_committed_layout_size(uint32_t id) {
    const uint32_t sizes[] = {0, sizeof(bweog_occupied), sizeof(bweog_pending), sizeof(bweog_commitments)};
    return id < sizeof(sizes) / sizeof(sizes[0]) ? sizes[id] : 0;
}
int bweog_evaluate_committed(const bweog_snapshot* s, const bweog_plan* p,
                            const bweog_commitments* c, bweog_result** out) {
    if (!out) { last_error = "null output"; return 1; }
    *out = nullptr;
    try {
        last_error.clear(); require(s && p && c, "null input"); check_snapshot(*s); check_commitments(*s, *c);
        auto r = std::make_unique<bweog_result>(); check_plan(*s, *p, *r);
        if (r->status == BWEOG_AVAILABLE) check_committed_plan(*p, *c, *r);
        if (r->status == BWEOG_AVAILABLE) {
            for (uint32_t i = 0; i < s->facility_count && r->status == BWEOG_AVAILABLE; ++i)
                evaluate_facility(*s, *p, i, *r, c);
            r->truck_cost = static_cast<double>(p->trip_count + c->pickup_count) * s->cost_per_trip;
            r->total = r->truck_cost + r->lost_revenue;
            if (!nonnegative(r->total)) violation(*r, BWEOG_NUMERICAL);
        }
        *out = r.release(); return 0;
    } catch (const std::invalid_argument& e) { last_error = e.what(); return 1; }
    catch (...) { last_error = "allocation or internal committed evaluation failure"; return 2; }
}

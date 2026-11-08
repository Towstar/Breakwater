#include "breakwater/eog_analytical.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <utility>

namespace breakwater::eog::analytical {
namespace {
double finite(double value) {
    if (!std::isfinite(value)) throw std::invalid_argument("EOG analytical nonfinite value or arithmetic overflow.");
    return value;
}
double nonnegative(double value) {
    if (finite(value) < 0.0) throw std::invalid_argument("EOG analytical value must be nonnegative.");
    return value;
}
double positive(double value) {
    if (finite(value) <= 0.0) throw std::invalid_argument("EOG analytical value must be positive.");
    return value;
}
void identity(const std::string& id) {
    if (id.empty() || id.size() > 256) throw std::invalid_argument("EOG analytical missing/oversized ID.");
}
void inspect(const FailureFixture& fixture) {
    nonnegative(fixture.prices.oil_per_bbl);
    nonnegative(fixture.prices.gas_per_mcf);
    positive(fixture.truck_capacity_bbl);
    nonnegative(fixture.cost_per_trip);
    if (fixture.facilities.empty() || fixture.facilities.size() > max_facilities ||
        fixture.fleets.size() > max_facilities)
        throw std::invalid_argument("EOG analytical facility/fleet bound.");
    std::set<std::string> systems;
    for (const auto& fleet : fixture.fleets) {
        identity(fleet.system_id);
        if (!systems.insert(fleet.system_id).second || fleet.size > max_truck_jobs)
            throw std::invalid_argument("EOG analytical duplicate system or oversized fleet.");
    }
    std::set<std::string> facilities;
    std::set<std::string> wells;
    for (const auto& f : fixture.facilities) {
        identity(f.id); identity(f.system_id);
        if (!facilities.insert(f.id).second || !systems.contains(f.system_id))
            throw std::invalid_argument("EOG analytical duplicate facility or unknown system.");
        positive(f.capacity_bbl);
        if (nonnegative(f.initial_level_pct) > 100.0 || positive(f.shutdown_pct) > 100.0)
            throw std::invalid_argument("EOG analytical unsupported inventory/threshold; no clipping.");
        if (nonnegative(f.remaining_repair_h) > 168.0)
            throw std::invalid_argument("EOG analytical repair horizon exceeds 168 hours.");
        nonnegative(f.response_min); positive(f.cycle_min);
        if (f.wells.empty() || f.wells.size() > max_wells_per_facility)
            throw std::invalid_argument("EOG analytical well bound.");
        double flow = 0.0, revenue = 0.0;
        for (const auto& w : f.wells) {
            identity(w.id);
            if (!wells.insert(w.id).second) throw std::invalid_argument("EOG analytical duplicate well.");
            nonnegative(w.water_bpd); nonnegative(w.oil_bpd); nonnegative(w.gas_mcfd);
            if (w.productive) {
                flow = finite(flow + hourly_water_bbl(w.water_bpd));
                revenue = finite(revenue + hourly_revenue(w.oil_bpd, w.gas_mcfd,
                    fixture.prices.oil_per_bbl, fixture.prices.gas_per_mcf));
            }
        }
        finite(flow * f.remaining_repair_h);
        finite(revenue * f.remaining_repair_h);
    }
}
const FailedFacility* facility(const FailureFixture& f, const std::string& id) {
    const auto found = std::find_if(f.facilities.begin(), f.facilities.end(),
        [&](const auto& item) { return item.id == id; });
    return found == f.facilities.end() ? nullptr : &*found;
}
struct Event {
    double time_h;
    double remove_bbl;
    std::vector<std::string> stop_wells;
};
}

double volume_bbl(double capacity_bbl, double level_pct) {
    return finite(positive(capacity_bbl) * (finite(level_pct) / 100.0));
}
double hourly_water_bbl(double water_bpd) { return finite(finite(water_bpd) / 24.0); }
double hourly_revenue(double oil_bpd, double gas_mcfd, double oil_per_bbl, double gas_per_mcf) {
    return finite(finite(nonnegative(oil_bpd) * nonnegative(oil_per_bbl) +
                         nonnegative(gas_mcfd) * nonnegative(gas_per_mcf)) / 24.0);
}
double repair_remaining_h(double inferred_onset_h, double cutoff_h, double duration_h) {
    finite(inferred_onset_h); finite(cutoff_h); nonnegative(duration_h);
    if (cutoff_h < inferred_onset_h) throw std::invalid_argument("EOG analytical onset follows cutoff.");
    const double elapsed = finite(cutoff_h - inferred_onset_h);
    return elapsed >= duration_h ? 0.0 : duration_h - elapsed;
}
PumpState hysteresis(PumpState previous, double level_pct, double start_pct, double stop_pct, bool fault_active) {
    finite(level_pct);
    if (nonnegative(stop_pct) >= positive(start_pct) || start_pct > 100.0)
        throw std::invalid_argument("EOG analytical pump thresholds must satisfy 0 <= stop < start <= 100.");
    if (fault_active) return PumpState::fault;
    if (previous == PumpState::fault)
        throw std::invalid_argument("EOG analytical post-fault pump state requires an explicit restart assumption.");
    if (previous != PumpState::running && previous != PumpState::stopped)
        throw std::invalid_argument("EOG analytical unknown pump state.");
    if (level_pct >= start_pct) return PumpState::running;
    if (level_pct <= stop_pct) return PumpState::stopped;
    return previous;
}
std::optional<double> countdown_h(double inventory_bbl, double shutdown_bbl, double net_inflow_bblh) {
    nonnegative(inventory_bbl); positive(shutdown_bbl); finite(net_inflow_bblh);
    if (inventory_bbl >= shutdown_bbl) return 0.0;
    if (net_inflow_bblh <= 0.0) return std::nullopt;
    return finite((shutdown_bbl - inventory_bbl) / net_inflow_bblh);
}
CountdownEstimate observed_countdown_fixture(double capacity_bbl, double shutdown_pct,
                                             const ObservationFixture& o) {
    positive(capacity_bbl);
    if (positive(shutdown_pct) > 100.0) throw std::invalid_argument("EOG analytical unsupported shutdown percentage.");
    finite(o.cutoff_h); nonnegative(o.maximum_age_h); nonnegative(o.maximum_skew_h);
    auto unavailable = [](const char* reason) {
        return CountdownEstimate{CountdownStatus::unavailable, {}, reason};
    };
    if (!o.level_pct || !o.inflow_bpd || !o.pump_flow_bpd) return unavailable("missing_observation");
    double earliest = o.cutoff_h, latest = -std::numeric_limits<double>::infinity();
    for (const auto& sample : {*o.level_pct, *o.inflow_bpd, *o.pump_flow_bpd}) {
        if (!std::isfinite(sample.value) || !std::isfinite(sample.sampled_at_h))
            return unavailable("nonfinite_observation");
        if (sample.sampled_at_h > o.cutoff_h) return unavailable("future_observation");
        if (o.cutoff_h - sample.sampled_at_h > o.maximum_age_h) return unavailable("stale_observation");
        earliest = std::min(earliest, sample.sampled_at_h);
        latest = std::max(latest, sample.sampled_at_h);
    }
    if (latest - earliest > o.maximum_skew_h) return unavailable("incoherent_observations");
    if (o.level_pct->value < 0.0 || o.level_pct->value > 100.0 ||
        o.inflow_bpd->value < 0.0 || o.pump_flow_bpd->value < 0.0)
        return unavailable("unsupported_observation");
    const auto hours = countdown_h(volume_bbl(capacity_bbl, o.level_pct->value),
        volume_bbl(capacity_bbl, shutdown_pct), hourly_water_bbl(o.inflow_bpd->value - o.pump_flow_bpd->value));
    if (!hours) return {CountdownStatus::no_crossing, {}, "constant_nonpositive_net_inflow"};
    return {*hours == 0.0 ? CountdownStatus::already_reached : CountdownStatus::available,
        hours, "constant_observed_net_inflow_assumed_until_repair"};
}
bool Assessment::avoids_shutdown() const {
    return valid() && std::all_of(facilities.begin(), facilities.end(),
        [](const auto& f) { return f.complete && !f.shutdown_after_h; });
}

Assessment assess_failure_fixture(const FailureFixture& fixture, const Response& response) {
    inspect(fixture);
    if (response.truck_jobs.size() > max_truck_jobs ||
        response.shut_ins.size() > max_facilities * max_wells_per_facility)
        throw std::invalid_argument("EOG analytical response bound.");
    Assessment result;
    auto issue = [&](const char* code, const std::string& id, std::optional<double> t = {}) {
        result.violations.push_back({code, id, t});
    };
    std::set<std::pair<std::string, std::string>> selected_wells;
    for (const auto& action : response.shut_ins) {
        const auto* f = facility(fixture, action.facility_id);
        if (!f) { issue("unknown_facility", action.facility_id); continue; }
        const auto w = std::find_if(f->wells.begin(), f->wells.end(),
            [&](const auto& candidate) { return candidate.id == action.well_id; });
        if (w == f->wells.end() || !w->productive) issue("unknown_or_unproductive_well", f->id);
        if (!selected_wells.insert({f->id, action.well_id}).second) issue("duplicate_shut_in", f->id);
        if (!std::isfinite(action.start_after_h) || action.start_after_h < 0.0 ||
            action.start_after_h >= f->remaining_repair_h)
            issue("shut_in_outside_repair", f->id);
    }
    std::size_t total_trips = 0;
    for (const auto& job : response.truck_jobs) {
        const auto* f = facility(fixture, job.facility_id);
        if (!f) { issue("unknown_facility", job.facility_id); continue; }
        const auto fleet = std::find_if(fixture.fleets.begin(), fixture.fleets.end(),
            [&](const auto& item) { return item.system_id == f->system_id; });
        if (job.truck_index >= fleet->size) { issue("truck_outside_fleet", f->id); continue; }
        if (!std::isfinite(job.call_after_h) || job.call_after_h < 0.0 || job.trips == 0 ||
            job.trips > max_total_trips - total_trips) {
            issue("invalid_or_oversized_job", f->id); continue;
        }
        total_trips += job.trips;
        TruckCalendar calendar{f->id, f->system_id, job.truck_index, job.call_after_h, {}, 0.0};
        const double first = finite(job.call_after_h + f->response_min / 60.0);
        const double cycle = positive(f->cycle_min / 60.0);
        for (std::size_t k = 0; k < job.trips; ++k) {
            const double arrival = finite(first + static_cast<double>(k) * cycle);
            if ((!calendar.arrivals_h.empty() && arrival <= calendar.arrivals_h.back()) ||
                arrival < job.call_after_h)
                throw std::invalid_argument("EOG analytical unrepresentable trip grid.");
            calendar.arrivals_h.push_back(arrival);
            if (arrival >= f->remaining_repair_h) issue("arrival_not_before_repair", f->id, arrival);
        }
        calendar.free_after_h = finite(calendar.arrivals_h.back() + cycle);
        if (calendar.free_after_h <= calendar.arrivals_h.back())
            throw std::invalid_argument("EOG analytical unrepresentable return time.");
        result.calendars.push_back(std::move(calendar));
    }
    // Truck identity is (system, index); occupancy includes first response and
    // the return after the last load. Touching half-open intervals are allowed.
    std::map<std::pair<std::string, std::size_t>, std::vector<const TruckCalendar*>> calendars;
    for (const auto& c : result.calendars) calendars[{c.system_id, c.truck_index}].push_back(&c);
    for (auto& [key, jobs] : calendars) {
        (void)key;
        std::sort(jobs.begin(), jobs.end(), [](const auto* a, const auto* b) {
            return a->call_after_h < b->call_after_h;
        });
        for (std::size_t k = 1; k < jobs.size(); ++k)
            if (jobs[k]->call_after_h < jobs[k - 1]->free_after_h)
                issue("truck_resource_overlap", jobs[k]->facility_id, jobs[k]->call_after_h);
    }
    if (!result.violations.empty()) return result;

    double lost_revenue = 0.0;
    for (const auto& f : fixture.facilities) {
        FacilityOutcome outcome;
        outcome.facility_id = f.id;
        double inventory = volume_bbl(f.capacity_bbl, f.initial_level_pct);
        const double threshold = volume_bbl(f.capacity_bbl, f.shutdown_pct);
        std::map<std::string, double> stops;
        for (const auto& w : f.wells) stops[w.id] = f.remaining_repair_h;
        std::map<double, Event> events;
        events.emplace(f.remaining_repair_h, Event{f.remaining_repair_h, 0.0, {}});
        for (const auto& action : response.shut_ins) if (action.facility_id == f.id) {
            auto [it, added] = events.try_emplace(action.start_after_h, Event{action.start_after_h, 0.0, {}});
            (void)added;
            it->second.stop_wells.push_back(action.well_id);
            stops[action.well_id] = action.start_after_h;
        }
        for (const auto& c : result.calendars) if (c.facility_id == f.id)
            for (const double arrival : c.arrivals_h) {
                auto [it, added] = events.try_emplace(arrival, Event{arrival, 0.0, {}});
                (void)added;
                it->second.remove_bbl = finite(it->second.remove_bbl + fixture.truck_capacity_bbl);
            }
        bool tripped = f.already_shut_in || inventory >= threshold;
        if (tripped) outcome.shutdown_after_h = 0.0;
        outcome.trajectory.push_back({0.0, inventory, tripped ? "already_shut_in" : "initial"});
        double previous = 0.0;
        for (const auto& [time, event] : events) {
            double rate = 0.0;
            if (!tripped) for (const auto& w : f.wells)
                if (w.productive && stops.at(w.id) >= time && stops.at(w.id) > previous)
                    rate = finite(rate + hourly_water_bbl(w.water_bpd));
            const double dt = time - previous;
            const auto crossing = tripped ? std::optional<double>{} : countdown_h(inventory, threshold, rate);
            if (crossing && *crossing <= dt) {
                // This is an explicit analytical state transition, not volume clipping.
                outcome.shutdown_after_h = finite(previous + *crossing);
                inventory = threshold;
                tripped = true;
                outcome.trajectory.push_back({*outcome.shutdown_after_h, inventory, "automatic_shutdown"});
            } else {
                inventory = finite(inventory + rate * dt);
            }
            outcome.trajectory.push_back({time, inventory, "before_actions"});
            if (event.remove_bbl > 0.0) {
                if (tripped) { issue("arrival_at_or_after_shutdown", f.id, time); break; }
                if (event.remove_bbl > inventory) { issue("insufficient_full_load_inventory", f.id, time); break; }
                inventory -= event.remove_bbl;
            }
            outcome.trajectory.push_back({time, inventory,
                time == f.remaining_repair_h ? "repair" : "after_actions"});
            previous = time;
            if (time == f.remaining_repair_h) outcome.complete = true;
        }
        if (outcome.complete) {
            for (const auto& w : f.wells) if (w.productive) {
                const double stop = std::min(stops.at(w.id), outcome.shutdown_after_h.value_or(f.remaining_repair_h));
                outcome.lost_revenue = finite(outcome.lost_revenue +
                    (f.remaining_repair_h - stop) * hourly_revenue(w.oil_bpd, w.gas_mcfd,
                        fixture.prices.oil_per_bbl, fixture.prices.gas_per_mcf));
            }
            lost_revenue = finite(lost_revenue + outcome.lost_revenue);
        }
        result.facilities.push_back(std::move(outcome));
    }
    if (!result.violations.empty()) return result;
    result.truck_cost = finite(static_cast<double>(total_trips) * fixture.cost_per_trip);
    result.lost_revenue = lost_revenue;
    result.total_cost = finite(*result.truck_cost + lost_revenue);
    return result;
}

} // namespace breakwater::eog::analytical

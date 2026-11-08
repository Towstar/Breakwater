#include "breakwater/eog_simulation.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace breakwater::eog::synthetic {
namespace {
constexpr double infinity = std::numeric_limits<double>::infinity();
bool finite_nonnegative(double x) { return std::isfinite(x) && x >= 0; }
void require(bool test, const char* message) { if (!test) throw std::invalid_argument(message); }
double checked(double x) {
    if (!std::isfinite(x)) throw std::runtime_error("Nonfinite synthetic arithmetic.");
    return x;
}
double product(double a, double b) {
    const double value = checked(a * b);
    if (a != 0 && b != 0 && value == 0) throw std::runtime_error("Unrepresentable nonzero synthetic product.");
    return value;
}
double rate(double bpd) {
    const double result = bpd / 86400.0;
    if (bpd != 0 && result == 0) throw std::runtime_error("Unrepresentable synthetic rate.");
    return checked(result);
}
}

struct Simulator::Impl {
    struct State {
        double anchor_s{}, anchor_bbl{}, produced{}, pumped{}, lost{}, trucked{};
        double q{}, p{}, loss_rate{};
        bool running{}, failed{}, automatic{};
        double repair_at{};
    };
    struct WellState {
        double water{}, oil{}, gas{}, revenue{}, loss{}, loss_anchor_s{};
        bool voluntary{}, losing{};
    };
    struct Scheduled { double at{}; std::size_t index{}; bool repair{}; };
    struct Pickup { double at{}; std::size_t facility{}, job{}; };
    struct Job { std::size_t facility{}, truck{}; double returned{}; bool return_recorded{}; };
    Scenario original;
    std::vector<State> states;
    std::vector<WellState> wells;
    std::vector<std::size_t> owner;
    std::vector<Scheduled> incidents;
    std::vector<RateChange> forcing;
    std::vector<std::vector<double>> free_at;
    std::vector<Job> jobs;
    std::vector<Pickup> pickups;
    std::vector<Point> points;
    std::size_t incident_next{}, forcing_next{}, event_count{}, dispatched_trips{}, full_loads{};
    double now{}, truck_cost{};
    Status status{Status::running};
    std::string diagnostic;

    explicit Impl(const Scenario& input) : original(input) {
        require(std::isfinite(input.duration_s) && input.duration_s > 0 && input.duration_s <= 31 * 86400.0,
                "Synthetic horizon must be in (0,31 days].");
        require(input.facilities.size() > 0 && input.facilities.size() <= 256 && input.wells.size() <= 16384 &&
                input.fleet_sizes.size() > 0 && input.fleet_sizes.size() <= 64, "Synthetic asset bound exceeded.");
        require(input.point_limit > 0 && input.point_limit <= 262144 && input.event_limit > 0 &&
                input.event_limit <= 1000000 && input.trip_limit > 0 && input.trip_limit <= 16384,
                "Synthetic execution bound is unsupported.");
        require(input.failures.size() <= 16384 && input.rate_changes.size() <= 16384, "Synthetic forcing bound exceeded.");
        require(finite_nonnegative(input.oil_per_bbl) && finite_nonnegative(input.gas_per_mcf) &&
                finite_nonnegative(input.cost_per_trip) && std::isfinite(input.truck_capacity_bbl) &&
                input.truck_capacity_bbl > 0, "Invalid synthetic economics/full-load capacity.");
        for (auto count : input.fleet_sizes) {
            require(count <= 128, "Synthetic fleet bound exceeded."); free_at.emplace_back(count, 0.0);
        }
        owner.assign(input.wells.size(), input.facilities.size());
        for (std::size_t fi = 0; fi < input.facilities.size(); ++fi) {
            const auto& f = input.facilities[fi];
            require(f.system < free_at.size() && f.first_well <= input.wells.size() &&
                    f.well_count <= input.wells.size() - f.first_well && f.well_count <= 128,
                    "Invalid synthetic facility ownership.");
            require(std::isfinite(f.capacity_bbl) && f.capacity_bbl > 0 && finite_nonnegative(f.initial_bbl) &&
                    f.initial_bbl <= f.capacity_bbl && finite_nonnegative(f.stop_bbl) && f.stop_bbl < f.start_bbl &&
                    f.start_bbl < f.shutdown_bbl && f.shutdown_bbl <= f.capacity_bbl,
                    "Invalid synthetic inventory or hysteresis thresholds.");
            require(std::isfinite(f.pump_bpd) && f.pump_bpd > 0 && std::isfinite(f.pipeline_capacity_bpd) &&
                    f.pipeline_capacity_bpd >= f.pump_bpd && finite_nonnegative(f.response_min) &&
                    std::isfinite(f.cycle_min) && f.cycle_min > 0,
                    "Unsupported synthetic pump/pipeline/travel parameters.");
            require(f.initially_running ? f.initial_bbl >= f.stop_bbl : f.initial_bbl <= f.start_bbl,
                    "Initial pump state contradicts hysteresis.");
            for (std::size_t wi = f.first_well; wi < f.first_well + f.well_count; ++wi) {
                require(owner[wi] == input.facilities.size(), "A synthetic well has multiple owners."); owner[wi] = fi;
            }
            State state; state.anchor_bbl = f.initial_bbl; state.running = f.initially_running;
            states.push_back(state);
        }
        for (std::size_t wi = 0; wi < input.wells.size(); ++wi) {
            const auto& w = input.wells[wi];
            require(owner[wi] < states.size(), "A synthetic well has no facility.");
            require(finite_nonnegative(w.water_bpd) && finite_nonnegative(w.oil_bpd) && finite_nonnegative(w.gas_mcfd),
                    "Invalid synthetic well rates.");
            wells.push_back(make_well(w.water_bpd, w.oil_bpd, w.gas_mcfd));
        }
        for (std::size_t index = 0; index < input.failures.size(); ++index) {
            const auto& f = input.failures[index];
            require(f.facility < states.size() && finite_nonnegative(f.start_s) && f.start_s < input.duration_s &&
                    std::isfinite(f.repair_s) && f.repair_s > 0 && std::isfinite(f.start_s + f.repair_s) &&
                    f.start_s + f.repair_s > f.start_s, "Invalid synthetic failure schedule.");
            incidents.push_back({f.start_s, index, false});
            incidents.push_back({f.start_s + f.repair_s, index, true});
            for (std::size_t other = 0; other < index; ++other) {
                const auto& g = input.failures[other];
                require(g.facility != f.facility || f.start_s >= g.start_s + g.repair_s ||
                        g.start_s >= f.start_s + f.repair_s, "Overlapping failures on one synthetic pump.");
            }
        }
        std::sort(incidents.begin(), incidents.end(), [](auto a, auto b) {
            // A repair clears its old latch before a new failure at the same time.
            return std::tuple(a.at, !a.repair, a.index) < std::tuple(b.at, !b.repair, b.index);
        });
        forcing = input.rate_changes;
        std::sort(forcing.begin(), forcing.end(), [](const auto& a, const auto& b) { return std::tie(a.elapsed_s, a.well) < std::tie(b.elapsed_s, b.well); });
        for (std::size_t index = 0; index < forcing.size(); ++index) {
            const auto& r = forcing[index];
            require(r.well < wells.size() && finite_nonnegative(r.elapsed_s) && r.elapsed_s <= input.duration_s &&
                    finite_nonnegative(r.water_bpd) && finite_nonnegative(r.oil_bpd) && finite_nonnegative(r.gas_mcfd),
                    "Invalid synthetic rate forcing.");
            require(index == 0 || forcing[index - 1].elapsed_s != r.elapsed_s || forcing[index - 1].well != r.well,
                    "Duplicate synthetic rate forcing.");
        }
        for (std::size_t fi = 0; fi < states.size(); ++fi) { rephase(fi); point(fi, Kind::initial); }
        process();
    }

    WellState make_well(double water, double oil, double gas) const {
        WellState w; w.water = rate(water); w.oil = rate(oil); w.gas = rate(gas);
        w.revenue = checked(product(w.oil, original.oil_per_bbl) + product(w.gas, original.gas_per_mcf));
        return w;
    }
    bool producing(std::size_t wi) const {
        return original.wells[wi].initially_productive && !wells[wi].voluntary && !states[owner[wi]].automatic;
    }
    double volume(std::size_t fi, double at) const {
        const auto& s = states[fi]; return checked(s.anchor_bbl + product(s.q - s.p, at - s.anchor_s));
    }
    void close(std::size_t fi) {
        auto& s = states[fi];
        const double dt = now - s.anchor_s;
        s.anchor_bbl = volume(fi, now);
        s.produced = checked(s.produced + product(s.q, dt));
        s.pumped = checked(s.pumped + product(s.p, dt));
        s.lost = checked(s.lost + product(s.loss_rate, dt));
        s.anchor_s = now;
        const auto& f = original.facilities[fi];
        for (std::size_t wi = f.first_well; wi < f.first_well + f.well_count; ++wi) {
            auto& w = wells[wi];
            if (w.losing) w.loss = checked(w.loss + product(w.revenue, now - w.loss_anchor_s));
            w.loss_anchor_s = now;
        }
    }
    void rephase(std::size_t fi) {
        auto& s = states[fi]; const auto& f = original.facilities[fi];
        double q = 0, loss = 0;
        for (std::size_t wi = f.first_well; wi < f.first_well + f.well_count; ++wi) {
            auto& w = wells[wi]; w.losing = original.wells[wi].initially_productive && !producing(wi);
            if (producing(wi)) q = checked(q + w.water);
            if (w.losing) loss = checked(loss + w.revenue);
        }
        s.q = q; s.p = s.failed || !s.running ? 0 : rate(f.pump_bpd); s.loss_rate = loss;
    }
    void fail(Status new_status, const char* message) { status = new_status; diagnostic = message; }
    void point(std::size_t fi, Kind kind) {
        if (points.size() >= original.point_limit) { fail(Status::limit, "Synthetic trace bound reached; no trace is silently decimated."); return; }
        const auto& s = states[fi];
        points.push_back({now, fi, kind, volume(fi, now), s.failed ? 2 : s.running ? 1 : 0, s.automatic});
    }
    void transition(std::size_t fi, Kind kind) { rephase(fi); point(fi, kind); }

    double root(std::size_t fi, double target, bool rising) const {
        const auto& s = states[fi]; const double slope = s.q - s.p;
        if (rising ? slope <= 0 : slope >= 0) return infinity;
        const double value = volume(fi, now);
        if (rising ? value >= target : value <= target) return now;
        double at = s.anchor_s + (target - s.anchor_bbl) / slope;
        if (!std::isfinite(at)) return infinity;
        if (at <= now) at = std::nextafter(now, infinity);
        for (int step = 0; step < 8; ++step) {
            const double level = volume(fi, at);
            if (rising ? level >= target : level <= target) return at;
            at = std::nextafter(at, infinity);
        }
        throw std::runtime_error("Synthetic threshold crossing cannot be represented unambiguously.");
    }
    double next_crossing(std::size_t fi) const {
        const auto& s = states[fi]; const auto& f = original.facilities[fi];
        if (s.failed) return s.automatic ? infinity : root(fi, f.shutdown_bbl, true);
        if (!s.running) return root(fi, f.start_bbl, true);
        return s.q < s.p ? root(fi, f.stop_bbl, false) : root(fi, f.capacity_bbl, true);
    }

    void process() {
        // Continuous crossings precede repair/forcing/pickup at an exact tie.
        for (std::size_t fi = 0; fi < states.size() && status == Status::running; ++fi) {
            auto& s = states[fi]; const auto& f = original.facilities[fi]; const double v = volume(fi, now);
            if (v < 0 || v > f.capacity_bbl) {
                point(fi, Kind::sample); fail(Status::unavailable, "Synthetic inventory leaves physical bounds; raw value retained without clipping."); break;
            }
            if (s.failed && !s.automatic && v >= f.shutdown_bbl) {
                close(fi); s.automatic = true; transition(fi, Kind::automatic_shut_in);
            } else if (!s.failed && !s.running && v >= f.start_bbl) {
                close(fi); s.running = true; transition(fi, Kind::pump_start);
            } else if (!s.failed && s.running && s.q < s.p && v <= f.stop_bbl) {
                close(fi); s.running = false; transition(fi, Kind::pump_stop);
            } else if (!s.failed && s.running && s.q > s.p && v >= f.capacity_bbl) {
                point(fi, Kind::sample); fail(Status::unavailable, "Healthy inflow exceeds available pumping at physical capacity; no clipping.");
            }
        }
        while (incident_next < incidents.size() && incidents[incident_next].at == now && status == Status::running) {
            const auto event = incidents[incident_next++]; const auto& incident = original.failures[event.index];
            const auto fi = incident.facility; auto& s = states[fi]; const auto& f = original.facilities[fi];
            close(fi);
            if (event.repair) {
                s.failed = false; s.automatic = false;
                for (std::size_t wi = f.first_well; wi < f.first_well + f.well_count; ++wi) wells[wi].voluntary = false;
                // Failed pumps resume the normal controller. Threshold-first
                // automatic shutdown applies to a failed pump; a repaired pump
                // drains a high tank while the wells immediately resume (SPEC).
                s.running = s.anchor_bbl >= f.start_bbl;
                transition(fi, Kind::repair);
            } else {
                s.failed = true; s.running = false; s.repair_at = event.at + incident.repair_s;
                transition(fi, Kind::failure);
                if (s.anchor_bbl >= f.shutdown_bbl) { s.automatic = true; transition(fi, Kind::automatic_shut_in); }
            }
        }
        while (forcing_next < forcing.size() && forcing[forcing_next].elapsed_s == now && status == Status::running) {
            const auto r = forcing[forcing_next++]; const auto fi = owner[r.well];
            auto replacement = make_well(r.water_bpd, r.oil_bpd, r.gas_mcfd);
            if (replacement.water == wells[r.well].water && replacement.oil == wells[r.well].oil && replacement.gas == wells[r.well].gas) {
                point(fi, Kind::rate_change); continue; // no numerical phase reset for zero-effect forcing
            }
            close(fi);
            const double prior_loss = wells[r.well].loss;
            const bool voluntary = wells[r.well].voluntary;
            wells[r.well] = replacement;
            wells[r.well].loss = prior_loss; wells[r.well].loss_anchor_s = now; wells[r.well].voluntary = voluntary;
            transition(fi, Kind::rate_change);
        }
        // Check all simultaneous full loads together, before any stock/cost edit.
        std::vector<std::size_t> due(states.size(), 0);
        for (const auto& p : pickups) if (p.at == now) ++due[p.facility];
        for (std::size_t fi = 0; fi < states.size() && status == Status::running; ++fi) if (due[fi]) {
            point(fi, Kind::before_pickup);
            const double removed = product(static_cast<double>(due[fi]), original.truck_capacity_bbl);
            if (!states[fi].failed || !(now < states[fi].repair_at) || volume(fi, now) < removed) {
                fail(Status::invalid_action, "A pickup is not a full available load strictly before repair."); break;
            }
            close(fi); auto& s = states[fi]; s.anchor_bbl = checked(s.anchor_bbl - removed);
            s.trucked = checked(s.trucked + removed);
            truck_cost = checked(truck_cost + product(static_cast<double>(due[fi]), original.cost_per_trip));
            full_loads += due[fi]; transition(fi, Kind::after_pickup);
        }
        if (status != Status::running) return;
        std::erase_if(pickups, [this](const auto& p) { return p.at == now; });
        for (auto& job : jobs) if (!job.return_recorded && job.returned == now) {
            job.return_recorded = true; point(job.facility, Kind::truck_return);
        }
    }

    bool advance(double until, const std::function<bool()>& cancelled) {
        require(std::isfinite(until) && until >= now && until <= original.duration_s, "Invalid synthetic advance clock.");
        if (status != Status::running) return status == Status::completed && until == now;
        try {
            while (now < until && status == Status::running) {
                if (cancelled && cancelled()) { fail(Status::cancelled, "Synthetic run cancelled before completion."); break; }
                if (++event_count > original.event_limit) { fail(Status::limit, "Synthetic event-work bound reached."); break; }
                double next = until;
                if (incident_next < incidents.size()) next = std::min(next, incidents[incident_next].at);
                if (forcing_next < forcing.size()) next = std::min(next, forcing[forcing_next].elapsed_s);
                for (const auto& p : pickups) next = std::min(next, p.at);
                for (const auto& j : jobs) if (!j.return_recorded) next = std::min(next, j.returned);
                for (std::size_t fi = 0; fi < states.size(); ++fi) next = std::min(next, next_crossing(fi));
                if (!(next > now)) throw std::runtime_error("Synthetic event clock did not advance.");
                now = next; process();
            }
            if (status == Status::running) {
                for (std::size_t fi = 0; fi < states.size() && status == Status::running; ++fi) point(fi, Kind::sample);
                if (status == Status::running && now == original.duration_s) status = Status::completed;
            }
            if (status == Status::running || status == Status::completed) {
                const auto checked_snapshot = snapshot();
                if (checked_snapshot.status == Status::unavailable) fail(Status::unavailable, checked_snapshot.diagnostic.c_str());
            }
        } catch (const std::runtime_error& error) { fail(Status::unavailable, error.what()); }
        return status == Status::running || status == Status::completed;
    }

    ActionResult dispatch(std::size_t fi, std::size_t truck, std::size_t trips) {
        if (status != Status::running) return {false, "Synthetic run is not accepting actions."};
        if (fi >= states.size()) return {false, "Unknown synthetic facility."};
        const auto& f = original.facilities[fi];
        if (truck >= free_at[f.system].size() || !states[fi].failed || free_at[f.system][truck] > now)
            return {false, "Truck is unknown/busy or the facility has no active failure."};
        if (trips == 0 || trips > 4095 || trips > original.trip_limit - dispatched_trips)
            return {false, "Synthetic trip bound exceeded."};
        const double first = now + f.response_min * 60.0, cycle = f.cycle_min * 60.0;
        const double last = first + static_cast<double>(trips - 1) * cycle, returned = last + cycle;
        if (!std::isfinite(returned) || !(returned > last) || first < now ||
            (f.response_min > 0 && !(first > now)) || !(last < states[fi].repair_at))
            return {false, "Dispatch timing is unrepresentable or a pickup reaches/exceeds repair."};
        std::vector<Pickup> additions;
        double prior = -infinity;
        for (std::size_t k = 0; k < trips; ++k) {
            const double at = first + static_cast<double>(k) * cycle;
            if (!(at > prior) || !(at < states[fi].repair_at)) return {false, "Dispatch pickups do not advance strictly before repair."};
            additions.push_back({at, fi, jobs.size()}); prior = at;
        }
        free_at[f.system][truck] = returned;
        jobs.push_back({fi, truck, returned, false});
        pickups.insert(pickups.end(), additions.begin(), additions.end()); dispatched_trips += trips;
        try {
            point(fi, Kind::dispatch);
            if (first == now && status == Status::running) process();
        } catch (const std::runtime_error& error) { fail(Status::unavailable, error.what()); return {false, diagnostic}; }
        return {true, {}};
    }

    ActionResult shut_in(std::size_t wi) {
        if (status != Status::running || wi >= wells.size()) return {false, "Unknown well or synthetic run is not accepting actions."};
        const auto fi = owner[wi];
        if (!states[fi].failed || !original.wells[wi].initially_productive || wells[wi].voluntary)
            return {false, "Well shut-in needs an active failure and an eligible, uncommanded well."};
        try {
            if (!states[fi].automatic) close(fi);
            wells[wi].voluntary = true;
            if (states[fi].automatic) point(fi, Kind::well_stop); else transition(fi, Kind::well_stop);
        }
        catch (const std::runtime_error& error) { fail(Status::unavailable, error.what()); return {false, diagnostic}; }
        return {true, {}};
    }

    Snapshot snapshot() const {
        Snapshot out; out.elapsed_s = now; out.status = status; out.diagnostic = diagnostic;
        out.completed_full_loads = full_loads;
        double lost = 0;
        try {
        for (std::size_t fi = 0; fi < states.size(); ++fi) {
            const auto& s = states[fi]; const double dt = now - s.anchor_s;
            FacilityView f; f.volume_bbl = volume(fi, now); f.inflow_bpd = product(s.q, 86400.0); f.pump_flow_bpd = product(s.p, 86400.0);
            f.pump_status = s.failed ? 2 : s.running ? 1 : 0; f.automatic_shut_in = s.automatic;
            f.produced_bbl = checked(s.produced + product(s.q, dt)); f.pumped_bbl = checked(s.pumped + product(s.p, dt));
            f.trucked_bbl = s.trucked; f.lost_revenue = checked(s.lost + product(s.loss_rate, dt));
            f.balance_residual_bbl = checked(original.facilities[fi].initial_bbl + f.produced_bbl - f.pumped_bbl - f.trucked_bbl - f.volume_bbl);
            lost = checked(lost + f.lost_revenue); out.facilities.push_back(f);
        }
        for (std::size_t wi = 0; wi < wells.size(); ++wi) {
            const auto& w = wells[wi]; const bool active = producing(wi);
            out.wells.push_back({active, active ? product(w.water, 86400) : 0, active ? product(w.oil, 86400) : 0,
                active ? product(w.gas, 86400) : 0, checked(w.loss + (w.losing ? product(w.revenue, now - w.loss_anchor_s) : 0))});
        }
        for (std::size_t si = 0; si < free_at.size(); ++si) for (std::size_t ti = 0; ti < free_at[si].size(); ++ti)
            out.trucks.push_back({si, ti, free_at[si][ti] > now, free_at[si][ti]});
        if (status == Status::running || status == Status::completed) {
            out.truck_cost = truck_cost; out.lost_revenue = lost; out.total_cost = checked(truck_cost + lost);
        }
        } catch (const std::runtime_error& error) {
            out.status = Status::unavailable; out.diagnostic = error.what();
            out.facilities.clear(); out.wells.clear(); out.trucks.clear();
            out.truck_cost.reset(); out.lost_revenue.reset(); out.total_cost.reset();
        }
        return out;
    }
};

Simulator::Simulator(const Scenario& scenario) : impl_(std::make_unique<Impl>(scenario)) {}
Simulator::~Simulator() = default;
Simulator::Simulator(Simulator&&) noexcept = default;
Simulator& Simulator::operator=(Simulator&&) noexcept = default;
bool Simulator::advance(double until, const std::function<bool()>& cancelled) { return impl_->advance(until, cancelled); }
ActionResult Simulator::dispatch(std::size_t facility, std::size_t truck, std::size_t trips) { return impl_->dispatch(facility, truck, trips); }
ActionResult Simulator::shut_in(std::size_t well) { return impl_->shut_in(well); }
Snapshot Simulator::snapshot() const { return impl_->snapshot(); }
const std::vector<Point>& Simulator::trace() const { return impl_->points; }
void Simulator::reset() { impl_ = std::make_unique<Impl>(impl_->original); }
} // namespace breakwater::eog::synthetic

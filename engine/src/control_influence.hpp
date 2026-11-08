#pragma once

// Private, request-local specialization of the route-walk screen. Public controls,
// topology compilation and independent finalist validation remain unchanged.
#include "search_kernel.hpp"
#include <bit>
#include <cstdint>

namespace breakwater::detail {

// The recorded A7 matrix showed no full-search benefit on small workloads.
// This is a performance policy, not a feasibility condition or a speed guarantee.
inline bool use_control_influence(std::size_t route_count, std::size_t candidate_count) {
    return route_count >= 32 && candidate_count >= 2048;
}

struct InfluenceWorkspace;

class ControlInfluence {
public:
    using workspace_type = InfluenceWorkspace;

    ControlInfluence(const Prepared& prepared, const CandidateSet& candidates)
        : p_(prepared), set_(candidates), base_rates_(p_.baseline.tank_ids.size()),
          base_destinations_(p_.destination_capacities.size()),
          maximum_flows_(base_rates_.size()), maximum_destinations_(base_destinations_.size()) {
        // The prepared object owns a validated copy of this request's network. No
        // cached map survives a request or observes later caller/network edits.
        if (set_.bindings.size() > max_search_controls)
            throw std::invalid_argument("Influence map supports at most four controls.");
        Workspace baseline(p_);
        const CandidateSet unchanged;
        double unused = 0.0;
        (void)screen_candidate(p_, unchanged, Values{}, 1.0, baseline, unused);
        base_rates_ = baseline.rates;
        base_destinations_ = baseline.destination_totals;
        base_cost_ = p_.baseline.cost_per_h;

        std::vector<double> route_maximum;
        route_maximum.reserve(p_.routes.size());
        for (const auto& route : p_.routes)
            route_maximum.push_back(route.source_slot == absent ? route.setpoint :
                (p_.sources[route.source_slot].production - p_.sources[route.source_slot].curtailment) * route.source_fraction);
        std::vector<double> curtailment_maximum;
        for (const auto& source : p_.sources) curtailment_maximum.push_back(source.curtailment);

        for (std::size_t j = 0; j < set_.bindings.size(); ++j) {
            const auto& binding = p_.bindings.at(set_.bindings[j]);
            auto& column = columns_[j];
            column.baseline = binding.descriptor.value;
            double coefficient = 1.0;
            std::size_t route_index = binding.slot;
            if (binding.kind == ControlKind::curtailment) {
                const auto& source = p_.sources[binding.slot];
                column.cost = source.cost;
                curtailment_maximum[binding.slot] = std::max(source.curtailment, binding.descriptor.maximum);
                // Production bounds every source-feed flow, including subtraction
                // rounding. It is deliberately broader than the selected interval.
                for (const auto feed_index : source.feed_routes) {
                    const auto& feed = p_.routes[feed_index];
                    const double share_coefficient = -feed.source_fraction;
                    column.rates.push_back({feed.target_tank, share_coefficient}); ++column.rate_count;
                    column.cost += share_coefficient * feed.route_cost;
                    route_maximum[feed_index] = source.production * feed.source_fraction;
                }
                continue;
            } else {
                route_maximum[route_index] = std::max(route_maximum[route_index], binding.descriptor.maximum);
            }
            const auto& route = p_.routes[route_index];
            if (route.source_tank != absent)
                { column.rates.push_back({route.source_tank, -coefficient}); ++column.rate_count; }
            if (route.target_tank != absent)
                { column.rates.push_back({route.target_tank, coefficient}); ++column.rate_count; }
            else column.destination = {route.destination_slot, coefficient};
            column.cost += coefficient * route.route_cost;
            if (route.destination_slot != absent) column.cost += coefficient * route.destination_cost;
        }

        // Nonnegative upper sums include an outward rounding step. These are
        // envelopes, not a second physical configuration or clipped inventories.
        for (std::size_t s = 0; s < p_.sources.size(); ++s)
            maximum_cost_ = upper_add(maximum_cost_, upper_product(curtailment_maximum[s], p_.sources[s].cost));
        for (std::size_t r = 0; r < p_.routes.size(); ++r) {
            const auto& route = p_.routes[r];
            const double upper = route_maximum[r] == 0.0 ? 0.0 : std::nextafter(route_maximum[r], infinity());
            if (route.source_tank != absent)
                maximum_flows_[route.source_tank] = upper_add(maximum_flows_[route.source_tank], upper);
            if (route.target_tank != absent)
                maximum_flows_[route.target_tank] = upper_add(maximum_flows_[route.target_tank], upper);
            else maximum_destinations_[route.destination_slot] = upper_add(maximum_destinations_[route.destination_slot], upper);
            maximum_cost_ = upper_add(maximum_cost_, upper_product(upper, route.route_cost));
            if (route.destination_slot != absent)
                maximum_cost_ = upper_add(maximum_cost_, upper_product(upper, route.destination_cost));
        }
        // At most 256 routes and four controls. This deliberately loose envelope
        // exceeds the usual gamma_n bounds for the original sum, prepared baseline,
        // rounded deltas, sparse column additions, and endpoint arithmetic. The
        // 1e-9 floor also dominates subnormal absolute errors. Near the envelope we
        // execute the old screen; these guards do not certify a recommendation.
        relative_guard_ = 64.0 * std::numeric_limits<double>::epsilon() *
            static_cast<double>(p_.routes.size() + p_.sources.size() + 2 * set_.bindings.size() + 8);
        usable_ = std::isfinite(base_cost_) && std::isfinite(maximum_cost_);
        for (const auto* row : {&base_rates_, &base_destinations_, &maximum_flows_, &maximum_destinations_})
            for (double value : *row) usable_ = usable_ && std::isfinite(value);
        for (std::size_t j = 0; j < set_.bindings.size(); ++j)
            usable_ = usable_ && std::isfinite(columns_[j].cost);
    }

    Screen screen(const Values& values, double horizon, InfluenceWorkspace& workspace, double& cost) const;
    const Prepared& prepared() const { return p_; }

    // Observable numeric preparation result for benchmarks (not a physical total).
    std::uint64_t precomputation_checksum() const {
        std::uint64_t hash = 14695981039346656037ull;
        const auto mix = [&](std::uint64_t value) { hash = (hash ^ value) * 1099511628211ull; };
        mix(std::bit_cast<std::uint64_t>(base_cost_));
        mix(std::bit_cast<std::uint64_t>(maximum_cost_));
        mix(std::bit_cast<std::uint64_t>(relative_guard_));
        mix(usable_ ? 1ull : 0ull);
        for (const auto* row : {&base_rates_, &base_destinations_, &maximum_flows_, &maximum_destinations_})
            for (double value : *row) mix(std::bit_cast<std::uint64_t>(value));
        for (std::size_t j = 0; j < set_.bindings.size(); ++j) {
            const auto& column = columns_[j];
            mix(std::bit_cast<std::uint64_t>(column.baseline));
            mix(std::bit_cast<std::uint64_t>(column.cost));
            mix(column.rate_count);
            for (std::size_t k = 0; k < column.rate_count; ++k) {
                mix(column.rates[k].index);
                mix(std::bit_cast<std::uint64_t>(column.rates[k].coefficient));
            }
            mix(column.destination.index);
            mix(std::bit_cast<std::uint64_t>(column.destination.coefficient));
        }
        return hash;
    }

private:
    struct Change { std::size_t index = absent; double coefficient = 0.0; };
    struct Column {
        double baseline = 0.0, cost = 0.0;
        std::vector<Change> rates;
        std::size_t rate_count = 0;
        Change destination;
    };
    static constexpr double infinity() { return std::numeric_limits<double>::infinity(); }
    static double upper_add(double a, double b) {
        return a == 0.0 && b == 0.0 ? 0.0 : std::nextafter(a + b, infinity());
    }
    static double upper_product(double a, double b) {
        return a == 0.0 || b == 0.0 ? 0.0 : std::nextafter(a * b, infinity());
    }

    const Prepared& p_;
    const CandidateSet& set_;
    std::vector<double> base_rates_, base_destinations_, maximum_flows_, maximum_destinations_;
    std::array<Column, max_search_controls> columns_{};
    double base_cost_ = 0.0, maximum_cost_ = 0.0, relative_guard_ = 0.0;
    bool usable_ = false;
};

struct InfluenceWorkspace : Workspace {
    explicit InfluenceWorkspace(const ControlInfluence& map) : Workspace(map.prepared()) {}
    bool used_fallback = false;
    std::size_t fallback_count = 0;
};

inline Screen ControlInfluence::screen(const Values& values, double horizon,
                                      InfluenceWorkspace& w, double& cost) const {
    w.used_fallback = false;
    const auto fallback = [&] {
        w.used_fallback = true;
        ++w.fallback_count;
        return screen_candidate(p_, set_, values, horizon, w, cost);
    };
    if (!usable_ || !std::isfinite(maximum_cost_ * horizon)) return fallback();
    std::copy(base_rates_.begin(), base_rates_.end(), w.rates.begin());
    std::copy(base_destinations_.begin(), base_destinations_.end(), w.destination_totals.begin());
    std::copy(maximum_flows_.begin(), maximum_flows_.end(), w.flow_magnitudes.begin());
    double hourly_cost = base_cost_;
    for (std::size_t j = 0; j < set_.bindings.size(); ++j) {
        const auto& binding = p_.bindings[set_.bindings[j]];
        if (binding.kind == ControlKind::curtailment) w.curtailments[binding.slot] = values[j];
        else w.setpoints[binding.slot] = values[j];
        const auto& column = columns_[j];
        const double delta = values[j] - column.baseline;
        for (std::size_t k = 0; k < column.rate_count; ++k)
            w.rates[column.rates[k].index] += column.rates[k].coefficient * delta;
        if (column.destination.index != absent)
            w.destination_totals[column.destination.index] += column.destination.coefficient * delta;
        hourly_cost += column.cost * delta;
    }
    cost = hourly_cost * horizon; // Estimate only for rejected rows; see below.
    if (!std::isfinite(cost)) return fallback();
    bool failed = false, borderline = false;
    for (std::size_t i = 0; i < w.rates.size(); ++i) {
        const double end = p_.baseline.initial_m3[i] + horizon * w.rates[i];
        const double scale = 1.0 + std::abs(p_.baseline.initial_m3[i]) + horizon * maximum_flows_[i];
        const double guard = constraint_tolerance + relative_guard_ * scale;
        if (!std::isfinite(end) || !std::isfinite(guard)) return fallback();
        failed = failed || end < -guard || end > p_.baseline.capacity_m3[i] + guard;
        borderline = borderline || std::abs(end) <= guard ||
            std::abs(end - p_.baseline.capacity_m3[i]) <= guard;
    }
    for (std::size_t i = 0; i < w.destination_totals.size(); ++i) {
        const double total = w.destination_totals[i];
        const double guard = constraint_tolerance + relative_guard_ * (1.0 + maximum_destinations_[i]);
        if (!std::isfinite(total) || !std::isfinite(guard)) return fallback();
        failed = failed || total > p_.destination_capacities[i] + guard;
        borderline = borderline || std::abs(total - p_.destination_capacities[i]) <= guard;
    }
    if (borderline) return fallback();
    if (failed) return Screen::constraint_failure;

    // Reassociation of cost changes can change last bits, even with /fp:precise.
    // Every survivor gets the original source-then-route sum BEFORE sorting, so
    // near ties and the top-five cutoff retain the reference cost/index ordering.
    // Only rejected rows avoid this route pass; benchmarks include this work.
    hourly_cost = 0.0;
    for (std::size_t s = 0; s < p_.sources.size(); ++s)
        hourly_cost += w.curtailments[s] * p_.sources[s].cost;
    for (std::size_t r = 0; r < p_.routes.size(); ++r) {
        const auto& route = p_.routes[r];
        const double q = route.source_slot == absent ? w.setpoints[r] :
            (p_.sources[route.source_slot].production - w.curtailments[route.source_slot]) * route.source_fraction;
        hourly_cost += q * route.route_cost;
        if (route.destination_slot != absent) hourly_cost += q * route.destination_cost;
    }
    cost = hourly_cost * horizon;
    return std::isfinite(cost) ? Screen::potential : Screen::numerical_failure;
}

} // namespace breakwater::detail

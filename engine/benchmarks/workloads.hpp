#pragma once

// Private, deterministic A5 benchmark inputs; not application/scenario contracts.
#include "breakwater/search.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace breakwater::bench {

struct Workload {
    std::string name;
    Network network;
    std::vector<std::string> varying_control_ids;
};

namespace workload_detail {
inline std::string tank(std::size_t index) {
    return "storage:" + std::to_string(index);
}

inline Network custom_network() {
    Network network;
    constexpr std::size_t tank_count = 24;
    network.assets.reserve(tank_count + 4);
    network.routes.reserve(66);
    for (std::size_t i = 0; i < tank_count; ++i) {
        // Tank 1 has deliberately limited headroom. Raising ring:0 above 7 m3/h
        // overflows it in the 3 h forecast; unchanged internal cycles balance.
        network.assets.push_back({tank(i), "Custom storage " + std::to_string(i),
                                  AssetKind::tank, i == 1 ? 115.0 : 200.0, 100.0});
    }
    network.assets.push_back({"production:west", "West production", AssetKind::source,
                              0.0, 0.0, 6.0, 1.0, 4.0, 40.0});
    network.assets.push_back({"production:east", "East production", AssetKind::source,
                              0.0, 0.0, 4.0, 0.0, 2.0, 30.0});
    network.assets.push_back({"sink:shared:treatment", "Shared treatment", AssetKind::treatment,
                              8.0, 0.0, 0.0, 0.0, 0.0, 3.0});
    network.assets.push_back({"sink:shared:trucking", "Shared trucking", AssetKind::trucking,
                              3.0, 0.0, 0.0, 0.0, 0.0, 25.0});

    // Three balanced families give 60 directed internal edges, with cycles and
    // branching at the tanks. They are identical at every candidate count.
    for (std::size_t i = 0; i < tank_count; ++i) {
        network.routes.push_back({"pipe:ring:" + std::to_string(i), tank(i), tank((i + 1) % tank_count),
                                  10.0, 2.0, i == 0, true, .01 * static_cast<double>(1 + i % 3)});
    }
    for (std::size_t i = 0; i < tank_count; ++i) {
        network.routes.push_back({"pipe:offset5:" + std::to_string(i), tank(i), tank((i + 5) % tank_count),
                                  5.0, 1.0, false, true, .02});
    }
    for (std::size_t i = 0; i < tank_count; i += 2) {
        network.routes.push_back({"pipe:even:" + std::to_string(i), tank(i), tank((i + 2) % tank_count),
                                  3.0, .5, false, true, .03});
    }
    network.routes.push_back({"feed:west", "production:west", tank(0), 6.0, 6.0, false, true, .2});
    network.routes.push_back({"feed:east", "production:east", tank(12), 4.0, 4.0, false, true, .15});
    network.routes.push_back({"discharge:treatment:west", tank(0), "sink:shared:treatment", 8.0, 4.0, true, true, .3});
    network.routes.push_back({"discharge:treatment:east", tank(12), "sink:shared:treatment", 8.0, 4.0, true, true, .4});
    network.routes.push_back({"discharge:truck:west", tank(0), "sink:shared:trucking", 4.0, 1.0, false, true, .5});
    network.routes.push_back({"discharge:truck:east", tank(12), "sink:shared:trucking", 2.0, .5, false, true, .6});
    return network;
}

inline double radical_inverse(std::size_t index, std::size_t base) {
    double value = 0.0;
    double fraction = 1.0 / static_cast<double>(base);
    while (index != 0) {
        value += static_cast<double>(index % base) * fraction;
        index /= base;
        fraction /= static_cast<double>(base);
    }
    return value;
}
} // namespace workload_detail

inline std::vector<Workload> workloads() {
    std::vector<Workload> result;
    result.push_back({"canonical_incident", main_pump_failure(canonical_scenario()), {
        "route:bypass:flow", "route:backup_pump:flow", "route:truck_route:flow", "source:source_a:curtail"}});
    result.push_back({"custom_24_tanks", workload_detail::custom_network(), {
        "route:pipe:ring:0:flow", "route:discharge:treatment:west:flow",
        "route:discharge:truck:west:flow", "source:production:west:curtail"}});
    return result;
}

inline std::vector<CandidateControls> candidates(const Workload& workload, std::size_t count) {
    if (count == 0 || count > max_search_candidates || workload.varying_control_ids.size() != 4)
        throw std::invalid_argument("Benchmark needs 1..10000 candidates and four varying controls.");
    const auto catalog = control_catalog(workload.network);
    std::array<ControlDescriptor, 4> selected;
    for (std::size_t axis = 0; axis < selected.size(); ++axis) {
        const auto& id = workload.varying_control_ids[axis];
        if (std::find(workload.varying_control_ids.begin(), workload.varying_control_ids.begin() + axis, id)
            != workload.varying_control_ids.begin() + axis)
            throw std::invalid_argument("Benchmark varying control IDs must be distinct.");
        const auto found = std::find_if(catalog.begin(), catalog.end(), [&](const auto& control) { return control.id == id; });
        if (found == catalog.end() || found->maximum <= found->minimum)
            throw std::invalid_argument("Benchmark varying controls must have positive-width catalog bounds.");
        selected[axis] = *found;
    }

    // Row zero guarantees a nominally feasible plan even in the 100-row prefix.
    // Canonical values are the known mixed plan; the custom current settings are
    // balanced except tank12 loses .5 m3/h (well within its 100 m3 inventory).
    auto canonical_seed = incident_controls("mixed");
    constexpr std::array<std::size_t, 4> bases{2, 3, 5, 7};
    std::vector<CandidateControls> result;
    result.reserve(count);
    for (std::size_t row = 0; row < count; ++row) {
        CandidateControls candidate;
        candidate.overrides.reserve(selected.size());
        for (std::size_t axis = 0; axis < selected.size(); ++axis) {
            const auto& control = selected[axis];
            double value = control.value;
            if (row == 0 && workload.name == "canonical_incident") {
                const auto seed = std::find_if(canonical_seed.begin(), canonical_seed.end(),
                                              [&](const auto& entry) { return entry.id == control.id; });
                if (seed == canonical_seed.end()) throw std::invalid_argument("Unknown canonical benchmark seed control.");
                value = seed->value;
            } else if (row != 0) {
                value = control.minimum + (control.maximum - control.minimum) *
                    workload_detail::radical_inverse(row, bases[axis]);
            }
            candidate.overrides.push_back({control.id, value});
        }
        result.push_back(std::move(candidate));
    }
    // These Halton prefixes depend only on row index, never count. Axis zero's
    // base-two radical inverse is injective for rows1..9999. The seeded row is
    // distinct too: canonical backup is at its upper bound; custom ring=2/10 is
    // not a base-two fraction in this finite prefix. No repeated candidate rows.
    return result;
}

} // namespace breakwater::bench

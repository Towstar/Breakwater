#pragma once

// Private production kernels shared by simulate() and the native benchmark.
// Callers provide dimensionally valid, disjoint workspace; these functions do not
// allocate, validate topology, change floating-point policy or establish feasibility.
#include "breakwater/engine.hpp"

#include <algorithm>
#include <span>

namespace breakwater::detail {

struct StorageRatesView {
    std::span<const double> external_in_m3h;
    std::span<const double> external_out_m3h;
    std::span<const InternalEdge> edges;
};

inline void build_incidence(StorageRatesView view, std::span<double> incidence) {
    std::fill(incidence.begin(), incidence.end(), 0.0);
    for (std::size_t e = 0; e < view.edges.size(); ++e) {
        incidence[view.edges[e].source * view.edges.size() + e] -= 1.0;
        incidence[view.edges[e].target * view.edges.size() + e] += 1.0;
    }
}

inline void sparse_rates(StorageRatesView view, std::span<double> rates) {
    for (std::size_t i = 0; i < rates.size(); ++i)
        rates[i] = view.external_in_m3h[i] - view.external_out_m3h[i];
    for (const auto& edge : view.edges) {
        rates[edge.source] -= edge.flow_m3h;
        rates[edge.target] += edge.flow_m3h;
    }
}

inline void dense_rates(StorageRatesView view, std::span<const double> incidence,
                        std::span<double> rates) {
    for (std::size_t i = 0; i < rates.size(); ++i) {
        rates[i] = view.external_in_m3h[i] - view.external_out_m3h[i];
        for (std::size_t e = 0; e < view.edges.size(); ++e)
            rates[i] += incidence[i * view.edges.size() + e] * view.edges[e].flow_m3h;
    }
}

inline void euler_update(std::span<double> state, std::span<const double> rates, double dt) {
    for (std::size_t i = 0; i < state.size(); ++i) state[i] += dt * rates[i];
}

} // namespace breakwater::detail

#pragma once

// Private storage-grid policy shared by simulation, search request checks and
// benchmark sampling. Multiples are evaluated in precise double arithmetic.
#include "breakwater/types.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace breakwater::detail {
inline constexpr std::size_t max_storage_updates = 10000;

inline void check_storage_config(SimulationConfig config) {
    if (!std::isfinite(config.horizon_h) || !std::isfinite(config.step_h) ||
        config.horizon_h <= 0.0 || config.horizon_h > 24.0 || config.step_h <= 0.0)
        throw std::invalid_argument("Forecast needs 0 < horizon <= 24 h and a finite positive step.");
    // Test actual rounded multiples, not ceil(horizon / step): division can
    // round down, round up, or underflow. An overflowing positive product is
    // harmless here: one interval already covers the bounded horizon.
    if (config.horizon_h > static_cast<double>(max_storage_updates) * config.step_h)
        throw std::invalid_argument("Storage forecasts support at most 10000 updates including the final partial interval.");
}

inline std::vector<double> storage_sample_times(SimulationConfig config) {
    check_storage_config(config);
    std::vector<double> times;
    // This quotient is only a bounded allocation hint, never a grid decision.
    const auto hint = static_cast<std::size_t>(std::ceil(config.horizon_h / config.step_h));
    times.reserve(std::min(max_storage_updates + 1, hint + 2));
    times.push_back(0.0);
    for (std::size_t k = 1; k <= max_storage_updates; ++k) {
        const double time = static_cast<double>(k) * config.step_h;
        if (time >= config.horizon_h) {
            times.push_back(config.horizon_h); // Exact requested endpoint, once.
            return times;
        }
        if (time <= times.back())
            throw std::invalid_argument("Requested storage sample interval is not representable.");
        times.push_back(time); // Preserve every requested interior multiple.
    }
    throw std::invalid_argument("Storage forecasts support at most 10000 updates.");
}
} // namespace breakwater::detail

#pragma once

// Private A5 measurement helpers. Callers must make computed results observable;
// compiler fences alone do not prevent removal of a pure, unused calculation.
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <ostream>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace breakwater::bench {

using Clock = std::chrono::steady_clock;

template<class Fn>
double sample_ms(Fn&& function, std::size_t iterations) {
    if (iterations == 0) throw std::invalid_argument("Timing requires positive iterations.");
    std::atomic_signal_fence(std::memory_order_seq_cst);
    const auto start = Clock::now();
    std::atomic_signal_fence(std::memory_order_seq_cst);
    for (std::size_t i = 0; i < iterations; ++i) function();
    std::atomic_signal_fence(std::memory_order_seq_cst);
    const auto finish = Clock::now();
    std::atomic_signal_fence(std::memory_order_seq_cst);
    return std::chrono::duration<double, std::milli>(finish - start).count() /
           static_cast<double>(iterations);
}

template<class Fn>
std::size_t calibrate(Fn&& function, double target_ms = 10.0,
                      std::size_t max_iterations = 1024) {
    if (!std::isfinite(target_ms) || target_ms <= 0.0 || max_iterations == 0)
        throw std::invalid_argument("Calibration requires a positive target and iteration cap.");
    std::size_t iterations = 1;
    for (;;) {
        const double elapsed = sample_ms(function, iterations) * static_cast<double>(iterations);
        if (elapsed >= target_ms || iterations == max_iterations) return iterations;
        // Clamp before multiplication so a caller-supplied large cap cannot overflow.
        iterations = iterations > max_iterations / 2 ? max_iterations : iterations * 2;
    }
}

struct Summary {
    double minimum_ms;
    double median_ms;
    double mean_ms;
    double p90_ms;
    double maximum_ms;
    double stddev_ms;
};

inline Summary summarize(std::span<const double> samples) {
    if (samples.empty()) throw std::invalid_argument("Cannot summarize empty timing samples.");
    for (const double value : samples)
        if (!std::isfinite(value) || value < 0.0)
            throw std::invalid_argument("Timing samples must be finite and nonnegative.");
    std::vector<double> ordered(samples.begin(), samples.end());
    std::sort(ordered.begin(), ordered.end());
    const auto count = ordered.size();
    const auto middle = count / 2;
    // Halves avoid overflow when both middle observations are large finite values.
    const double median = count % 2 == 0 ? ordered[middle - 1] / 2.0 + ordered[middle] / 2.0
                                          : ordered[middle];
    // Normalize before summation/squaring: MSVC long double has double precision,
    // and all-finite observations should not overflow an auxiliary sum/variance.
    const double scale = ordered.back();
    double normalized_mean = 0.0;
    if (scale > 0.0)
        for (std::size_t i = 0; i < count; ++i)
            normalized_mean += (ordered[i] / scale - normalized_mean) /
                               static_cast<double>(i + 1);
    double normalized_variance = 0.0;
    if (scale > 0.0) for (const double value : ordered) {
        const double deviation = value / scale - normalized_mean;
        normalized_variance += deviation * deviation / static_cast<double>(count);
    }
    // ceil(0.9*n)-1, calculated with integers without forming 9*n.
    const auto rank90 = count - count / 10;
    return {ordered.front(), median, scale * normalized_mean, ordered[rank90 - 1],
            ordered.back(), scale * std::sqrt(normalized_variance)};
}

inline void json_string(std::ostream& out, const std::string& value) {
    constexpr char hex[] = "0123456789abcdef";
    out << '"';
    for (const unsigned char c : value) {
        if (c == '"') out << "\\\"";
        else if (c == '\\') out << "\\\\";
        else if (c < 0x20) out << "\\u00" << hex[c >> 4] << hex[c & 15];
        else out << static_cast<char>(c);
    }
    out << '"';
}

} // namespace breakwater::bench

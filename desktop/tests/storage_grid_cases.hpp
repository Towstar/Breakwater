#pragma once

// Independent acceptance expectations: counts are fixed, never inferred from a
// floating-point quotient or the implementation under test.
#include <limits>
#include <vector>

namespace storage_grid_test {
struct Case {
    const char* name;
    double horizon;
    double step;
    std::vector<double> times;
};
inline std::vector<Case> cases() {
    std::vector<Case> result{
        {"rounded-third-tail", .225, .075, {0, .075, .15, .22499999999999998, .225}},
        {"one-ulp-tail", .9000000000000001, .1,
            {0, .1, .2, .30000000000000004, .4, .5, .6000000000000001,
             .7000000000000001, .8, .9, .9000000000000001}},
        {"rounded-endpoint-once", .30000000000000004, .1, {0, .1, .2, .30000000000000004}},
        {"partial-step", 1.1, .3, {0, .3, .6, .8999999999999999, 1.1}},
        {"smallest-positive", std::numeric_limits<double>::denorm_min(), 24,
            {0, std::numeric_limits<double>::denorm_min()}}
    };
    for (const auto& spec : std::vector<Case>{{"rounded-budget", .0082, 8.2e-7, {}},
                                            {"exact-budget", 1, .0001, {}}}) {
        auto item = spec;
        for (int k = 0; k < 10000; ++k) item.times.push_back(static_cast<double>(k) * item.step);
        item.times.push_back(item.horizon);
        result.push_back(item);
    }
    return result;
}
} // namespace storage_grid_test

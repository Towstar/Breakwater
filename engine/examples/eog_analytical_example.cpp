#include "breakwater/eog_analytical.hpp"
#include <iomanip>
#include <iostream>

int main() {
    using namespace breakwater::eog::analytical;
    // The public opening illustration, expressed as a standalone numerical fixture.
    const FailureFixture fixture{{60.0, 3.0}, 150.0, 905.0, {{"demo-system", 2}},
        {{"demo-facility", "demo-system", 800.0, 50.0, 92.0, 6.0, 75.0, 230.0, false,
          {{"well-1", 450.0, 560.0, 2000.0}, {"well-2", 700.0, 170.0, 600.0},
           {"well-3", 650.0, 390.0, 1400.0}}}}};
    const auto baseline = assess_failure_fixture(fixture);
    const auto hauled = assess_failure_fixture(fixture, {{}, {{"demo-facility", 0, 0.0, 1}}});
    const auto shut = assess_failure_fixture(fixture, {{{"demo-facility", "well-2", 2.0}}, {}});
    if (!baseline.valid() || !hauled.avoids_shutdown() || !shut.avoids_shutdown()) return 1;
    std::cout << std::fixed << std::setprecision(6)
        << "method=" << analytical_version << " (private E1 fixture; no detection/search/worker)\n"
        << "no-action: shutdown_after_h=" << *baseline.facilities[0].shutdown_after_h
        << " cost=" << *baseline.total_cost << '\n'
        << "one-truck: arrival_h=" << hauled.calendars[0].arrivals_h[0]
        << " free_h=" << hauled.calendars[0].free_after_h
        << " final_bbl=" << hauled.facilities[0].trajectory.back().inventory_bbl
        << " cost=" << *hauled.total_cost << '\n'
        << "well-2-off-at-2h: final_bbl=" << shut.facilities[0].trajectory.back().inventory_bbl
        << " cost=" << *shut.total_cost << '\n';
    return 0;
}

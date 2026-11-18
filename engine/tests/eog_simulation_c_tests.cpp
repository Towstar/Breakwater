#include "breakwater/eog_simulation.h"
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace {
unsigned checks{};
void check(bool value) { ++checks; if (!value) throw std::runtime_error("Private synthetic C ABI assertion failed."); }
int cancel(void*) { return 1; }
}
int main() {
    try {
        check(bwes_abi_version() == 1 && bwes_layout_size(0) == 0 && bwes_layout_size(12) == 0);
        const uint32_t sizes[] = {sizeof(bwes_well), sizeof(bwes_facility), sizeof(bwes_failure), sizeof(bwes_rate_change), sizeof(bwes_scenario),
            sizeof(bwes_facility_view), sizeof(bwes_well_view), sizeof(bwes_truck_view), sizeof(bwes_snapshot), sizeof(bwes_point), sizeof(bwes_action)};
        for (uint32_t i = 0; i < 11; ++i) check(bwes_layout_size(i + 1) == sizes[i]);
        uint32_t fleet = 1;
        bwes_well well{86400, 172800, 0, 1};
        bwes_facility facility{0, 0, 1, 0, 100, 50, 70, 30, 90, 259200, 300000, 2.0 / 60, 20.0 / 60};
        bwes_scenario scenario{1, sizeof(bwes_scenario), 180, 1, 0, 20, 3, &fleet, &facility, &well, nullptr, nullptr,
            1, 1, 1, 0, 0, 262144, 1000000, 16384};
        bwes_plant* raw{}; check(bwes_create(&scenario, &raw) == 0 && raw);
        std::unique_ptr<bwes_plant, decltype(&bwes_destroy)> plant(raw, &bwes_destroy);
        bwes_snapshot snapshot{}; snapshot.abi_version = 1; snapshot.struct_size = sizeof(snapshot);
        check(bwes_get_snapshot(raw, &snapshot) == 0 && snapshot.facility_count == 1 && snapshot.has_cost && snapshot.total_cost == 0);
        check(snapshot.facilities[0].volume_bbl == 50 && snapshot.wells[0].producing == 1);
        bwes_snapshot wrong{}; check(bwes_get_snapshot(raw, &wrong) == 1 && wrong.struct_size == 0);
        uint32_t written = 123;
        check(bwes_copy_trace(raw, 0, nullptr, 0, &written) == 0 && written == 0);
        struct { bwes_point point; double sentinel; } bounded{{}, 123456.5};
        check(bwes_copy_trace(raw, 0, &bounded.point, 1, &written) == 0 && written == 1 && bounded.sentinel == 123456.5);
        check(bwes_copy_trace(raw, 1, &bounded.point, 1, &written) == 0 && written == 0);
        check(bwes_copy_trace(raw, 100, &bounded.point, 1, &written) == 1 && bounded.sentinel == 123456.5);
        uint32_t advanced = 0;
        check(bwes_advance(raw, -1, nullptr, nullptr, &advanced) == 1);
        check(bwes_advance(raw, 180, nullptr, nullptr, &advanced) == 0 && advanced == 1);
        check(bwes_get_snapshot(raw, &snapshot) == 0 && snapshot.status == 1 && snapshot.has_cost);
        check(snapshot.facilities[0].volume_bbl == 50 && snapshot.total_cost == 0);
        check(bwes_reset(raw) == 0 && bwes_advance(raw, 0, cancel, nullptr, &advanced) == 0 && advanced == 0);
        check(bwes_get_snapshot(raw, &snapshot) == 0 && snapshot.status == 4 && !snapshot.has_cost && std::isnan(snapshot.total_cost));
        scenario.struct_size = 0; bwes_plant* invalid = raw;
        check(bwes_create(&scenario, &invalid) == 1 && invalid == nullptr);
        scenario.struct_size = sizeof(scenario); well.productive = 2;
        check(bwes_create(&scenario, &invalid) == 1 && invalid == nullptr);
        bwes_destroy(nullptr);
        std::cout << checks << " synthetic C ABI checks passed\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}

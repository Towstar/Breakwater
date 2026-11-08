#include "breakwater/eog_response.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {
int checks = 0;
void expect(bool condition) { ++checks; if (!condition) throw std::runtime_error("committed-state check " + std::to_string(checks)); }
struct Fixture {
    bweog_fleet fleet{1};
    bweog_well well{86400, 86400, 0, 1};
    bweog_facility facility{0, 0, 1, 0, 100, 80, 90, 20, 2.0 / 60, 5.0 / 60};
    bweog_snapshot snapshot{1, sizeof(bweog_snapshot), 1, 0, 10, 3, &fleet, &facility, &well, 1, 1, 1};
    bweog_horizon horizon{0, 20};
    bweog_plan plan{1, sizeof(bweog_plan), &horizon, nullptr, nullptr, nullptr, 1, 0, 0, 0};
    bweog_occupied occupied{0, 0, 10};
    bweog_pending pickup{0, 0, 0, 5};
    bweog_commitments commitments{1, sizeof(bweog_commitments), &occupied, &pickup, nullptr, 1, 1, 0};
    bweog_result_view run(int expected_code = 0) {
        bweog_result* result = nullptr;
        const int code = bweog_evaluate_committed(&snapshot, &plan, &commitments, &result);
        expect(code == expected_code);
        bweog_result_view view{};
        if (code) { expect(!result); return view; }
        expect(result && bweog_get_result(result, &view) == 0);
        // Scalar output is copied before destroy; these checks never dereference
        // the deliberately expired array pointers in the returned scalar view.
        if (view.status == BWEOG_AVAILABLE) {
            expect(view.facility_count == 1 && view.facilities[0].balance_residual_bbl == 0);
        }
        bweog_destroy(result); return view;
    }
};
}
int main() {
    try {
        expect(bweog_abi_version() == 1 && bweog_layout_size(14) == 0);
        expect(std::string(bweog_method()) == "eog-native-seconds-0.1.1");
        expect(std::string(bweog_committed_method()) == "eog-native-committed-seconds-0.1.0");
        expect(bweog_committed_layout_size(1) == sizeof(bweog_occupied));
        expect(bweog_committed_layout_size(2) == sizeof(bweog_pending));
        expect(bweog_committed_layout_size(3) == sizeof(bweog_commitments));
        expect(bweog_committed_layout_size(4) == 0);
        Fixture f;
        auto r = f.run(); expect(r.status == BWEOG_AVAILABLE && r.total_cost == 3 && r.lost_revenue == 0);
        bweog_trip trip{12, 10, 3};
        bweog_job job{0, 0, 0, 0, 1, 10, 2.0 / 60, 5.0 / 60, 17};
        f.plan.jobs = &job; f.plan.trips = &trip; f.plan.job_count = f.plan.trip_count = 1;
        r = f.run(); expect(r.status == BWEOG_AVAILABLE && r.total_cost == 6 && !r.automatic_shutdown);
        job.called_s = std::nextafter(10.0, 0.0); trip.arrival_s = job.called_s + 2; job.available_again_s = trip.arrival_s + 5;
        r = f.run(); expect(r.status == BWEOG_INVALID && std::isnan(r.total_cost));
        f.plan.job_count = f.plan.trip_count = 0;
        uint32_t stopped = 0; f.commitments.stopped_wells = &stopped; f.commitments.stopped_count = 1;
        r = f.run(); expect(r.status == BWEOG_AVAILABLE && r.lost_revenue == 20 && r.total_cost == 23);
        f.facility.initial_bbl = 90; f.facility.already_shut_in = 1;
        r = f.run(); expect(r.status == BWEOG_AVAILABLE && r.lost_revenue == 20 && r.total_cost == 23);
        bweog_stop stop{0, 0, 0, 20}; f.plan.stops = &stop; f.plan.stop_count = 1;
        r = f.run(); expect(r.status == BWEOG_INVALID && std::isnan(r.truck_cost));
        f.plan.stop_count = 0; f.commitments.stopped_count = 0;
        f.facility.initial_bbl = 1; f.facility.already_shut_in = 0; f.well.water_bpd = 0;
        r = f.run(); expect(r.status == BWEOG_INVALID && std::isnan(r.total_cost));
        f.pickup.arrival_s = 0; f.run(1); f.pickup.arrival_s = 5;
        f.commitments.occupied = nullptr; f.run(1); f.commitments.occupied = &f.occupied;
        f.commitments.struct_size = 0; f.run(1); f.commitments.struct_size = sizeof(bweog_commitments);
        f.commitments.pickup_count = 16385; f.run(1); f.commitments.pickup_count = 1;
        f.facility.initial_bbl = 80; f.well.water_bpd = 86400;
        f.snapshot.cost_per_trip = 1e308; trip = {12, 10, 1e308}; job = {0,0,0,0,1,10,2.0/60,5.0/60,17};
        f.plan.job_count = f.plan.trip_count = 1;
        r = f.run(); expect(r.status == BWEOG_UNAVAILABLE && std::isnan(r.total_cost));
        std::cout << "Committed-state native checks passed: " << checks << '\n';
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}

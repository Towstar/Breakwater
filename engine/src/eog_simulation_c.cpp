#include "breakwater/eog_simulation.h"
#include "breakwater/eog_simulation.hpp"
#include <algorithm>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace sim = breakwater::eog::synthetic;
namespace {
thread_local std::string error_message;
void require(bool test, const char* message) { if (!test) throw std::invalid_argument(message); }
template<class F> int guarded(F body) noexcept {
    try { error_message.clear(); body(); return 0; }
    catch (const std::invalid_argument& e) { error_message = e.what(); return 1; }
    catch (const std::runtime_error& e) { error_message = e.what(); return 2; }
    catch (...) { error_message = "Native synthetic execution could not complete."; return 2; }
}
}
struct bwes_plant {
    sim::Simulator plant;
    std::vector<bwes_facility_view> facilities;
    std::vector<bwes_well_view> wells;
    std::vector<bwes_truck_view> trucks;
    std::string diagnostic;
    explicit bwes_plant(const sim::Scenario& scenario) : plant(scenario) {}
};
extern "C" {
uint32_t bwes_abi_version(void) { return BWES_ABI_VERSION; }
const char* bwes_method(void) { return sim::method; }
const char* bwes_last_error(void) { return error_message.c_str(); }
uint32_t bwes_layout_size(uint32_t id) {
    switch (id) {
    case 1: return sizeof(bwes_well); case 2: return sizeof(bwes_facility);
    case 3: return sizeof(bwes_failure); case 4: return sizeof(bwes_rate_change);
    case 5: return sizeof(bwes_scenario); case 6: return sizeof(bwes_facility_view);
    case 7: return sizeof(bwes_well_view); case 8: return sizeof(bwes_truck_view);
    case 9: return sizeof(bwes_snapshot); case 10: return sizeof(bwes_point); case 11: return sizeof(bwes_action);
    default: return 0;
    }
}
int bwes_create(const bwes_scenario* input, bwes_plant** out) {
    if (!out) return 1;
    *out = nullptr;
    return guarded([&] {
        require(input && input->abi_version == BWES_ABI_VERSION && input->struct_size == sizeof(bwes_scenario), "Invalid synthetic ABI/layout.");
        require(input->fleet_count > 0 && input->fleet_count <= 64 && input->fleet_sizes &&
                input->facility_count > 0 && input->facility_count <= 256 && input->facilities &&
                input->well_count <= 16384 && (!input->well_count || input->wells) &&
                input->failure_count <= 16384 && (!input->failure_count || input->failures) &&
                input->rate_change_count <= 16384 && (!input->rate_change_count || input->rate_changes), "Invalid synthetic array/count.");
        sim::Scenario scenario;
        scenario.duration_s = input->duration_s; scenario.oil_per_bbl = input->oil_per_bbl;
        scenario.gas_per_mcf = input->gas_per_mcf; scenario.truck_capacity_bbl = input->truck_capacity_bbl;
        scenario.cost_per_trip = input->cost_per_trip; scenario.point_limit = input->point_limit;
        scenario.event_limit = input->event_limit; scenario.trip_limit = input->trip_limit;
        for (uint32_t i = 0; i < input->fleet_count; ++i) scenario.fleet_sizes.push_back(input->fleet_sizes[i]);
        for (uint32_t i = 0; i < input->facility_count; ++i) {
            const auto& f = input->facilities[i]; require(f.initially_running <= 1, "Invalid initial synthetic pump flag.");
            scenario.facilities.push_back({f.system, f.first_well, f.well_count, f.capacity_bbl, f.initial_bbl,
                f.start_bbl, f.stop_bbl, f.shutdown_bbl, f.pump_bpd, f.pipeline_capacity_bpd,
                f.response_min, f.cycle_min, f.initially_running != 0});
        }
        for (uint32_t i = 0; i < input->well_count; ++i) {
            const auto& w = input->wells[i]; require(w.productive <= 1, "Invalid initial synthetic well flag.");
            scenario.wells.push_back({w.water_bpd, w.oil_bpd, w.gas_mcfd, w.productive != 0});
        }
        for (uint32_t i = 0; i < input->failure_count; ++i) {
            const auto& f = input->failures[i]; scenario.failures.push_back({f.facility, f.start_s, f.repair_s});
        }
        for (uint32_t i = 0; i < input->rate_change_count; ++i) {
            const auto& r = input->rate_changes[i]; scenario.rate_changes.push_back({r.well, r.elapsed_s, r.water_bpd, r.oil_bpd, r.gas_mcfd});
        }
        *out = new bwes_plant(scenario);
    });
}
int bwes_advance(bwes_plant* handle, double elapsed_s, bwes_cancel_fn cancel, void* context, uint32_t* advanced) {
    return guarded([&] {
        require(handle && advanced, "Invalid synthetic advance output/handle.");
        *advanced = handle->plant.advance(elapsed_s, cancel ? std::function<bool()>([&] { return cancel(context) != 0; }) : std::function<bool()>{}) ? 1u : 0u;
    });
}
int bwes_get_snapshot(bwes_plant* handle, bwes_snapshot* out) {
    return guarded([&] {
        require(handle && out && out->abi_version == BWES_ABI_VERSION && out->struct_size == sizeof(bwes_snapshot), "Invalid synthetic snapshot ABI/handle.");
        const auto view = handle->plant.snapshot();
        handle->facilities.clear(); handle->wells.clear(); handle->trucks.clear(); handle->diagnostic = view.diagnostic;
        for (const auto& f : view.facilities) handle->facilities.push_back({f.volume_bbl, f.inflow_bpd, f.pump_flow_bpd,
            static_cast<uint32_t>(f.pump_status), f.automatic_shut_in ? 1u : 0u, f.produced_bbl, f.pumped_bbl,
            f.trucked_bbl, f.lost_revenue, f.balance_residual_bbl, f.event_boundary_rounding_bbl});
        for (const auto& w : view.wells) handle->wells.push_back({w.producing ? 1u : 0u, w.water_bpd, w.oil_bpd, w.gas_mcfd, w.lost_revenue});
        for (const auto& t : view.trucks) handle->trucks.push_back({static_cast<uint32_t>(t.system), static_cast<uint32_t>(t.truck), t.busy ? 1u : 0u, t.available_again_s});
        const double absent = std::numeric_limits<double>::quiet_NaN();
        *out = {BWES_ABI_VERSION, sizeof(bwes_snapshot), static_cast<uint32_t>(view.status), view.total_cost ? 1u : 0u,
            static_cast<uint32_t>(view.completed_full_loads), static_cast<uint32_t>(handle->facilities.size()),
            static_cast<uint32_t>(handle->wells.size()), static_cast<uint32_t>(handle->trucks.size()),
            static_cast<uint32_t>(handle->plant.trace().size()), view.elapsed_s,
            view.truck_cost.value_or(absent), view.lost_revenue.value_or(absent), view.total_cost.value_or(absent),
            handle->diagnostic.c_str(), handle->facilities.data(), handle->wells.data(), handle->trucks.data()};
    });
}
int bwes_dispatch(bwes_plant* handle, uint32_t facility, uint32_t truck, uint32_t trips, bwes_action* out) {
    return guarded([&] {
        require(handle && out, "Invalid synthetic dispatch output/handle.");
        const auto result = handle->plant.dispatch(facility, truck, trips); handle->diagnostic = result.diagnostic;
        *out = {result.accepted ? 1u : 0u, handle->diagnostic.c_str()};
    });
}
int bwes_shut_in(bwes_plant* handle, uint32_t well, bwes_action* out) {
    return guarded([&] {
        require(handle && out, "Invalid synthetic shut-in output/handle.");
        const auto result = handle->plant.shut_in(well); handle->diagnostic = result.diagnostic;
        *out = {result.accepted ? 1u : 0u, handle->diagnostic.c_str()};
    });
}
int bwes_copy_trace(const bwes_plant* handle, uint32_t offset, bwes_point* out, uint32_t capacity, uint32_t* written) {
    return guarded([&] {
        require(handle && written && capacity <= 262144 && (!capacity || out), "Invalid synthetic trace buffer/count.");
        const auto& trace = handle->plant.trace(); require(offset <= trace.size(), "Synthetic trace offset exceeds retained history.");
        *written = static_cast<uint32_t>(std::min<std::size_t>(capacity, trace.size() - offset));
        for (uint32_t i = 0; i < *written; ++i) {
            const auto& p = trace[offset + i];
            out[i] = {p.elapsed_s, p.volume_bbl, static_cast<uint32_t>(p.facility), static_cast<uint32_t>(p.kind),
                      static_cast<uint32_t>(p.pump_status), p.automatic_shut_in ? 1u : 0u};
        }
    });
}
int bwes_reset(bwes_plant* handle) { return guarded([&] { require(handle, "Invalid synthetic reset handle."); handle->plant.reset(); }); }
void bwes_destroy(bwes_plant* handle) { delete handle; }
}

#ifndef BREAKWATER_EOG_SIMULATION_H
#define BREAKWATER_EOG_SIMULATION_H
/* Private computational ABI. Arrays are borrowed only during create; the plant
 * owns a copy. A handle must be valid and used serially. Snapshot strings/arrays
 * live until its next API call; callers copy them before mutating/reset/destroy.
 * Future incident/forcing schedules have no read API. This is not a JSON wire. */
#include <stdint.h>
#if defined(_WIN32)
# if defined(BWES_BUILD)
#  define BWES_API __declspec(dllexport)
# else
#  define BWES_API __declspec(dllimport)
# endif
#else
# define BWES_API __attribute__((visibility("default")))
#endif
#ifdef __cplusplus
extern "C" {
#endif
#define BWES_ABI_VERSION 1u
typedef struct { double water_bpd, oil_bpd, gas_mcfd; uint32_t productive; } bwes_well;
typedef struct {
    uint32_t system, first_well, well_count, initially_running;
    double capacity_bbl, initial_bbl, start_bbl, stop_bbl, shutdown_bbl;
    double pump_bpd, pipeline_capacity_bpd, response_min, cycle_min;
} bwes_facility;
typedef struct { uint32_t facility; double start_s, repair_s; } bwes_failure;
typedef struct { uint32_t well; double elapsed_s, water_bpd, oil_bpd, gas_mcfd; } bwes_rate_change;
typedef struct {
    uint32_t abi_version, struct_size;
    double duration_s, oil_per_bbl, gas_per_mcf, truck_capacity_bbl, cost_per_trip;
    const uint32_t* fleet_sizes;
    const bwes_facility* facilities;
    const bwes_well* wells;
    const bwes_failure* failures;
    const bwes_rate_change* rate_changes;
    uint32_t fleet_count, facility_count, well_count, failure_count, rate_change_count;
    uint32_t point_limit, event_limit, trip_limit;
} bwes_scenario;
typedef struct {
    double volume_bbl, inflow_bpd, pump_flow_bpd;
    uint32_t pump_status, automatic_shut_in;
    double produced_bbl, pumped_bbl, trucked_bbl, lost_revenue;
    double balance_residual_bbl, event_boundary_rounding_bbl;
} bwes_facility_view;
typedef struct { uint32_t producing; double water_bpd, oil_bpd, gas_mcfd, lost_revenue; } bwes_well_view;
typedef struct { uint32_t system, truck, busy; double available_again_s; } bwes_truck_view;
/* Status 0 running, 1 completed, 2 invalid_action, 3 unavailable, 4 cancelled,
 * 5 limit. Costs are reportable iff has_cost=1; absent costs are NaN, never 0. */
typedef struct {
    uint32_t abi_version, struct_size, status, has_cost, completed_full_loads;
    uint32_t facility_count, well_count, truck_count, trace_count;
    double elapsed_s, truck_cost, lost_revenue, total_cost;
    const char* diagnostic;
    const bwes_facility_view* facilities;
    const bwes_well_view* wells;
    const bwes_truck_view* trucks;
} bwes_snapshot;
typedef struct {
    double elapsed_s, volume_bbl;
    uint32_t facility, kind, pump_status, automatic_shut_in;
} bwes_point;
typedef struct { uint32_t accepted; const char* diagnostic; } bwes_action;
typedef struct bwes_plant bwes_plant;
typedef int (*bwes_cancel_fn)(void* context);

BWES_API uint32_t bwes_abi_version(void);
/* Layout IDs 1..11 follow the typedef order above (well through action). */
BWES_API uint32_t bwes_layout_size(uint32_t);
BWES_API const char* bwes_method(void);
BWES_API const char* bwes_last_error(void);
/* Return 0: structured execution; 1: invalid ABI/input/call; 2: unavailable or
 * allocation/internal exception. No exception crosses this boundary. */
BWES_API int bwes_create(const bwes_scenario*, bwes_plant**);
BWES_API int bwes_advance(bwes_plant*, double elapsed_s, bwes_cancel_fn, void* context, uint32_t* advanced);
BWES_API int bwes_get_snapshot(bwes_plant*, bwes_snapshot*);
BWES_API int bwes_dispatch(bwes_plant*, uint32_t facility, uint32_t truck, uint32_t trips, bwes_action*);
BWES_API int bwes_shut_in(bwes_plant*, uint32_t well, bwes_action*);
/* Copy at most capacity points from offset to caller-owned storage. No writes
 * outside that capacity, no future points, no silent sampling/decimation. */
BWES_API int bwes_copy_trace(const bwes_plant*, uint32_t offset, bwes_point*, uint32_t capacity, uint32_t* written);
BWES_API int bwes_reset(bwes_plant*);
BWES_API void bwes_destroy(bwes_plant*);
#ifdef __cplusplus
}
#endif
#endif

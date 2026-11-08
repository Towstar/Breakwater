#ifndef BREAKWATER_EOG_RESPONSE_H
#define BREAKWATER_EOG_RESPONSE_H

/* Private computational ABI, not the application JSON contract. All arrays are
 * borrowed for evaluate(); returned arrays live until destroy(). Indices map to
 * caller-owned opaque asset IDs. Caller must supply valid allocated buffers.
 * Units: elapsed seconds, bbl, bpd, mcfd and synthetic currency. No hour conversion
 * of action timestamps, feasibility tolerances, partial loads or volume clipping.
 */
#include <stdint.h>
#if defined(_WIN32)
# if defined(BWEOG_BUILD)
#  define BWEOG_API __declspec(dllexport)
# else
#  define BWEOG_API __declspec(dllimport)
# endif
#else
# define BWEOG_API __attribute__((visibility("default")))
#endif
#ifdef __cplusplus
extern "C" {
#endif

#define BWEOG_ABI_VERSION 1u
#define BWEOG_NO_INDEX UINT32_MAX

typedef struct { uint32_t trucks; } bweog_fleet;
typedef struct {
    double water_bpd, oil_bpd, gas_mcfd;
    uint32_t productive; /* known pre-trip production eligibility, 0 or 1 */
} bweog_well;
typedef struct {
    uint32_t system, first_well, well_count, already_shut_in;
    double capacity_bbl, initial_bbl, shutdown_bbl, repair_s;
    double response_min, cycle_min;
} bweog_facility;
typedef struct {
    uint32_t abi_version, struct_size;
    double oil_per_bbl, gas_per_mcf, truck_capacity_bbl, cost_per_trip;
    const bweog_fleet* fleets;
    const bweog_facility* facilities;
    const bweog_well* wells;
    uint32_t fleet_count, facility_count, well_count;
} bweog_snapshot;
typedef struct { uint32_t facility; double repair_s; } bweog_horizon;
typedef struct { uint32_t facility, well; double start_s, end_s; } bweog_stop;
typedef struct { double arrival_s, volume_bbl, cost; } bweog_trip;
typedef struct {
    uint32_t facility, system, truck, first_trip, trip_count;
    double called_s, response_min, cycle_min, available_again_s;
} bweog_job;
typedef struct {
    uint32_t abi_version, struct_size;
    const bweog_horizon* horizons;
    const bweog_stop* stops;
    const bweog_job* jobs;
    const bweog_trip* trips;
    uint32_t horizon_count, stop_count, job_count, trip_count;
} bweog_plan;

/* Status: AVAILABLE includes a valid do-nothing automatic shut-in. A complete
 * available evaluation is the only state with a cost. INVALID actions are never
 * repaired. UNAVAILABLE denotes numerical/representation limits, not zero cost.
 */
enum bweog_status { BWEOG_AVAILABLE = 0, BWEOG_INVALID = 1, BWEOG_UNAVAILABLE = 2 };
enum bweog_code {
    BWEOG_HORIZONS = 1, BWEOG_STOP = 2, BWEOG_JOB = 3,
    BWEOG_TRIP = 4, BWEOG_FLEET = 5, BWEOG_INVENTORY = 6,
    BWEOG_NUMERICAL = 7, BWEOG_LIMIT = 8
};
typedef struct {
    uint32_t code, facility, action, has_time;
    double elapsed_s;
} bweog_violation;
/* point kind: 0 continuous/breakpoint, 1 before pickup, 2 after pickup,
 * 3 automatic shutdown. Duplicate times preserve discontinuities/order. */
typedef struct { double elapsed_s, volume_bbl; uint32_t kind; } bweog_point;
typedef struct {
    uint32_t facility, first_point, point_count, shut_in;
    double shutdown_s, lost_revenue, produced_bbl, trucked_bbl;
    double final_bbl, balance_residual_bbl;
} bweog_facility_result;
typedef struct {
    uint32_t status, automatic_shutdown;
    double truck_cost, lost_revenue, total_cost;
    const bweog_facility_result* facilities;
    const bweog_point* points;
    const bweog_violation* violations;
    uint32_t facility_count, point_count, violation_count;
} bweog_result_view;
typedef struct bweog_result bweog_result;

BWEOG_API uint32_t bweog_abi_version(void);
BWEOG_API const char* bweog_method(void);
BWEOG_API const char* bweog_last_error(void);
/* Layout IDs 1..13 follow the typedef order: fleet, well, facility, snapshot,
 * horizon, stop, trip, job, plan, violation, point, facility_result, result_view.
 * ID 14 is reserved (zero). Used by the bridge before any output write. */
BWEOG_API uint32_t bweog_layout_size(uint32_t layout_id);
/* Return 0: structured result, including invalid/unavailable. Return 1: invalid
 * computational input/ABI; return 2: allocation/internal failure. *out is null
 * on a nonzero return. No exception crosses this C boundary. */
BWEOG_API int bweog_evaluate(const bweog_snapshot*, const bweog_plan*, bweog_result** out);
BWEOG_API int bweog_get_result(const bweog_result*, bweog_result_view* out);
BWEOG_API void bweog_destroy(bweog_result*);

/* Additive private committed-state extension. V1 layouts/entry points above are
 * unchanged. The initial snapshot is AFTER all events at decision time zero.
 * Busy slots reserve [0, available_s); pending pickups are strictly after zero.
 * Previously stopped eligible wells lose future revenue through repair. Costs
 * cover future committed/new pickups and future lost revenue, never sunk costs.
 * Caller authenticates/derives this initial state; this ABI grants no authority. */
typedef struct { uint32_t system, truck; double available_s; } bweog_occupied;
typedef struct { uint32_t facility, system, truck; double arrival_s; } bweog_pending;
typedef struct {
    uint32_t abi_version, struct_size;
    const bweog_occupied* occupied;
    const bweog_pending* pickups;
    const uint32_t* stopped_wells;
    uint32_t occupied_count, pickup_count, stopped_count;
} bweog_commitments;
BWEOG_API const char* bweog_committed_method(void);
/* Separate extension layouts: 1 occupied, 2 pending, 3 commitments. */
BWEOG_API uint32_t bweog_committed_layout_size(uint32_t layout_id);
BWEOG_API int bweog_evaluate_committed(const bweog_snapshot*, const bweog_plan*,
                                      const bweog_commitments*, bweog_result** out);

#ifdef __cplusplus
}
#endif
#endif

#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

// Engine-owned analytical types, not the shared EOG capture/worker contract.
// No file loading, detection, diagnosis, optimization or independent validation.
namespace breakwater::eog {

inline constexpr const char* analytical_version = "eog-analytical-0.1.0";
inline constexpr std::size_t max_facilities = 128;
inline constexpr std::size_t max_wells_per_facility = 128;
inline constexpr std::size_t max_truck_jobs = 1024;
inline constexpr std::size_t max_total_trips = 16384;

double volume_bbl(double capacity_bbl, double level_pct);
double hourly_water_bbl(double water_bpd);
double hourly_revenue(double oil_bpd, double gas_mcfd,
                      double oil_per_bbl, double gas_per_mcf);
double repair_remaining_h(double inferred_onset_h, double cutoff_h, double duration_h);

enum class PumpState { stopped, running, fault };
PumpState hysteresis(PumpState previous, double level_pct,
                     double start_pct, double stop_pct, bool fault_active);

// Null means no future crossing under the supplied constant net rate; not "healthy".
// An already-reached threshold returns zero even if the net rate is nonpositive.
std::optional<double> countdown_h(double inventory_bbl, double shutdown_bbl,
                                double net_inflow_bblh);

struct Sample { double value = 0.0; double sampled_at_h = 0.0; };
struct ObservationFixture {
    std::optional<Sample> level_pct;
    std::optional<Sample> inflow_bpd;
    std::optional<Sample> pump_flow_bpd;
    double cutoff_h = 0.0;
    double maximum_age_h = 0.0; // Caller declares the EOG policy; no G1 defaults.
    double maximum_skew_h = 0.0;
};
enum class CountdownStatus { available, no_crossing, already_reached, unavailable };
struct CountdownEstimate {
    CountdownStatus status = CountdownStatus::unavailable;
    std::optional<double> hours;
    std::string reason;
};
// Balance arithmetic for a declared pump-failure context, not a fault detector.
// Missing, future, stale, incoherent or unsupported observations abstain.
CountdownEstimate observed_countdown_fixture(double capacity_bbl, double shutdown_pct,
                                             const ObservationFixture& observations);

struct Prices { double oil_per_bbl = 0.0; double gas_per_mcf = 0.0; };
struct Well {
    std::string id;
    double water_bpd = 0.0; // Declared constant prospective water rate.
    double oil_bpd = 0.0;
    double gas_mcfd = 0.0;
    // Productive before the incident, including wells now automatically shut in.
    // This is an explicit assumption, not a mapping of the latest observed status.
    bool productive = true;
};
struct FailedFacility {
    std::string id;
    std::string system_id;
    double capacity_bbl = 0.0;
    double initial_level_pct = 0.0;
    double shutdown_pct = 0.0;
    double remaining_repair_h = 0.0;
    double response_min = 0.0;
    double cycle_min = 0.0;
    bool already_shut_in = false;
    std::vector<Well> wells;
};
struct Fleet { std::string system_id; std::size_t size = 0; };
struct FailureFixture {
    Prices prices;
    double truck_capacity_bbl = 0.0;
    double cost_per_trip = 0.0;
    std::vector<Fleet> fleets;
    std::vector<FailedFacility> facilities;
};
struct WellShutIn {
    std::string facility_id;
    std::string well_id;
    double start_after_h = 0.0;
};
struct TruckJob {
    std::string facility_id;
    std::size_t truck_index = 0; // Local to the facility's system fleet.
    double call_after_h = 0.0;
    std::size_t trips = 1;
};
struct Response { std::vector<WellShutIn> shut_ins; std::vector<TruckJob> truck_jobs; };

struct Violation {
    std::string code;
    std::string facility_id;
    std::optional<double> time_h;
};
struct TruckCalendar {
    std::string facility_id;
    std::string system_id;
    std::size_t truck_index = 0;
    double call_after_h = 0.0;
    std::vector<double> arrivals_h;
    double free_after_h = 0.0; // Last arrival + cycle, even beyond repair.
};
struct Point {
    double time_h = 0.0;
    double inventory_bbl = 0.0;
    // Repeated times preserve both sides of a discrete removal.
    std::string phase;
};
struct FacilityOutcome {
    std::string facility_id;
    std::vector<Point> trajectory;
    std::optional<double> shutdown_after_h;
    double lost_revenue = 0.0;
    bool complete = false;
};
struct Assessment {
    std::vector<Violation> violations;
    std::vector<TruckCalendar> calendars;
    std::vector<FacilityOutcome> facilities;
    std::optional<double> truck_cost;
    std::optional<double> lost_revenue;
    std::optional<double> total_cost; // Absent for invalid/incomplete responses.
    bool valid() const { return violations.empty() && total_cost.has_value(); }
    bool avoids_shutdown() const;
};

// Pure E1 analytical fixture model: every supplied facility has a failed pump
// until its own remaining repair deadline. No trajectories after repair.
// Malformed/unsupported fixture inputs throw invalid_argument. Invalid actions
// return violations with no total cost; a partial trace is explicitly incomplete.
// Automatic shut-in is a legitimate do-nothing outcome, not an invalid action.
Assessment assess_failure_fixture(const FailureFixture& fixture, const Response& response = {});

} // namespace breakwater::eog

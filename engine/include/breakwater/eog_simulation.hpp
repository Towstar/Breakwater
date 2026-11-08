#pragma once

// Private, additive native synthetic-plant API. No Qt/Python/JSON dependency.
// Indexed assets are mapped to imported opaque IDs by the caller. Units are
// elapsed seconds, bbl, bpd, mcfd and synthetic currency. Future forcing and
// incidents belong to the plant, never to the observation/planning snapshot.
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace breakwater::eog::synthetic {
inline constexpr const char* method = "eog-synthetic-events-0.1.0";
struct Well {
    double water_bpd{}, oil_bpd{}, gas_mcfd{};
    bool initially_productive{true};
};
struct Facility {
    std::size_t system{}, first_well{}, well_count{};
    double capacity_bbl{}, initial_bbl{}, start_bbl{}, stop_bbl{}, shutdown_bbl{};
    double pump_bpd{}, pipeline_capacity_bpd{}, response_min{}, cycle_min{};
    bool initially_running{};
};
struct Failure { std::size_t facility{}; double start_s{}, repair_s{}; };
struct RateChange {
    std::size_t well{};
    double elapsed_s{}, water_bpd{}, oil_bpd{}, gas_mcfd{};
};
struct Scenario {
    double duration_s{}, oil_per_bbl{}, gas_per_mcf{}, truck_capacity_bbl{}, cost_per_trip{};
    std::vector<std::size_t> fleet_sizes;
    std::vector<Facility> facilities;
    std::vector<Well> wells;
    std::vector<Failure> failures;
    std::vector<RateChange> rate_changes;
    std::size_t point_limit{262144}, event_limit{1000000}, trip_limit{16384};
};
enum class Status { running, completed, invalid_action, unavailable, cancelled, limit };
enum class Kind { initial, sample, pump_start, pump_stop, failure, repair,
                  automatic_shut_in, before_pickup, after_pickup, dispatch,
                  truck_return, well_stop, rate_change };
struct Point {
    double elapsed_s{};
    std::size_t facility{};
    Kind kind{};
    double volume_bbl{};
    int pump_status{}; // 0 off, 1 running, 2 fault
    bool automatic_shut_in{};
};
struct FacilityView {
    double volume_bbl{}, inflow_bpd{}, pump_flow_bpd{};
    int pump_status{};
    bool automatic_shut_in{};
    double produced_bbl{}, pumped_bbl{}, trucked_bbl{}, lost_revenue{}, balance_residual_bbl{};
    // Signed change from materialized affine volume to the algebraically known
    // event boundary, caused by rounded event timestamps. Never a physical flow.
    double event_boundary_rounding_bbl{};
};
struct WellView {
    bool producing{};
    double water_bpd{}, oil_bpd{}, gas_mcfd{}, lost_revenue{};
};
struct TruckView { std::size_t system{}, truck{}; bool busy{}; double available_again_s{}; };
struct Snapshot {
    double elapsed_s{};
    Status status{Status::running};
    std::string diagnostic;
    std::vector<FacilityView> facilities;
    std::vector<WellView> wells;
    std::vector<TruckView> trucks;
    std::size_t completed_full_loads{};
    // Only running/completed supported prefixes have reportable aggregate costs.
    std::optional<double> truck_cost, lost_revenue, total_cost;
};
struct ActionResult { bool accepted{}; std::string diagnostic; };

class Simulator {
public:
    explicit Simulator(const Scenario&);
    ~Simulator();
    Simulator(Simulator&&) noexcept;
    Simulator& operator=(Simulator&&) noexcept;
    Simulator(const Simulator&) = delete;
    Simulator& operator=(const Simulator&) = delete;
    // Calls are monotone and bounded by Scenario.duration_s. Observation times
    // do not reset physical affine phases. Cancellation leaves no completed claim.
    bool advance(double elapsed_s, const std::function<bool()>& cancelled = {});
    ActionResult dispatch(std::size_t facility, std::size_t truck, std::size_t trips);
    ActionResult shut_in(std::size_t well);
    Snapshot snapshot() const;
    const std::vector<Point>& trace() const;
    void reset();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace breakwater::eog::synthetic

#pragma once
#include "observations.hpp"
#include <deque>
#include <map>

namespace breakwater::observations {
inline constexpr const char* detector_version="bounded-volume-monitor-0.1.0";
struct DetectionPolicy {
    double window_s=30, maximum_gap_s=5, clock_bound_s=0.25;
    double command_tracking_tolerance_m3h=0.1;
    int activate_count=3, clear_count=5;
    // Required for physical balance residuals. At most one physical delivery
    // transition between adjacent samples; no hidden excursions. Unknown external
    // publishers cannot silently inherit the bundled plant's temporal assumption.
    bool piecewise_constant_delivery=false;
};
struct DetectionUpdate {
    struct BalanceStep {
        double time_s=0;
        std::string snapshot_id,tank_id;
        BoundedValue inventory,net_rate;
        double timing_volume_bound=0;
        std::map<std::string,BoundedValue> delivery_channels;
    };
    std::vector<telemetry::DetectionEvent> changed_events;
    std::vector<telemetry::DetectionEvent> events;
    // Bounded pre-assimilation points, supplied only when an event changes.
    // Together with the registered configuration and original sample references,
    // these permit independent residual/bound reconstruction.
    std::vector<BalanceStep> witness;
    std::string balance_status="unavailable";
    double observed_window_s=0;
};
class ObservationDetector {
public:
    explicit ObservationDetector(std::string eventNamespace,DetectionPolicy policy={});
    DetectionUpdate observe(const MappingResult&);
    std::optional<telemetry::DetectionEvent> acknowledge(const std::string& eventId,const telemetry::EventAcknowledgement&);
    void reset();
private:
    struct TankPoint { BoundedValue inventory,rate; double timing_volume_bound=0; std::map<std::string,BoundedValue> channels; };
    struct Point { double time_s=0; std::string snapshot_id; std::map<std::string,TankPoint> tanks; };
    struct State {
        telemetry::DetectionEvent event;
        int above=0,below=0;
        double first_above_s=0;
        bool active=false;
        std::string last_samples;
    };
    std::string namespace_,context_,last_samples_;
    DetectionPolicy policy_;
    std::uint64_t next_id_=0;
    std::deque<Point> window_;
    std::map<std::string,State> states_;
    void condition(const std::string& key,const std::string& detector,const std::string& attachment,
                   const MappingResult&,double value,double bound,const std::string& metric,const std::string& unit,
                   const std::string& explanation,const std::string& sampleSignature,std::vector<telemetry::DetectionEvent>&);
};
}

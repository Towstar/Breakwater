#include "breakwater/observation_detection.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <stdexcept>

namespace breakwater::observations {
namespace {
constexpr std::size_t maximum_window_points=4096,maximum_window_scalars=32768;
bool valid(double x) { return std::isfinite(x) && x>=0; }
std::string identity(const telemetry::ObservedSnapshot& s) {
    return s.physical_hash+"\n"+s.configuration_hash+"\n"+s.stream_id+"\n"+s.epoch_id+"\n"+s.mode+"\n"+std::to_string(s.numerical_revision);
}
std::string sample_identity(const telemetry::ObservedSnapshot& s,const std::vector<std::string>& ids={}) {
    std::map<std::string,std::uint64_t> ordered;
    for (const auto& sample:s.samples)
        if (ids.empty() || std::find(ids.begin(),ids.end(),sample.sensor_id)!=ids.end()) ordered[sample.sensor_id]=sample.frame_sequence;
    std::string out;
    for (const auto& [id,sequence]:ordered) out+=id+":"+std::to_string(sequence)+"\n";
    return out;
}
const MappedValue* value(const MappingResult& m,const std::string& quantity,const std::string& id) {
    const auto found=std::find_if(m.values.begin(),m.values.end(),[&](const auto& v) { return v.quantity==quantity && v.attachment_id==id; });
    if (found==m.values.end()) return nullptr;
    for (const auto& sensor:found->sensor_ids)
        if (std::find(m.snapshot.coverage.conflicting_sensor_ids.begin(),m.snapshot.coverage.conflicting_sensor_ids.end(),sensor)!=m.snapshot.coverage.conflicting_sensor_ids.end()) return nullptr;
    return &*found;
}
double age(const MappingResult& m,const MappedValue& v) {
    double oldest=m.snapshot.captured_event_time_s;
    for (const auto& id:v.sensor_ids) for (const auto& sample:m.snapshot.samples) if (sample.sensor_id==id) oldest=std::min(oldest,sample.acquisition_start_s);
    return m.snapshot.captured_event_time_s-oldest;
}
}
ObservationDetector::ObservationDetector(std::string eventNamespace,DetectionPolicy policy):namespace_(std::move(eventNamespace)),policy_(policy) {
    const bool name=std::all_of(namespace_.begin(),namespace_.end(),[](char c) {
        return (c>='a' && c<='z') || (c>='A' && c<='Z') || (c>='0' && c<='9') || c=='-' || c=='_' || c=='.' || c==':';
    });
    if (namespace_.empty() || namespace_.size()>80 || !name || !valid(policy.window_s) || policy.window_s<=0 || policy.window_s>300 ||
        !valid(policy.maximum_gap_s) || policy.maximum_gap_s<=0 || policy.maximum_gap_s>30 || !valid(policy.clock_bound_s) ||
        !valid(policy.command_tracking_tolerance_m3h) || policy.activate_count<1 || policy.activate_count>100 || policy.clear_count<1 || policy.clear_count>100)
        throw std::invalid_argument("Invalid bounded observation detector policy.");
}
void ObservationDetector::reset() { context_.clear(); last_samples_.clear(); window_.clear(); states_.clear(); }
void ObservationDetector::condition(const std::string& key,const std::string& detector,const std::string& attachment,
    const MappingResult& mapped,double metricValue,double bound,const std::string& metric,const std::string& unit,
    const std::string& explanation,const std::string& signature,std::vector<telemetry::DetectionEvent>& updates) {
    if (!std::isfinite(metricValue) || !valid(bound)) return;
    auto& state=states_[key]; const auto& s=mapped.snapshot;
    if (state.last_samples==signature) return;
    state.last_samples=signature;
    const bool exceeds=std::abs(metricValue)>bound;
    const bool clears=std::abs(metricValue)<=0.5*bound;
    if (exceeds) {
        if (state.above==0) state.first_above_s=s.captured_event_time_s;
        state.above=std::min(policy_.activate_count,state.above+1); state.below=0;
    } else {
        state.above=0;
        state.below=clears ? std::min(policy_.clear_count,state.below+1):0;
    }
    if (!state.active && state.above<policy_.activate_count) return;
    if (!state.active) {
        state.event={}; state.event.event_id=namespace_+"-"+std::to_string(++next_id_);
        state.event.onset_event_time_s=state.first_above_s; state.event.revision=0; state.active=true;
    }
    auto& event=state.event;
    event.physical_hash=s.physical_hash; event.configuration_hash=s.configuration_hash;
    event.stream_id=s.stream_id; event.epoch_id=s.epoch_id; event.snapshot_id=s.snapshot_id;
    event.detector_id=detector; event.detector_version=detector_version; event.threshold_version="absolute-bounds-0.1.0";
    event.updated_event_time_s=s.captured_event_time_s; ++event.revision;
    event.status="active"; event.severity="warning"; event.asset_ids={attachment};
    event.sensor_ids=s.coverage.required_sensor_ids;
    event.evidence={{metric,unit,metricValue,bound}};
    event.diagnosis="unresolved"; event.explanation=explanation; event.actionable=false;
    if (state.below>=policy_.clear_count) {
        event.status="cleared"; event.cleared_event_time_s=s.captured_event_time_s; state.active=false;
        state.above=state.below=0;
    }
    updates.push_back(event);
}
DetectionUpdate ObservationDetector::observe(const MappingResult& mapped) {
    DetectionUpdate output;
    const auto& snapshot=mapped.snapshot;
    if (!valid(snapshot.captured_event_time_s)) { output.balance_status="invalid_clock"; return output; }
    const auto context=identity(snapshot);
    if (context!=context_) { reset(); context_=context; }
    const auto samples=sample_identity(snapshot);
    const bool distinct=samples!=last_samples_;
    std::set<std::string> observedPumps;
    const bool sourceUnavailable=std::any_of(mapped.issues.begin(),mapped.issues.end(),[](const auto& issue){return issue.code=="source_unavailable";});
    if (!sourceUnavailable) {
        // Direct delivery symptoms may remain observable even if a different tank
        // is missing. They cannot establish complete forecast coverage or a cause.
        for (const auto& state:mapped.values) if (state.quantity=="pump_state") {
            const auto* feedback=value(mapped,"pump_state",state.attachment_id);
            const auto* flow=value(mapped,"route_flow",state.attachment_id);
            const auto* label=feedback ? std::get_if<std::string>(&feedback->value):nullptr;
            if (!label || !flow) continue;
            const auto command=std::find_if(snapshot.commands.begin(),snapshot.commands.end(),[&](const auto& c) {
                return c.id=="route:"+state.attachment_id+":flow";
            });
            if (command==snapshot.commands.end()) continue;
            const auto* delivered=std::get_if<double>(&flow->value);
            if (!delivered) continue;
            const double error=command->value-*delivered;
            const auto flowBound=flow->bound+policy_.command_tracking_tolerance_m3h;
            const bool stateOnly=command->value>flowBound && std::abs(error)<=flowBound && (*label=="delivery_loss" || *label=="unavailable");
            auto ids=flow->sensor_ids; ids.insert(ids.end(),feedback->sensor_ids.begin(),feedback->sensor_ids.end());
            observedPumps.insert("pump:"+state.attachment_id);
            condition("pump:"+state.attachment_id,"pump-delivery",state.attachment_id,mapped,stateOnly ? 1.0:error,
                stateOnly ? 0.5:flowBound,stateOnly ? "reported_availability_restricted":"delivery_error",stateOnly ? "state":"m3/h",
                "Measured delivery or reported availability differs from the frozen command. Sensor error, operating state and delivery loss remain possible; no mechanical root cause is established.",
                sample_identity(snapshot,ids),output.changed_events);
        }
    }
    for (auto& [key,state]:states_) if (key.starts_with("pump:") && !observedPumps.contains(key)) state.above=state.below=0;
    if (distinct) last_samples_=samples;
    if (!policy_.piecewise_constant_delivery) output.balance_status="temporal_model_unavailable";
    else if (!mapped.effective_network || snapshot.availability!="available") {
        window_.clear(); output.balance_status="coverage_unavailable";
    } else if (!distinct) output.balance_status=window_.size()>1 ? "unchanged_samples":"warming_up";
    else {
        const auto& network=*mapped.effective_network;
        Point point; point.time_s=snapshot.captured_event_time_s; point.snapshot_id=snapshot.snapshot_id;
        bool usable=true;
        for (const auto& asset:network.assets) if (asset.kind==AssetKind::tank) {
            const auto* inventory=value(mapped,"inventory",asset.id);
            if (!inventory || !std::holds_alternative<double>(inventory->value)) { usable=false; break; }
            auto& tank=point.tanks[asset.id]; tank.inventory={std::get<double>(inventory->value),inventory->bound};
            double rateCapacity=0,oldest=age(mapped,*inventory);
            for (const auto& route:network.routes) {
                if (route.source_id!=asset.id && route.target_id!=asset.id) continue;
                const auto source=std::find_if(network.assets.begin(),network.assets.end(),[&](const auto& a){return a.id==route.source_id;});
                const auto* reading=source!=network.assets.end() && source->kind==AssetKind::source ?
                    value(mapped,"source_flow",source->id):value(mapped,"route_flow",route.id);
                if (!reading || !std::holds_alternative<double>(reading->value)) { usable=false; break; }
                tank.rate.value+=(route.target_id==asset.id ? 1.0:-1.0)*std::get<double>(reading->value);
                tank.rate.bound+=reading->bound; rateCapacity+=route.capacity_m3h;
                tank.channels[route.id]={std::get<double>(reading->value),reading->bound};
                oldest=std::max(oldest,age(mapped,*reading));
            }
            tank.timing_volume_bound=rateCapacity*(oldest+policy_.clock_bound_s)/3600;
            usable=usable && valid(tank.inventory.bound) && std::isfinite(tank.rate.value) && valid(tank.rate.bound) && valid(tank.timing_volume_bound);
        }
        if (!usable || point.tanks.empty()) { window_.clear(); output.balance_status="mapping_unavailable"; }
        else {
            if (!window_.empty() && (point.time_s<=window_.back().time_s || point.time_s-window_.back().time_s>policy_.maximum_gap_s)) window_.clear();
            window_.push_back(std::move(point));
            const auto cutoff=snapshot.captured_event_time_s-policy_.window_s;
            while (window_.size()>2 && window_[1].time_s<=cutoff) window_.pop_front();
            std::size_t scalars=0;
            for (const auto& [id,tank]:window_.back().tanks) scalars+=4+2*tank.channels.size();
            while (window_.size()>maximum_window_points || window_.size()*scalars>maximum_window_scalars) window_.pop_front();
            output.observed_window_s=window_.back().time_s-window_.front().time_s;
            output.balance_status=output.observed_window_s<policy_.window_s ? "warming_up":"available";
            if (output.balance_status=="available") for (const auto& [tankId,last]:window_.back().tanks) {
                const auto& first=window_.front().tanks.at(tankId);
                double integrated=0,flowBound=0,quadrature=0,largest=0;
                for (std::size_t i=1;i<window_.size();++i) {
                    const auto& before=window_[i-1].tanks.at(tankId); const auto& after=window_[i].tanks.at(tankId);
                    const auto dt=window_[i].time_s-window_[i-1].time_s;
                    largest=std::max(largest,std::abs(before.inventory.value));
                    integrated+=before.rate.value*dt/3600;
                    flowBound+=std::max(before.rate.bound,after.rate.bound)*dt/3600;
                    // Under the declared at-most-one-transition hold model, the
                    // unknown transition location contributes at most |dq|*dt.
                    for (const auto& [route,reading]:before.channels)
                        quadrature+=std::abs(after.channels.at(route).value-reading.value)*dt/3600;
                }
                const auto residual=last.inventory.value-first.inventory.value-integrated;
                const auto roundoff=64*std::numeric_limits<double>::epsilon()*static_cast<double>(window_.size())*
                    std::max({1.0,largest,std::abs(last.inventory.value),std::abs(integrated)});
                const auto bound=first.inventory.bound+last.inventory.bound+flowBound+quadrature+
                    first.timing_volume_bound+last.timing_volume_bound+roundoff;
                if (!std::isfinite(residual) || !valid(bound)) { output.balance_status="arithmetic_unavailable"; continue; }
                const auto* inventory=value(mapped,"inventory",tankId);
                condition("balance:"+tankId,"volume-balance",tankId,mapped,residual,bound,"signed_inventory_residual","m3",
                    "Pre-assimilation inventory change disagrees with integrated measured delivery beyond declared absolute measurement, calibration, timing, quadrature and roundoff bounds. Unmetered flow, sensor bias/drift and model mismatch remain unresolved alternatives.",
                    inventory ? sample_identity(snapshot,inventory->sensor_ids):std::string{},output.changed_events);
            }
        }
    }
    if (output.balance_status!="available" && output.balance_status!="unchanged_samples")
        for (auto& [key,state]:states_) if (key.starts_with("balance:")) state.above=state.below=0;
    for (const auto& [key,state]:states_) if (!state.event.event_id.empty()) output.events.push_back(state.event);
    if (!output.changed_events.empty()) for (const auto& point:window_) for (const auto& [id,tank]:point.tanks)
        output.witness.push_back({point.time_s,point.snapshot_id,id,tank.inventory,tank.rate,tank.timing_volume_bound,tank.channels});
    return output;
}
std::optional<telemetry::DetectionEvent> ObservationDetector::acknowledge(const std::string& eventId,const telemetry::EventAcknowledgement& acknowledgement) {
    if (acknowledgement.actor.empty() || acknowledgement.acknowledged_utc.empty()) return {};
    for (auto& [key,state]:states_) if (state.event.event_id==eventId) {
        state.event.acknowledgement=acknowledgement; ++state.event.revision;
        return state.event;
    }
    return {};
}
}

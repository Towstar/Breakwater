#include "breakwater/observation_detection.hpp"
#include "breakwater/engine.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <random>
#include <stdexcept>

using namespace breakwater;
namespace dto=breakwater::telemetry;
using namespace breakwater::observations;
namespace {
void require(bool value,const char* text) { if (!value) throw std::runtime_error(text); }
struct Fixture {
    Network design{{{"tank","Tank",AssetKind::tank,200,80}, {"well","Well",AssetKind::source,0,0,36,0,36},
                    {"destination","Destination",AssetKind::treatment,100}},
                   {{"feed","well","tank",100,36,false,true},{"pump","tank","destination",100,12,true,true}}};
    dto::SensorConfiguration configuration;
    Fixture() {
        configuration.physical_hash="design";
        add("v","inventory","tank","m3"); add("s","source_flow","well","m3/h");
        add("q","route_flow","pump","m3/h"); add("p","pump_state","pump","state");
    }
    void add(std::string id,std::string quantity,std::string attachment,std::string unit) {
        dto::Sensor sensor; sensor.sensor_id=std::move(id); sensor.quantity=std::move(quantity); sensor.unit=std::move(unit);
        sensor.attachment.id=std::move(attachment); sensor.uncertainty_bound=0.0001;
        configuration.sensors.push_back(std::move(sensor));
    }
    dto::ObservedSnapshot raw(double t,double volume,double delivery=12,const std::string& state="running") const {
        dto::ObservedSnapshot snapshot;
        snapshot.snapshot_id="capture-"+std::to_string(t); snapshot.physical_hash="design"; snapshot.configuration_hash="configuration";
        snapshot.stream_id="stream"; snapshot.epoch_id="epoch"; snapshot.mode="synthetic_live";
        snapshot.captured_event_time_s=t; snapshot.commands={{"source:well:curtail",0},{"route:pump:flow",12}};
        const dto::Value values[]={volume,36.0,delivery,state};
        for (std::size_t i=0;i<4;++i) {
            dto::Sample sample; sample.sensor_id=configuration.sensors[i].sensor_id;
            sample.frame_sequence=static_cast<std::uint64_t>(t)+1;
            sample.event_time_s=sample.acquisition_start_s=sample.acquisition_end_s=t;
            sample.canonical_value=sample.original_value=values[i];
            sample.canonical_unit=sample.original_unit=configuration.sensors[i].unit;
            sample.reported_quality=sample.receiver_status="good"; snapshot.samples.push_back(sample);
        }
        return snapshot;
    }
    MappingResult at(double t,double volume,double delivery=12,const std::string& state="running") const {
        return map_volume_state(design,configuration,raw(t,volume,delivery,state));
    }
};
DetectionPolicy policy() { DetectionPolicy result; result.piecewise_constant_delivery=true; return result; }
void bounded_noise_has_no_physical_events() {
    Fixture f; ObservationDetector detector("healthy",policy());
    std::mt19937 random(7); std::uniform_real_distribution<double> noise(-0.0001,0.0001);
    for (int second=0;second<=3600;++second) {
        auto raw=f.raw(second,80+24.0*second/3600+noise(random));
        raw.samples[1].canonical_value=raw.samples[1].original_value=36+noise(random);
        raw.samples[2].canonical_value=raw.samples[2].original_value=12+noise(random);
        const auto result=detector.observe(map_volume_state(f.design,f.configuration,raw));
        require(result.events.empty(),"Bounded independent instrument noise produced a physical alarm");
        if (second>=30) require(result.balance_status=="available","Declared healthy balance window unavailable");
    }
}
void physical_onset_detection_clear_and_ack_are_separate() {
    Fixture f; ObservationDetector detector("residual",policy());
    double activated=-1,cleared=-1; std::string eventId;
    for (int second=0;second<=240;++second) {
        const auto duration=std::clamp(double(second)-60,0.0,120.0);
        const auto result=detector.observe(f.at(second,80+24.0*second/3600-18*duration/3600));
        for (const auto& event:result.changed_events) if (event.detector_id=="volume-balance") {
            require(!event.actionable && event.diagnosis=="unresolved","Residual claimed authority or root cause");
            if (event.status=="active" && activated<0) {
                activated=second; eventId=event.event_id;
                const auto ack=detector.acknowledge(eventId,{"tester","2026-11-18T00:00:00Z"});
                require(ack && ack->status=="active" && ack->acknowledgement.has_value(),"Acknowledgement cleared the event");
            }
            if (event.status=="cleared") {
                cleared=second; require(event.event_id==eventId && event.acknowledgement.has_value(),"Clear lost stable identity or acknowledgement");
            }
        }
    }
    require(activated>=60 && activated<=120,"Observable residual fault missed the declared sixty-second target");
    require(cleared>=210 && cleared<=240,"Five healthy windows did not clear the retained residual event");
}
void debounce_counts_actual_pump_samples_and_missing_feedback_breaks_streak() {
    Fixture f; f.add("aux","temperature","tank","degC"); ObservationDetector detector("pump",policy());
    auto first=f.raw(1,80.01,0,"delivery_loss");
    require(detector.observe(map_volume_state(f.design,f.configuration,first)).events.empty(),"Pump alarm did not debounce");
    for (int i=1;i<50;++i) {
        auto repeated=first; repeated.captured_event_time_s=1+i/100.0;
        dto::Sample auxiliary; auxiliary.sensor_id="aux"; auxiliary.frame_sequence=static_cast<std::uint64_t>(i);
        auxiliary.event_time_s=auxiliary.acquisition_start_s=auxiliary.acquisition_end_s=1;
        auxiliary.original_value=auxiliary.canonical_value=25.0; auxiliary.original_unit=auxiliary.canonical_unit="degC";
        auxiliary.receiver_status=auxiliary.reported_quality="good"; repeated.samples.push_back(auxiliary);
        require(detector.observe(map_volume_state(f.design,f.configuration,repeated)).events.empty(),"Unrelated sample updates fabricated a pump debounce streak");
    }
    require(detector.observe(f.at(2,80.02,0,"delivery_loss")).events.empty(),"Two pump samples triggered a three-sample detector");
    auto missing=f.raw(3,80.03,0,"delivery_loss"); missing.samples.pop_back();
    detector.observe(map_volume_state(f.design,f.configuration,missing));
    require(detector.observe(f.at(4,80.04,0,"delivery_loss")).events.empty(),"Missing feedback did not break the pending streak");
    require(detector.observe(f.at(5,80.05,0,"delivery_loss")).events.empty(),"Two post-gap samples triggered activation");
    auto active=detector.observe(f.at(6,80.06,0,"delivery_loss")); require(active.events.size()==1 && active.events[0].status=="active","Third fresh pump sample did not activate");
    const auto eventId=active.events[0].event_id;
    for (int second=7;second<=11;++second) active=detector.observe(f.at(second,80.06,12,"running"));
    require(active.events[0].status=="cleared" && active.events[0].event_id==eventId,"Five recovery samples did not clear pump symptom");
}
void channel_variation_bound_does_not_cancel_opposing_transitions() {
    Fixture f; auto settings=policy(); settings.clock_bound_s=0; settings.activate_count=1;
    ObservationDetector detector("transitions",settings);
    for (int second=0;second<=40;++second) {
        // The source increases at 0.25 s; the outlet increases at 0.75 s.
        // Both sampled net rates are 24 m3/h, but the half-second excursion adds
        // 5/3600 m3. A bound formed from net-rate changes would miss that area.
        auto raw=f.raw(second,80+24.0*second/3600+(second ? 5.0/3600:0),second ? 22:12);
        if (second) raw.samples[1].canonical_value=raw.samples[1].original_value=46.0;
        const auto result=detector.observe(map_volume_state(f.design,f.configuration,raw));
        for (const auto& event:result.events) require(event.detector_id!="volume-balance","Opposing physical transitions produced an unbounded quadrature residual");
    }
}
void unknown_temporal_model_coverage_gaps_and_new_epochs_remain_explicit() {
    Fixture f; ObservationDetector unknown("unknown");
    for (int t=0;t<90;++t) {
        const auto result=unknown.observe(f.at(t,80.0));
        require(result.balance_status=="temporal_model_unavailable" && result.events.empty(),"Unknown timing model silently acquired residual authority");
    }
    ObservationDetector known("known",policy());
    for (int t=0;t<35;++t) known.observe(f.at(t,80+24.0*t/3600));
    auto missing=f.raw(35,80.3); missing.samples.erase(missing.samples.begin());
    require(known.observe(map_volume_state(f.design,f.configuration,missing)).balance_status=="coverage_unavailable","Missing inventory became zero");
    require(known.observe(f.at(36,80.3)).balance_status=="warming_up","Coverage gap retained a falsely coherent window");
    for (int t=37;t<40;++t) known.observe(f.at(t,80.3,0,"delivery_loss"));
    auto changed=f.raw(40,80.3); changed.epoch_id="new-epoch";
    const auto result=known.observe(map_volume_state(f.design,f.configuration,changed));
    require(result.events.empty() && result.balance_status=="warming_up","New epoch inherited old event eligibility");
}
void held_out_two_topology_hour_qualification() {
    std::uint64_t physicalEvents=0,coverageFailures=0,windows=0;
    for (int topology=0;topology<2;++topology) for (int seed=1001;seed<=1010;++seed) {
        Fixture f;
        if (topology) {
            f.design={{{"north","North",AssetKind::tank,200,80},{"south","South",AssetKind::tank,180,90},
                {"west","West",AssetKind::source,0,0,42,0,42},{"out","Outlet",AssetKind::treatment,100}},
                {{"feed-west","west","north",100,42,false,true},{"transfer","north","south",100,18,true,true},
                 {"north-out","north","out",100,8,false,true},{"south-out","south","out",100,14,true,true}}};
        }
        f.configuration.sensors.clear();
        for (const auto& a:f.design.assets) {
            if (a.kind==AssetKind::tank) f.add("inventory-"+a.id,"inventory",a.id,"m3");
            if (a.kind==AssetKind::source) f.add("production-"+a.id,"source_flow",a.id,"m3/h");
        }
        for (const auto& r:f.design.routes) {
            const auto a=std::find_if(f.design.assets.begin(),f.design.assets.end(),[&](const auto& asset){return asset.id==r.source_id;});
            if (a->kind==AssetKind::tank) { f.add("delivery-"+r.id,"route_flow",r.id,"m3/h"); if (r.has_pump) f.add("feedback-"+r.id,"pump_state",r.id,"state"); }
        }
        std::mt19937 random(static_cast<unsigned>(seed)); std::uniform_real_distribution<double> noise(-0.0001,0.0001);
        ObservationDetector detector("held-out-"+std::to_string(topology)+"-"+std::to_string(seed),policy());
        for (int t=0;t<=3600;++t) {
            dto::ObservedSnapshot s; s.snapshot_id="held-out-"+std::to_string(t); s.physical_hash="design"; s.configuration_hash="configuration";
            s.stream_id="acquisition"; s.epoch_id="seed-"+std::to_string(seed); s.mode="synthetic_live"; s.captured_event_time_s=t;
            for (const auto& control:control_catalog(f.design)) s.commands.push_back({control.id,control.value});
            for (const auto& sensor:f.configuration.sensors) {
                dto::Value reading;
                const auto asset=std::find_if(f.design.assets.begin(),f.design.assets.end(),[&](const auto& a){return a.id==sensor.attachment.id;});
                const auto route=std::find_if(f.design.routes.begin(),f.design.routes.end(),[&](const auto& r){return r.id==sensor.attachment.id;});
                if (sensor.quantity=="inventory") {
                    // Independent explicit affine reference, not a simulator output.
                    const auto rate=topology ? (asset->id=="north" ? 16.0:4.0):24.0;
                    reading=asset->initial_m3+rate*t/3600+noise(random);
                } else if (sensor.quantity=="source_flow") reading=asset->inflow_m3h+noise(random);
                else if (sensor.quantity=="route_flow") reading=route->setpoint_m3h+noise(random);
                else reading=std::string("running");
                dto::Sample sample; sample.sensor_id=sensor.sensor_id; sample.frame_sequence=static_cast<std::uint64_t>(t)+1;
                sample.event_time_s=sample.acquisition_start_s=sample.acquisition_end_s=t;
                sample.canonical_value=sample.original_value=reading; sample.canonical_unit=sample.original_unit=sensor.unit;
                sample.reported_quality=sample.receiver_status="good"; s.samples.push_back(sample);
            }
            const auto result=detector.observe(map_volume_state(f.design,f.configuration,s));
            if (t>=30 && result.balance_status!="available") ++coverageFailures;
            physicalEvents+=result.changed_events.size(); ++windows;
        }
    }
    std::cout<<"Held-out qualification: 2 topologies x 10 seeds x 1 healthy hour; "<<windows
        <<" captures, "<<physicalEvents<<" physical event updates, "<<coverageFailures<<" coverage failures\n";
    require(physicalEvents==0 && coverageFailures==0,"Held-out healthy qualification failed");
}
void planned_unavailable_zero_delivery_is_not_a_new_fault() {
    Fixture f; f.design.routes[1].available=false; f.design.routes[1].setpoint_m3h=0;
    ObservationDetector detector("planned-off",policy());
    for (int t=0;t<40;++t) {
        auto raw=f.raw(t,80+36.0*t/3600,0,"unavailable"); raw.commands[1].value=0;
        const auto result=detector.observe(map_volume_state(f.design,f.configuration,raw));
        require(result.events.empty(),"An intentionally unavailable zero-command route produced a new fault");
    }
}
}
int main() {
    try {
        bounded_noise_has_no_physical_events(); physical_onset_detection_clear_and_ack_are_separate();
        debounce_counts_actual_pump_samples_and_missing_feedback_breaks_streak();
        channel_variation_bound_does_not_cancel_opposing_transitions();
        unknown_temporal_model_coverage_gaps_and_new_epochs_remain_explicit();
        held_out_two_topology_hour_qualification();
        planned_unavailable_zero_delivery_is_not_a_new_fault();
        std::cout<<"7 observation detector suites passed\n"; return 0;
    } catch (const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}

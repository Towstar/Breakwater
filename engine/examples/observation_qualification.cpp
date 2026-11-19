#include "breakwater/observation_detection.hpp"
#include "breakwater/engine.hpp"
#include <algorithm>
#include <cmath>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <random>
#include <set>
#include <sstream>
#include <stdexcept>

// Independent affine/step reference and offline comparators. No native forecast,
// publisher preset/seed or hidden fault truth is supplied to the detector.
using namespace breakwater;
namespace dto=breakwater::telemetry;
using namespace breakwater::observations;
namespace {
constexpr double onset=60,recovery=180,window=30,clockBound=0.25;
constexpr const char* presets[]={"healthy","clock_skew","pump_loss","unmetered_loss","marginal_loss","small_loss","drift","stuck","missing","delayed"};
struct Sample { double value=0,bound=0,time=0,truth=0; std::uint64_t sequence=0; };
struct Point { double time=0; Sample inventory; std::map<std::string,Sample> channels; double timing=0; };
struct Row {
    int topology=0,seed=0,scale=1,samples=0,coverageFailures=0,coverageMismatches=0,falseAlarms=0,boundChecks=0,boundMisses=0;
    std::string preset;
    double observable=-1,activated=-1,cleared=-1,coverageOnset=-1,ewmaAlarm=-1,cusumAlarm=-1;
    std::size_t maximumPoints=0,maximumScalars=0;
    bool targetMet=true;
};
struct Comparator {
    double ewma=0,upper=0,lower=0;
    void reset() { ewma=upper=lower=0; }
    std::pair<bool,bool> update(double z) {
        ewma=0.2*z+0.8*ewma;
        upper=std::max(0.0,upper+z-0.5); lower=std::max(0.0,lower-z-0.5);
        return {std::abs(ewma)>1,upper>5 || lower>5};
    }
};
struct Reference {
    Network network; dto::SensorConfiguration configuration;
    std::string firstTank,pump;
    explicit Reference(int topology,int scale) {
        if (!topology) {
            network={{{"vessel","Vessel",AssetKind::tank,200,80},{"inlet","Inlet",AssetKind::source,0,0,36,0,36},
                {"outlet","Outlet",AssetKind::treatment,100}},{{"feed","inlet","vessel",100,36,false,true},{"delivery","vessel","outlet",100,12,true,true}}};
            firstTank="vessel"; pump="delivery";
        } else {
            network={{{"north","North",AssetKind::tank,200,80},{"south","South",AssetKind::tank,180,90},
                {"west","West",AssetKind::source,0,0,42,0,42},{"out","Outlet",AssetKind::treatment,100}},
                {{"feed-west","west","north",100,42,false,true},{"transfer","north","south",100,18,true,true},
                 {"north-out","north","out",100,8,false,true},{"south-out","south","out",100,14,true,true}}};
            firstTank="north"; pump="transfer";
        }
        if (!inspect_network(network).empty()) throw std::runtime_error("Invalid independent qualification network");
        configuration.physical_hash="qualification-design";
        const auto add=[&](const std::string& quantity,const std::string& id,const std::string& unit) {
            dto::Sensor s; s.sensor_id=quantity+":"+id; s.quantity=quantity; s.unit=unit;
            s.attachment={quantity=="route_flow" || quantity=="pump_state" ? "route":"asset",id};
            s.uncertainty_bound=0.0001*scale; s.calibration.error_bound=0.00002*scale;
            configuration.sensors.push_back(s);
        };
        for (const auto& a:network.assets) {
            if (a.kind==AssetKind::tank) add("inventory",a.id,"m3");
            if (a.kind==AssetKind::source) add("source_flow",a.id,"m3/h");
        }
        for (const auto& r:network.routes) if (asset(r.source_id).kind==AssetKind::tank) {
            add("route_flow",r.id,"m3/h"); if (r.has_pump) add("pump_state",r.id,"state");
        }
    }
    const Asset& asset(const std::string& id) const { return *std::find_if(network.assets.begin(),network.assets.end(),[&](const auto& a){return a.id==id;}); }
    const Route& route(const std::string& id) const { return *std::find_if(network.routes.begin(),network.routes.end(),[&](const auto& r){return r.id==id;}); }
    double baseRate(const std::string& tank) const {
        double rate=0; for (const auto& r:network.routes) rate+=(r.target_id==tank ? 1.0:r.source_id==tank ? -1.0:0.0)*r.setpoint_m3h;
        return rate;
    }
    double volume(const std::string& tank,double t,const std::string& preset) const {
        const auto duration=std::clamp(t-onset,0.0,recovery-onset);
        auto result=asset(tank).initial_m3+baseRate(tank)*t/3600;
        if (preset=="pump_loss") {
            const auto& p=route(pump); result+=(p.source_id==tank ? 1.0:p.target_id==tank ? -1.0:0.0)*p.setpoint_m3h*duration/3600;
        }
        if (tank==firstTank) {
            const double loss=preset=="unmetered_loss" ? 18:preset=="marginal_loss" ? 6:preset=="small_loss" ? 0.1:0;
            result-=loss*duration/3600;
        }
        return result;
    }
    double instrumentVolume(const std::string& tank,double t,const std::string& preset) const {
        if (tank==firstTank && preset=="stuck" && t>=onset && t<recovery) return volume(tank,onset,preset);
        return volume(tank,t,preset)+(tank==firstTank && preset=="drift" ? 0.002*std::clamp(t-onset,0.0,recovery-onset):0);
    }
    double flow(const Route& r,double t,const std::string& preset) const {
        return preset=="pump_loss" && r.id==pump && t>=onset && t<recovery ? 0:r.setpoint_m3h;
    }
};
Row execute(int topology,int seed,int scale,const std::string& preset) {
    Reference ref(topology,scale); Row row; row.topology=topology; row.seed=seed; row.scale=scale; row.preset=preset;
    const bool healthy=preset=="healthy" || preset=="clock_skew",quality=preset=="missing" || preset=="delayed",pump=preset=="pump_loss";
    const int horizon=healthy ? 3600:600;
    std::mt19937 random(static_cast<unsigned>(seed)); std::uniform_real_distribution<double> unit(-1,1);
    std::map<std::string,double> offsets; for (const auto& sensor:ref.configuration.sensors) offsets[sensor.sensor_id]=unit(random)*sensor.calibration.error_bound;
    std::map<std::string,Sample> retained; std::deque<Point> history; Comparator comparator;
    DetectionPolicy policy; policy.piecewise_constant_delivery=true; ObservationDetector detector("qualification",policy);
    std::map<std::string,std::string> statuses;
    for (int t=0;t<=horizon;++t) {
        dto::ObservedSnapshot snapshot; snapshot.snapshot_id="sample-"+std::to_string(t); snapshot.physical_hash="qualification-design";
        snapshot.configuration_hash="qualification-instruments"; snapshot.stream_id="independent-reference"; snapshot.epoch_id="one";
        snapshot.mode="synthetic_live"; snapshot.captured_event_time_s=t;
        for (const auto& c:control_catalog(ref.network)) snapshot.commands.push_back({c.id,c.value});
        for (const auto& sensor:ref.configuration.sensors) {
            const bool targetInventory=sensor.quantity=="inventory" && sensor.attachment.id==ref.firstTank;
            double at=t;
            if (targetInventory && t>=onset && t<recovery) {
                if (preset=="missing") at=onset-1;
                if (preset=="delayed") at=std::max(onset-1,double(t)-3);
            }
            if (targetInventory && preset=="clock_skew") at=std::max(0.0,t-0.2);
            dto::Sample value; value.sensor_id=sensor.sensor_id; value.canonical_unit=value.original_unit=sensor.unit;
            value.reported_quality=value.receiver_status="good";
            if (sensor.quantity=="pump_state") {
                const auto& route=ref.route(sensor.attachment.id);
                value.canonical_value=value.original_value=std::string(ref.flow(route,at,preset)==0 ? "delivery_loss":"running");
                value.frame_sequence=static_cast<std::uint64_t>(t)+1;
            } else {
                auto& sample=retained[sensor.sensor_id];
                if (t==0 || sample.time!=at) {
                    const double truth=sensor.quantity=="inventory" ? ref.instrumentVolume(sensor.attachment.id,at,preset):
                        sensor.quantity=="source_flow" ? ref.asset(sensor.attachment.id).inflow_m3h:ref.flow(ref.route(sensor.attachment.id),at,preset);
                    // The synthetic zero-flow instrument reports exact zero;
                    // receivers never clamp signed or out-of-range measurements.
                    const double measured=truth==0 && sensor.quantity!="inventory" ? 0:truth+offsets[sensor.sensor_id]+unit(random)*sensor.uncertainty_bound;
                    sample={measured,sensor.uncertainty_bound+sensor.calibration.error_bound,at,truth,static_cast<std::uint64_t>(t)+1};
                }
                value.canonical_value=value.original_value=sample.value; value.frame_sequence=sample.sequence;
                if (healthy) { ++row.boundChecks; if (std::abs(sample.value-sample.truth)>sample.bound+1e-12) ++row.boundMisses; }
            }
            value.event_time_s=value.acquisition_start_s=value.acquisition_end_s=at; snapshot.samples.push_back(value);
        }
        const auto mapped=map_volume_state(ref.network,ref.configuration,snapshot);
        const auto observed=detector.observe(mapped); ++row.samples;
        const bool expectedUnavailable=quality && t>=61 && t<recovery;
        if ((mapped.snapshot.availability!="available")!=expectedUnavailable) ++row.coverageMismatches;
        if (mapped.snapshot.availability!="available") { ++row.coverageFailures; if (row.coverageOnset<0) row.coverageOnset=t; }
        row.maximumPoints=std::max(row.maximumPoints,observed.window_point_count); row.maximumScalars=std::max(row.maximumScalars,observed.window_scalar_count);
        for (const auto& e:observed.changed_events) {
            const bool activation=e.status=="active" && statuses[e.event_id]!="active"; statuses[e.event_id]=e.status;
            // Missing/delayed coverage and the declared sub-bound fault must not
            // acquire a physical-alert pass merely because no observable onset
            // exists. Keep these explicit no-alert cases in the acceptance gate.
            if (activation && (healthy || quality || preset=="small_loss" || t<onset)) ++row.falseAlarms;
            const bool target=pump ? e.detector_id=="pump-delivery" && e.asset_ids.front()==ref.pump:
                !healthy && !quality && e.detector_id=="volume-balance" && e.asset_ids.front()==ref.firstTank;
            if (target && activation && row.activated<0) row.activated=t;
            if (target && e.status=="cleared" && row.cleared<0) row.cleared=t;
        }
        double metric=0,bound=0,truth=0;
        bool metricAvailable=false;
        if (pump) {
            const auto q=retained.at("route_flow:"+ref.pump); metric=ref.route(ref.pump).setpoint_m3h-q.value;
            bound=q.bound+policy.command_tracking_tolerance_m3h; truth=ref.route(ref.pump).setpoint_m3h-q.truth; metricAvailable=true;
        } else if (mapped.snapshot.availability=="available") {
            Point point; point.time=t; point.inventory=retained.at("inventory:"+ref.firstTank); double capacity=0;
            double oldest=t-point.inventory.time;
            for (const auto& r:ref.network.routes) if (r.source_id==ref.firstTank || r.target_id==ref.firstTank) {
                const auto& source=ref.asset(r.source_id); const auto q=retained.at((source.kind==AssetKind::source ? "source_flow:":"route_flow:")+(source.kind==AssetKind::source ? source.id:r.id));
                point.channels[r.id]=q; capacity+=r.capacity_m3h; oldest=std::max(oldest,t-q.time);
            }
            point.timing=capacity*(oldest+clockBound)/3600; history.push_back(point);
            while (history.size()>2 && history[1].time<=t-window) history.pop_front();
            if (history.back().time-history.front().time>=window) {
                const auto& first=history.front(); const auto& last=history.back(); double integral=0,truthIntegral=0,flowBound=0,quadrature=0,largest=0;
                for (std::size_t i=1;i<history.size();++i) {
                    const auto& a=history[i-1]; const auto& b=history[i]; const double dt=b.time-a.time;
                    largest=std::max(largest,std::abs(a.inventory.value));
                    for (const auto& [id,q]:a.channels) {
                        const auto& route=ref.route(id); const double sign=route.target_id==ref.firstTank ? 1:-1;
                        integral+=sign*q.value*dt/3600; truthIntegral+=sign*q.truth*dt/3600;
                        flowBound+=std::max(q.bound,b.channels.at(id).bound)*dt/3600;
                        quadrature+=std::abs(q.value-b.channels.at(id).value)*dt/3600;
                    }
                }
                metric=last.inventory.value-first.inventory.value-integral; truth=last.inventory.truth-first.inventory.truth-truthIntegral;
                const double arithmetic=64*std::numeric_limits<double>::epsilon()*history.size()*std::max({1.0,largest,std::abs(last.inventory.value),std::abs(integral)});
                bound=first.inventory.bound+last.inventory.bound+flowBound+quadrature+first.timing+last.timing+arithmetic; metricAvailable=true;
            }
        } else { history.clear(); comparator.reset(); }
        if (metricAvailable) {
            if (!std::isfinite(metric) || !(bound>0) || !std::isfinite(bound)) throw std::runtime_error("Invalid independent reference arithmetic");
            if (!healthy && !quality && t>=onset && row.observable<0 && std::abs(truth)>bound) row.observable=t;
            const auto [ewma,cusum]=comparator.update(metric/bound);
            if (ewma && row.ewmaAlarm<0) row.ewmaAlarm=t;
            if (cusum && row.cusumAlarm<0) row.cusumAlarm=t;
        }
    }
    row.targetMet=row.falseAlarms==0 && row.coverageMismatches==0 && row.boundMisses==0 && row.maximumPoints<=4096 && row.maximumScalars<=32768;
    if (!quality && !healthy && row.observable>=0) row.targetMet=row.targetMet && row.activated>=row.observable &&
        row.activated-row.observable<=(pump ? 10:60) && row.cleared>=recovery && row.cleared<=recovery+40;
    return row;
}
void numeric(std::ostream& out,double value) { if (value<0) out<<"null"; else out<<value; }
void csv(std::ostream& out,const std::vector<Row>& rows) {
    out<<"topology,seed,uncertainty_scale,preset,samples,coverage_failures,coverage_mismatches,false_native_activations,healthy_bound_checks,healthy_bound_misses,physical_onset_s,observable_onset_s,native_activation_s,native_latency_physical_s,native_latency_observable_s,native_clear_s,coverage_unavailable_s,ewma_alarm_s,cusum_alarm_s,maximum_window_points,maximum_window_scalars,target_met\n";
    for (const auto& r:rows) {
        const bool healthy=r.preset=="healthy" || r.preset=="clock_skew";
        out<<r.topology<<','<<r.seed<<','<<r.scale<<','<<r.preset<<','<<r.samples<<','<<r.coverageFailures<<','<<r.coverageMismatches<<','<<r.falseAlarms<<','<<r.boundChecks<<','<<r.boundMisses<<',';
        numeric(out,healthy ? -1:onset); out<<','; numeric(out,r.observable); out<<','; numeric(out,r.activated); out<<',';
        numeric(out,r.activated<0 ? -1:r.activated-onset); out<<','; numeric(out,r.activated<0 || r.observable<0 ? -1:r.activated-r.observable);
        out<<','; numeric(out,r.cleared); out<<','; numeric(out,r.coverageOnset); out<<','; numeric(out,r.ewmaAlarm); out<<','; numeric(out,r.cusumAlarm);
        out<<','<<r.maximumPoints<<','<<r.maximumScalars<<','<<(r.targetMet ? "true":"false")<<'\n';
    }
}
std::string summary(const std::vector<Row>& rows) {
    int failures=0,falseAlarms=0,coverageMismatches=0,boundMisses=0; std::uint64_t samples=0,boundChecks=0;
    for (const auto& r:rows) { failures+=!r.targetMet; falseAlarms+=r.falseAlarms; coverageMismatches+=r.coverageMismatches; boundMisses+=r.boundMisses; samples+=r.samples; boundChecks+=r.boundChecks; }
    std::ostringstream out; out<<std::setprecision(12);
    out<<"{\n  \"qualification_version\": 1,\n  \"detector_version\": \""<<detector_version<<"\",\n  \"topologies\": 2,\n  \"seeds_per_topology\": 10,\n  \"seed_range\": [2001,2010],\n  \"uncertainty_scales\": [1,10],\n  \"cases\": "<<rows.size()<<",\n  \"captures\": "<<samples<<",\n  \"healthy_and_skew_hours\": 80,\n  \"target_failures\": "<<failures<<",\n  \"false_native_activations_in_no_alert_cases\": "<<falseAlarms<<",\n  \"coverage_classification_mismatches\": "<<coverageMismatches<<",\n  \"healthy_absolute_bound_checks\": "<<boundChecks<<",\n  \"healthy_absolute_bound_misses\": "<<boundMisses<<",\n  \"passed\": "<<(failures ? "false":"true")<<",\n  \"presets\": [\n";
    bool first=true;
    for (const auto* preset:presets) {
        if (!first) out<<",\n"; first=false;
        int count=0,detected=0,observable=0,ewma=0,cusum=0,coverage=0; double nativeWorst=-1,physicalWorst=-1,ewmaWorst=-1,cusumWorst=-1;
        for (const auto& r:rows) if (r.preset==preset) {
            ++count; coverage+=r.coverageFailures; observable+=r.observable>=0; detected+=r.activated>=0; ewma+=r.ewmaAlarm>=0; cusum+=r.cusumAlarm>=0;
            if (r.activated>=0) physicalWorst=std::max(physicalWorst,r.activated-onset);
            if (r.activated>=0 && r.observable>=0) nativeWorst=std::max(nativeWorst,r.activated-r.observable);
            if (r.ewmaAlarm>=0) ewmaWorst=std::max(ewmaWorst,r.ewmaAlarm-onset);
            if (r.cusumAlarm>=0) cusumWorst=std::max(cusumWorst,r.cusumAlarm-onset);
        }
        out<<"    {\"preset\":\""<<preset<<"\",\"cases\":"<<count<<",\"observable_cases\":"<<observable<<",\"native_detected_cases\":"<<detected<<",\"native_worst_physical_latency_s\":"; numeric(out,physicalWorst);
        out<<",\"native_worst_observable_latency_s\":"; numeric(out,nativeWorst);
        out<<",\"coverage_unavailable_captures\":"<<coverage<<",\"ewma_alarm_cases\":"<<ewma<<",\"ewma_worst_physical_latency_s\":"; numeric(out,ewmaWorst);
        out<<",\"cusum_alarm_cases\":"<<cusum<<",\"cusum_worst_physical_latency_s\":"; numeric(out,cusumWorst); out<<'}';
    }
    out<<"\n  ],\n  \"baseline_parameters\": {\"ewma_lambda\":0.2,\"ewma_absolute_limit\":1,\"cusum_k\":0.5,\"cusum_h\":5,\"normalization\":\"declared absolute bound, not standard deviation\"},\n  \"scope\":\"Independent synthetic affine/step instruments at 1 Hz; correlated overlapping windows. No field accuracy, ARL probabilities, robust forecast guarantee or comparative model claim. Small faults below the declared bound remain explicitly unobservable.\"\n}\n";
    return out.str();
}
}
int main(int argc,char** argv) {
    try {
        if (argc!=1 && !(argc==3 && std::string(argv[1])=="--output")) throw std::invalid_argument("Use --output with a new evidence directory, or no arguments for the qualification check.");
        std::vector<Row> rows;
        for (int topology=0;topology<2;++topology) for (int seed=2001;seed<=2010;++seed) for (const int scale:{1,10}) for (const auto* preset:presets)
            rows.push_back(execute(topology,seed,scale,preset));
        const auto report=summary(rows);
        if (argc==3) {
            const std::filesystem::path directory(argv[2]); if (!std::filesystem::create_directory(directory)) throw std::invalid_argument("Evidence directory already exists or its parent is unavailable.");
            std::ofstream table(directory/"cases.csv"),json(directory/"summary.json"); table<<std::setprecision(17); csv(table,rows); json<<report;
            table.close(); json.close(); if (!table || !json) throw std::runtime_error("Qualification evidence could not be saved.");
        }
        std::cout<<report;
        return std::all_of(rows.begin(),rows.end(),[](const Row& row){return row.targetMet;}) ? 0:1;
    } catch (const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}

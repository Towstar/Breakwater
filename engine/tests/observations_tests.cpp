#include "breakwater/observations.hpp"
#include "breakwater/engine.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace breakwater;
namespace dto = breakwater::telemetry;
using namespace breakwater::observations;

namespace {
void require(bool value, const char* context) { if (!value) throw std::runtime_error(context); }
void near(double actual, double expected) { require(std::isfinite(actual) && std::abs(actual-expected)<1e-10,"Independent expected value differs"); }
struct Fixture {
    Network design{{{"tank-A","A",AssetKind::tank,100,40}, {"tank-B","B",AssetKind::tank,100,50},
                    {"source-X","X",AssetKind::source,0,0,36,6,36,10}, {"treat-Z","Z",AssetKind::treatment,50}},
                   {{"feed","source-X","tank-A",60,0,false,true}, {"pump","tank-A","tank-B",50,12,true,true},
                    {"outlet","tank-B","treat-Z",40,10,false,true}}};
    dto::SensorConfiguration configuration;
    dto::ObservedSnapshot snapshot;
    Fixture() {
        configuration.physical_hash = snapshot.physical_hash = "bound-design";
        snapshot.snapshot_id="frozen-1"; snapshot.configuration_hash="bound-sensors";
        snapshot.mode="synthetic_live"; snapshot.captured_event_time_s=100;
        snapshot.stream_id="source-1"; snapshot.epoch_id="epoch-1";
        add("a","inventory","tank-A","m3",41.0);
        add("b","inventory","tank-B","m3",48.0);
        add("s","source_flow","source-X","m3/h",30.0);
        add("p","route_flow","pump","m3/h",12.0);
        add("o","route_flow","outlet","m3/h",10.0);
        add("feedback","pump_state","pump","state",std::string("running"));
    }
    void add(const std::string& id,const std::string& quantity,const std::string& attachment,const std::string& unit,dto::Value value) {
        dto::Sensor sensor; sensor.sensor_id=id; sensor.quantity=quantity; sensor.unit=unit;
        sensor.attachment={quantity=="route_flow" || quantity=="pump_state" ? "route" : "asset",attachment};
        sensor.uncertainty_bound=0.01; sensor.calibration.error_bound=0.02;
        configuration.sensors.push_back(sensor);
        dto::Sample sample; sample.sensor_id=id; sample.frame_sequence=1;
        sample.event_time_s=sample.acquisition_start_s=sample.acquisition_end_s=100;
        sample.original_value=sample.canonical_value=value; sample.original_unit=sample.canonical_unit=unit;
        sample.receiver_status=sample.reported_quality="good"; snapshot.samples.push_back(sample);
    }
    MappingResult map() const { return map_volume_state(design,configuration,snapshot); }
};
void healthy_and_immutable() {
    Fixture f; auto result=f.map(); require(result.effective_network.has_value(),"Complete observation unavailable");
    require(result.snapshot.availability=="available" && !result.snapshot.actionable,"Snapshot granted authority");
    near(result.snapshot.expires_event_time_s,105);
    const auto forecast=simulate(compile(*result.effective_network),{1,0.125});
    near(forecast.volumes_m3[0].back(),59); near(forecast.volumes_m3[1].back(),50);
    near(f.design.assets[0].initial_m3,40); near(f.design.routes[1].setpoint_m3h,12);
    require(f.snapshot.derived_state.empty() && f.snapshot.coverage.required_sensor_ids.empty(),"Input snapshot mutated");
    require(result.values.size()==6,"Mapping omitted an operation-required measurement");
    near(result.values.front().bound,0.03);
}
void calibration_independent_examples() {
    dto::Sensor sensor; sensor.quantity="level"; sensor.unit="m"; sensor.uncertainty_bound=0.01;
    sensor.calibration.kind="level_volume_piecewise_linear"; sensor.calibration.error_bound=0.2;
    sensor.calibration.knots={{0,0},{1,30},{2,80}};
    const auto value=calibrate(sensor,1.4); require(value.has_value(),"Worked level example unavailable");
    near(value->value,50); near(value->bound,0.7);
    near(calibrate(sensor,1)->bound,0.7); // uncertainty spans two different slopes
    require(!calibrate(sensor,-0.1) && !calibrate(sensor,2.1) && !calibrate(sensor,0),"Extrapolation or boundary uncertainty accepted");
    sensor.uncertainty_bound=0; near(calibrate(sensor,0)->value,0); near(calibrate(sensor,2)->value,80);
    sensor.calibration.knots={{-1,-30},{0,0},{1,30}}; near(calibrate(sensor,0.5)->value,15);
    sensor.calibration.knots[1].level_m=-1; require(!calibrate(sensor,0),"Repeated calibration knot accepted");
    sensor.calibration.knots={{0,0},{1,std::numeric_limits<double>::infinity()}};
    require(!calibrate(sensor,0),"Nonfinite calibration accepted");
    sensor.quantity="inventory"; sensor.unit="m3"; sensor.calibration.kind="identity";
    sensor.uncertainty_bound=1e308;
    require(!calibrate(sensor,1e308),"Overflowing uncertainty interval accepted");
}
void coverage_and_time() {
    for (const auto* failure:{"missing","stale","bad","future","skew","unknown-pump","disabled","unit","nonfinite","duplicate","hash"}) {
        Fixture f; const std::string kind(failure);
        if (kind=="missing") f.snapshot.samples.erase(f.snapshot.samples.begin());
        if (kind=="stale") f.snapshot.samples[0].event_time_s=f.snapshot.samples[0].acquisition_end_s=f.snapshot.samples[0].acquisition_start_s=94;
        if (kind=="bad") f.snapshot.samples[0].reported_quality="bad";
        if (kind=="future") f.snapshot.samples[0].event_time_s=101;
        if (kind=="skew") f.snapshot.samples[0].event_time_s=f.snapshot.samples[0].acquisition_end_s=f.snapshot.samples[0].acquisition_start_s=98;
        if (kind=="unknown-pump") f.snapshot.samples.back().canonical_value=std::string("unknown");
        if (kind=="disabled") f.configuration.sensors[0].enabled=false;
        if (kind=="unit") f.snapshot.samples[0].canonical_unit="L";
        if (kind=="nonfinite") f.snapshot.samples[0].canonical_value=std::numeric_limits<double>::quiet_NaN();
        if (kind=="duplicate") f.snapshot.samples.push_back(f.snapshot.samples.front());
        if (kind=="hash") f.snapshot.physical_hash="different-design";
        const auto result=f.map();
        if (result.effective_network) throw std::runtime_error("Bad coverage accepted: "+kind);
        require(result.snapshot.availability=="unavailable" && !result.issues.empty(),"Unavailable coverage hidden");
    }
    Fixture f; f.add("unused","temperature","tank-A","degC",25.0); f.snapshot.samples.pop_back();
    require(f.map().effective_network.has_value(),"Unrelated unsupported channel made volume coverage unavailable");
}
void raw_values_and_failed_feedback() {
    for (double value:{-4.0,104.0}) {
        Fixture f; f.snapshot.samples[0].original_value=f.snapshot.samples[0].canonical_value=value;
        const auto result=f.map(); require(!result.effective_network,"Out-of-model inventory forecast accepted");
        near(std::get<double>(result.snapshot.samples[0].canonical_value),value);
        near(std::get<double>(result.snapshot.derived_state[0].value),value);
    }
    Fixture f; f.snapshot.samples[3].original_value=f.snapshot.samples[3].canonical_value=0.0;
    f.snapshot.samples.back().original_value=f.snapshot.samples.back().canonical_value=std::string("delivery_loss");
    const auto result=f.map(); require(result.effective_network.has_value(),"Measured pump loss cannot initialize an advisory forecast");
    const auto compiled=compile(*result.effective_network);
    require(!compiled.network.routes[1].available && compiled.network.routes[1].setpoint_m3h==0,"Feedback failure recovered by command");
    bool rejected=false;
    try { (void)compile(*result.effective_network,std::vector<ControlValue>{{"route:pump:flow",1}}); }
    catch (const std::invalid_argument&) { rejected=true; }
    require(rejected,"Candidate command re-enabled failed pump");
    f.snapshot.samples[3].canonical_value=2.0;
    require(!f.map().effective_network,"Failed pump with positive measured delivery accepted");
}
void redundant_intervals_and_level_mapping() {
    Fixture f; f.add("a-secondary","inventory","tank-A","m3",41.01);
    f.configuration.sensors.back().role="redundant";
    auto result=f.map(); require(result.effective_network.has_value(),"Overlapping redundant bounds rejected");
    near(result.effective_network->assets[0].initial_m3,41); near(result.values[0].bound,0.03);
    f.snapshot.samples.back().canonical_value=42.0;
    result=f.map(); require(!result.effective_network && result.snapshot.coverage.conflicting_sensor_ids.size()==2,"Conflicting redundancy accepted");
    f.configuration.sensors.pop_back(); f.snapshot.samples.pop_back();
    auto& level=f.configuration.sensors[0]; level.quantity="level"; level.unit="m";
    level.calibration.kind="level_volume_piecewise_linear"; level.calibration.knots={{0,0},{1,30},{2,80}};
    f.snapshot.samples[0].canonical_value=f.snapshot.samples[0].original_value=1.4;
    f.snapshot.samples[0].canonical_unit=f.snapshot.samples[0].original_unit="m";
    result=f.map(); require(result.effective_network.has_value(),"Calibrated inventory unavailable");
    near(result.effective_network->assets[0].initial_m3,50);
    require(result.snapshot.derived_state[0].origin=="inferred","Calibration disguised as direct measurement");
    f.snapshot.mode="replay"; require(!f.map().snapshot.actionable,"Replay acquired authority");
    Fixture feed; feed.add("feed-meter","route_flow","feed","m3/h",10.0);
    require(!feed.map().effective_network,"Contradictory measured feed was ignored");
    feed.snapshot.samples.back().canonical_value=30.01;
    require(feed.map().effective_network.has_value(),"Consistent optional measured feed rejected");
}
void spatial_support() {
    PipeConfig pipe; pipe.length_m=1; pipe.cells=4;
    const std::vector<double> profile{0,2,4,6}; dto::PhysicalSupport support;
    support.kind="interval_average"; support.start_m=0.125; support.end_m=0.625;
    near(*concentration_average(pipe,profile,support),2);
    support.kind="cell_average"; support.cell_index=2; near(*concentration_average(pipe,profile,support),4);
    support.kind="point_unavailable"; require(!concentration_average(pipe,profile,support),"Unsupported point interpolation invented");
    support.kind="interval_average"; support.end_m=1.1; require(!concentration_average(pipe,profile,support),"Outside-pipe interval accepted");
    support.end_m=0.125; require(!concentration_average(pipe,profile,support),"Zero-width interval accepted");
}
}
int main() {
    try {
        healthy_and_immutable(); calibration_independent_examples(); coverage_and_time();
        raw_values_and_failed_feedback(); redundant_intervals_and_level_mapping(); spatial_support();
        std::cout<<"6 observation suites passed: analytical balances, calibration bounds, coverage/time, raw values, redundancy and spatial support\n";
        return 0;
    } catch (const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}

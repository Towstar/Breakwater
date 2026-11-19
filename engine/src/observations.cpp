#include "breakwater/observations.hpp"
#include "breakwater/engine.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <set>

namespace breakwater::observations {
namespace {
bool nonnegative(double value) { return std::isfinite(value) && value >= 0; }
bool finite_value(const telemetry::Value& value) {
    const auto* number = std::get_if<double>(&value);
    return !number || std::isfinite(*number);
}
std::optional<double> interpolate(const telemetry::Calibration& calibration, double level) {
    const auto& knots = calibration.knots;
    if (knots.size() < 2 || knots.size() > 256 || !std::isfinite(level)) return {};
    for (std::size_t i = 0; i < knots.size(); ++i) {
        if (!std::isfinite(knots[i].level_m) || !std::isfinite(knots[i].volume_m3) ||
            (i && (knots[i].level_m <= knots[i-1].level_m || knots[i].volume_m3 <= knots[i-1].volume_m3))) return {};
    }
    if (level < knots.front().level_m || level > knots.back().level_m) return {};
    const auto upper = std::lower_bound(knots.begin(), knots.end(), level,
        [](const auto& knot, double x) { return knot.level_m < x; });
    if (upper == knots.begin()) return upper->volume_m3;
    const auto& lower = *(upper - 1);
    const auto result = lower.volume_m3 + (upper->volume_m3 - lower.volume_m3) *
        ((level - lower.level_m) / (upper->level_m - lower.level_m));
    return std::isfinite(result) ? std::optional{result} : std::nullopt;
}
void unique(std::vector<std::string>& list, const std::string& value) {
    if (std::find(list.begin(), list.end(), value) == list.end()) list.push_back(value);
}
struct Candidate { const telemetry::Sensor* sensor; const telemetry::Sample* sample; BoundedValue numeric; };
}

std::optional<BoundedValue> calibrate(const telemetry::Sensor& sensor, double value) {
    if (!std::isfinite(value) || !nonnegative(sensor.uncertainty_bound) || !nonnegative(sensor.calibration.error_bound) ||
        (sensor.instrument_min && value < *sensor.instrument_min) || (sensor.instrument_max && value > *sensor.instrument_max)) return {};
    if (sensor.quantity == "level") {
        if (sensor.unit != "m" || sensor.calibration.kind != "level_volume_piecewise_linear") return {};
        const auto center = interpolate(sensor.calibration, value);
        const auto low = interpolate(sensor.calibration, value - sensor.uncertainty_bound);
        const auto high = interpolate(sensor.calibration, value + sensor.uncertainty_bound);
        if (!center || !low || !high) return {};
        const auto bound = std::max(*center - *low, *high - *center) + sensor.calibration.error_bound;
        if (!nonnegative(bound) || !std::isfinite(*center-bound) || !std::isfinite(*center+bound)) return {};
        return BoundedValue{*center, bound};
    }
    const bool volume = sensor.quantity == "inventory" && sensor.unit == "m3";
    const bool flow = (sensor.quantity == "source_flow" || sensor.quantity == "route_flow") && sensor.unit == "m3/h";
    if (sensor.calibration.kind != "identity" || (!volume && !flow)) return {};
    const auto bound = sensor.uncertainty_bound + sensor.calibration.error_bound;
    return nonnegative(bound) && std::isfinite(value-bound) && std::isfinite(value+bound) ? std::optional{BoundedValue{value, bound}} : std::nullopt;
}

std::optional<double> concentration_average(const PipeConfig& pipe, std::span<const double> values,
                                           const telemetry::PhysicalSupport& support) {
    if (!std::isfinite(pipe.length_m) || pipe.length_m <= 0 || pipe.cells == 0 || pipe.cells > max_transport_cells ||
        values.size() != pipe.cells || std::any_of(values.begin(), values.end(), [](double x) { return !std::isfinite(x); })) return {};
    if (support.kind == "cell_average" && support.cell_index && *support.cell_index < values.size())
        return values[static_cast<std::size_t>(*support.cell_index)];
    if (support.kind != "interval_average" || !support.start_m || !support.end_m ||
        !nonnegative(*support.start_m) || !std::isfinite(*support.end_m) ||
        *support.end_m <= *support.start_m || *support.end_m > pipe.length_m) return {};
    const auto dx = pipe.length_m / static_cast<double>(pipe.cells);
    double integral = 0;
    for (std::size_t i = 0; i < values.size(); ++i) {
        const auto overlap = std::max(0.0, std::min(*support.end_m, dx * static_cast<double>(i+1)) -
                                             std::max(*support.start_m, dx * static_cast<double>(i)));
        integral += overlap * values[i];
    }
    const auto average = integral / (*support.end_m - *support.start_m);
    return std::isfinite(average) ? std::optional{average} : std::nullopt;
}

MappingResult map_volume_state(const Network& design, const telemetry::SensorConfiguration& configuration,
                               const telemetry::ObservedSnapshot& captured) {
    MappingResult out;
    out.snapshot = captured;
    auto& snapshot = out.snapshot;
    snapshot.actionable = false; snapshot.derived_state.clear(); snapshot.coverage = {}; snapshot.assumptions.clear();
    snapshot.availability = "unavailable";
    snapshot.expires_event_time_s = snapshot.captured_event_time_s;
    const auto issue = [&](const std::string& code, const std::string& attachment = {}, const std::string& sensor = {}) {
        out.issues.push_back({code, attachment, sensor});
    };
    if (!inspect_network(design).empty() || configuration.physical_hash != captured.physical_hash ||
        configuration.sensors.size() > telemetry::maximum_sensors || captured.samples.size() > telemetry::maximum_sensors ||
        !nonnegative(captured.captured_event_time_s) || !nonnegative(configuration.policy.max_skew_s) ||
        (captured.mode != "synthetic_live" && captured.mode != "replay" && captured.mode != "historical")) {
        issue("invalid_context"); return out;
    }
    std::map<std::string,const telemetry::Sample*> samples;
    std::set<std::string> sensor_ids;
    for (const auto& sensor : configuration.sensors) {
        if (sensor.sensor_id.empty() || !sensor_ids.insert(sensor.sensor_id).second) { issue("duplicate_sensor"); return out; }
    }
    for (const auto& sample : captured.samples) {
        if (!sensor_ids.contains(sample.sensor_id) || !samples.emplace(sample.sensor_id,&sample).second ||
            !finite_value(sample.canonical_value) || !finite_value(sample.original_value)) { issue("invalid_sample"); return out; }
    }
    Network effective = design;
    double earliest = std::numeric_limits<double>::infinity(), latest = -earliest, expiry = earliest;
    bool available = true;
    const auto select = [&](const std::string& quantity, const std::string& attachment, bool required = true) -> std::optional<MappedValue> {
        std::vector<Candidate> candidates;
        std::vector<const telemetry::Sensor*> configured;
        for (const auto& sensor : configuration.sensors) {
            if (!sensor.enabled || sensor.attachment.id != attachment ||
                (sensor.quantity != quantity && !(quantity == "inventory" && sensor.quantity == "level"))) continue;
            configured.push_back(&sensor);
            const auto found = samples.find(sensor.sensor_id);
            if (found == samples.end()) continue;
            const auto& sample = *found->second;
            const bool time_valid = nonnegative(sample.event_time_s) && sample.event_time_s <= captured.captured_event_time_s &&
                nonnegative(sample.acquisition_start_s) && sample.acquisition_start_s <= sample.acquisition_end_s &&
                sample.acquisition_end_s <= sample.event_time_s && nonnegative(sensor.max_age_s) &&
                captured.captured_event_time_s - sample.event_time_s <= sensor.max_age_s;
            if (!time_valid || sample.receiver_status != "good" || sample.reported_quality != "good" || sample.canonical_unit != sensor.unit ||
                sensor.physical_support.kind != "none") continue;
            if (quantity == "pump_state") {
                const auto* state = std::get_if<std::string>(&sample.canonical_value);
                if (!state || (*state != "running" && *state != "commanded_off" && *state != "delivery_loss" && *state != "unavailable")) continue;
                candidates.push_back({&sensor,&sample,{}});
            } else {
                const auto* value = std::get_if<double>(&sample.canonical_value);
                const auto numeric = value ? calibrate(sensor,*value) : std::nullopt;
                if (numeric) candidates.push_back({&sensor,&sample,*numeric});
            }
        }
        if (candidates.empty()) {
            if (!required) return {};
            available = false; issue("missing_or_unusable_" + quantity,attachment);
            if (!configured.empty()) {
                const auto& id = configured.front()->sensor_id;
                unique(snapshot.coverage.required_sensor_ids,id);
                if (!samples.contains(id)) unique(snapshot.coverage.missing_sensor_ids,id);
            }
            return {};
        }
        std::stable_sort(candidates.begin(),candidates.end(),[](const auto& a,const auto& b) {
            return (a.sensor->role == "primary") > (b.sensor->role == "primary");
        });
        auto selected = candidates.front();
        double low = -std::numeric_limits<double>::infinity(), high = -low;
        for (const auto& candidate : candidates) {
            const auto& id = candidate.sensor->sensor_id;
            unique(snapshot.coverage.required_sensor_ids,id);
            earliest = std::min(earliest,candidate.sample->event_time_s); latest = std::max(latest,candidate.sample->event_time_s);
            expiry = std::min(expiry,candidate.sample->event_time_s + candidate.sensor->max_age_s);
            if (quantity == "pump_state") {
                if (candidate.sample->canonical_value != selected.sample->canonical_value) { low = 1; high = 0; }
            } else {
                low = std::max(low,candidate.numeric.value-candidate.numeric.bound);
                high = std::min(high,candidate.numeric.value+candidate.numeric.bound);
            }
        }
        if (low > high) {
            available = false; issue("conflicting_"+quantity,attachment);
            for (const auto& candidate:candidates) unique(snapshot.coverage.conflicting_sensor_ids,candidate.sensor->sensor_id);
            return {};
        }
        // Keep the primary value and its declared bound. Redundancy is a consistency
        // check, not an unqualified uncertainty reduction or silent averaging rule.
        const bool pump = quantity == "pump_state";
        const auto unit = pump ? "state" : quantity == "inventory" ? "m3" : "m3/h";
        telemetry::Value mapped = pump ? selected.sample->canonical_value : telemetry::Value(selected.numeric.value);
        MappedValue value{quantity,attachment,unit,mapped,pump ? 0 : selected.numeric.bound,{selected.sensor->sensor_id}};
        const auto origin = selected.sensor->quantity == "level" ? "inferred" :
            selected.sample->original_unit != selected.sample->canonical_unit ? "converted" : "measured";
        snapshot.derived_state.push_back({quantity,attachment,unit,origin,mapped,value.sensor_ids});
        out.values.push_back(value);
        return value;
    };
    for (auto& asset : effective.assets) {
        if (asset.kind == AssetKind::tank) {
            const auto value = select("inventory",asset.id);
            if (value) {
                asset.initial_m3 = std::get<double>(value->value);
                if (asset.initial_m3 < 0 || asset.initial_m3 > asset.capacity_m3) {
                    available = false; issue("inventory_outside_model",asset.id);
                }
            }
        } else if (asset.kind == AssetKind::source) {
            const auto value = select("source_flow",asset.id);
            if (value) {
                const auto delivery = std::get<double>(value->value);
                asset.inflow_m3h = delivery + asset.curtailment_m3h;
                asset.curtailment_max_m3h = std::min(asset.curtailment_max_m3h,asset.inflow_m3h);
                if (delivery < 0 || !nonnegative(asset.inflow_m3h)) { available = false; issue("flow_outside_model",asset.id); }
            }
        }
    }
    for (auto& route : effective.routes) {
        const auto source = std::find_if(effective.assets.begin(),effective.assets.end(),[&](const auto& a) { return a.id == route.source_id; });
        if (source == effective.assets.end()) continue;
        if (source->kind == AssetKind::source) {
            // A second usable feed meter cannot contradict the source-delivery
            // channel and then be silently ignored. It is optional coverage.
            const auto feed = select("route_flow",route.id,false);
            const auto production = std::find_if(out.values.begin(),out.values.end(),[&](const auto& v) {
                return v.quantity == "source_flow" && v.attachment_id == source->id;
            });
            if (feed && production != out.values.end() && std::abs(std::get<double>(feed->value)-std::get<double>(production->value)) > feed->bound+production->bound) {
                available=false; issue("source_feed_delivery_conflict",route.id);
                for (const auto& id:feed->sensor_ids) unique(snapshot.coverage.conflicting_sensor_ids,id);
                for (const auto& id:production->sensor_ids) unique(snapshot.coverage.conflicting_sensor_ids,id);
            }
            continue;
        }
        if (source->kind != AssetKind::tank) continue;
        const auto measured = select("route_flow",route.id);
        if (measured) route.setpoint_m3h = std::get<double>(measured->value);
        if (route.has_pump) {
            const auto feedback = select("pump_state",route.id);
            if (feedback) {
                route.available = route.available && std::get<std::string>(feedback->value) == "running";
                if (!route.available && measured && route.setpoint_m3h != 0) {
                    available = false; issue("pump_feedback_delivery_conflict",route.id);
                }
                if (!route.available) route.setpoint_m3h = 0;
            }
        }
    }
    if (latest-earliest > configuration.policy.max_skew_s) { available = false; issue("incoherent_samples"); }
    if (!inspect_network(effective).empty()) { available = false; issue("effective_network_outside_model"); }
    std::erase_if(snapshot.derived_state,[&](const auto& state) {
        return std::any_of(state.source_sensor_ids.begin(),state.source_sensor_ids.end(),[&](const auto& id) {
            return std::find(snapshot.coverage.conflicting_sensor_ids.begin(),snapshot.coverage.conflicting_sensor_ids.end(),id) != snapshot.coverage.conflicting_sensor_ids.end();
        });
    });
    snapshot.assumptions = {
        "Volume state uses calibrated inventory, measured net delivery and reported pump feedback; commands remain separate.",
        "Level mapping uses the declared piecewise-linear calibration and absolute error bound without extrapolation.",
        "Forecasts hold measured deliveries and pump availability constant unless a candidate explicitly changes an eligible control.",
        "Source potential is inferred as measured net delivery plus the frozen commanded curtailment; its future constancy is an assumption.",
        "Redundant readings must have overlapping absolute bounds; the primary value and bound are retained without averaging."};
    if (available && std::isfinite(expiry) && expiry >= captured.captured_event_time_s) {
        snapshot.availability = "available"; snapshot.expires_event_time_s = expiry;
        out.effective_network = std::move(effective);
    }
    return out;
}
} // namespace breakwater::observations

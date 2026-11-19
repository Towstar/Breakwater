#include "ObservedMapping.hpp"
#include "TelemetryContracts.hpp"
#include "ProjectDocument.hpp"
#include "breakwater/observations.hpp"
#include <QJsonArray>
#include <stdexcept>

namespace breakwater::desktop::telemetry {
namespace {
QJsonArray strings(const std::vector<std::string>& values) {
    QJsonArray out; for (const auto& value:values) out.append(QString::fromStdString(value)); return out;
}
QJsonValue json(const breakwater::telemetry::Value& value) {
    if (const auto* number=std::get_if<double>(&value)) return *number;
    return QString::fromStdString(std::get<std::string>(value));
}
}
observations::MappingResult native_volume_capture(const QJsonObject& document,const QJsonObject& context,bool sourceAvailable) {
    const auto captured=parse_observed_snapshot(document,context);
    const auto configuration=parse_sensor_configuration(context.value("configuration").toObject(),context);
    const auto project=parse_project({{"version",1},{"network",context.value("network")},{"layout",QJsonObject{}}});
    if (physics_hash(project)!=context.value("physical_hash")) throw std::invalid_argument("Observed design hash mismatch.");
    auto mapped=observations::map_volume_state(project.network,configuration,captured);
    if (!sourceAvailable) {
        mapped.effective_network.reset(); mapped.snapshot.availability="unavailable";
        mapped.snapshot.expires_event_time_s=captured.captured_event_time_s;
        mapped.issues.push_back({"source_unavailable",{}, {}});
    }
    return mapped;
}
MappedCapture map_volume_capture(const QJsonObject& document,const QJsonObject& context,bool sourceAvailable) {
    const auto mapped=native_volume_capture(document,context,sourceAvailable);
    MappedCapture result; result.document=document;
    result.document["availability"]=QString::fromStdString(mapped.snapshot.availability);
    result.document["expires_event_time_s"]=mapped.snapshot.expires_event_time_s;
    result.document["actionable"]=false;
    result.document["assumptions"]=strings(mapped.snapshot.assumptions);
    result.document["coverage"]=QJsonObject{{"required_sensor_ids",strings(mapped.snapshot.coverage.required_sensor_ids)},
        {"missing_sensor_ids",strings(mapped.snapshot.coverage.missing_sensor_ids)},
        {"conflicting_sensor_ids",strings(mapped.snapshot.coverage.conflicting_sensor_ids)}};
    QJsonArray states,bounds,issues;
    for (const auto& state:mapped.snapshot.derived_state) states.append(QJsonObject{
        {"quantity",QString::fromStdString(state.quantity)},{"attachment_id",QString::fromStdString(state.attachment_id)},
        {"unit",QString::fromStdString(state.unit)},{"origin",QString::fromStdString(state.origin)},
        {"value",json(state.value)},{"source_sensor_ids",strings(state.source_sensor_ids)}});
    for (const auto& value:mapped.values) bounds.append(QJsonObject{{"quantity",QString::fromStdString(value.quantity)},
        {"attachment_id",QString::fromStdString(value.attachment_id)},{"unit",QString::fromStdString(value.unit)},
        {"value",json(value.value)},{"absolute_bound",value.bound},{"source_sensor_ids",strings(value.sensor_ids)}});
    for (const auto& issue:mapped.issues) issues.append(QJsonObject{{"code",QString::fromStdString(issue.code)},
        {"attachment_id",QString::fromStdString(issue.attachment_id)},{"sensor_id",QString::fromStdString(issue.sensor_id)}});
    result.document["derived_state"]=states;
    result.diagnostics={{"mapper_version",observations::mapper_version},{"mapping_status",result.document.value("availability")},
        {"bounds",bounds},{"issues",issues},{"forecast_integration","native_state_ready"},{"actionable",false}};
    if (mapped.effective_network) {
        result.effective_network=network_json(*mapped.effective_network);
        ProjectDocument effective; effective.network=*mapped.effective_network;
        result.diagnostics["effective_physical_hash"]=physics_hash(effective);
        result.diagnostics["design_physical_hash"]=context.value("physical_hash");
    }
    (void)parse_observed_snapshot(result.document,context);
    return result;
}
}

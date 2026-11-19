#include "SensorProject.hpp"
#include "TelemetryContracts.hpp"
#include <QJsonArray>
#include <QJsonDocument>
#include <QUuid>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace breakwater::desktop::telemetry {
QJsonObject sensor_project_context(const ProjectDocument& project) {
    const auto document = project_json(project);
    QJsonObject parameters, hashes;
    for (auto it = project.pipe_parameters.cbegin(); it != project.pipe_parameters.cend(); ++it) {
        parameters[it.key()] = quality::pipe_parameters_json(it.value());
        hashes[it.key()] = quality::pipe_parameters_hash(it.value());
    }
    return {{"project", document}, {"network", document.value("network")},
        {"physical_hash", physics_hash(project)}, {"pipe_parameters", parameters},
        {"pipe_parameter_hashes", hashes}};
}
QJsonObject empty_sensor_configuration(const ProjectDocument& project) {
    return {{"telemetry_version", 1}, {"kind", "sensor_configuration"},
        {"configuration_id", "configuration_" + QUuid::createUuid().toString(QUuid::Id128)},
        {"revision", 1}, {"physical_hash", physics_hash(project)},
        {"policy", QJsonObject{{"max_skew_s", 1.0}, {"reorder_window_s", 2.0},
            {"future_allowance_s", 0.25}, {"heartbeat_period_s", 1.0}, {"source_timeout_s", 5.0}}},
        {"sensors", QJsonArray{}}};
}
QJsonObject sensor_project_json(const ProjectDocument& project, const QJsonObject& configuration,
                               const QJsonObject& layout) {
    if (configuration.isEmpty()) return project_json(project);
    return {{"version", 2}, {"project", project_json(project)},
        {"sensor_configuration", configuration}, {"sensor_layout", layout}};
}
QJsonObject new_sensor_definition(const QString& quantity,const QString& attachmentId,const QString& id) {
    const QMap<QString,QString> units{{"inventory","m3"},{"level","m"},{"source_flow","m3/h"},{"route_flow","m3/h"},
        {"pump_state","state"},{"concentration","mg/L"},{"pressure","kPa"},{"temperature","degC"},{"custom_numeric","custom"}};
    if (!units.contains(quantity)) throw std::invalid_argument("Choose a supported sensor quantity.");
    const auto kind=(quantity=="route_flow" || quantity=="pump_state" || quantity=="concentration") ? "route":"asset";
    return {{"sensor_id",id},{"label",quantity+" "+attachmentId},{"quantity",quantity},{"unit",units.value(quantity)},
        {"source_id","synthetic-publisher"},{"channel_id",id},{"attachment",QJsonObject{{"kind",kind},{"id",attachmentId}}},
        {"enabled",true},{"expected_period_s",1.0},{"max_age_s",5.0},{"uncertainty_bound",0.0},{"group_id",id},{"role","primary"},
        {"physical_support",QJsonObject{{"kind","none"},{"acquisition_window_s",0.0},{"delay_s",0.0}}},
        {"calibration",QJsonObject{{"calibration_id",id},{"version",1},{"kind","identity"},{"error_bound",0.0}}}};
}
QJsonObject volume_sensor_configuration(const ProjectDocument& project) {
    auto configuration=empty_sensor_configuration(project); QJsonArray sensors;
    const auto add=[&](const QString& quantity,const std::string& attachment,const std::string& label) {
        auto sensor=new_sensor_definition(quantity,QString::fromStdString(attachment),"sensor_"+QUuid::createUuid().toString(QUuid::Id128));
        sensor["label"]=QString::fromStdString(label)+" · "+QString(quantity).replace('_',' ');
        sensor["uncertainty_bound"]=quantity=="pump_state" ? 0.0:0.002;
        sensors.append(sensor);
    };
    for (const auto& asset:project.network.assets) {
        if (asset.kind==AssetKind::tank) add("inventory",asset.id,asset.label);
        if (asset.kind==AssetKind::source) add("source_flow",asset.id,asset.label);
    }
    for (const auto& route:project.network.routes) {
        const auto source=std::find_if(project.network.assets.begin(),project.network.assets.end(),[&](const auto& a){return a.id==route.source_id;});
        if (source!=project.network.assets.end() && source->kind==AssetKind::tank) {
            add("route_flow",route.id,route.id);
            if (route.has_pump) add("pump_state",route.id,route.id);
        }
    }
    if (sensors.size()>static_cast<qsizetype>(breakwater::telemetry::maximum_sensors))
        throw std::invalid_argument("This network needs more than 256 sensors for full volume coverage.");
    configuration["sensors"]=sensors; (void)parse_sensor_configuration(configuration,sensor_project_context(project));
    return configuration;
}
void validate_sensor_project(const ProjectDocument& project, const QJsonObject& configuration,
                             const QJsonObject& layout) {
    if (configuration.isEmpty()) {
        if (!layout.isEmpty()) throw std::invalid_argument("Sensor markers require a sensor configuration.");
        return;
    }
    validate_document(sensor_project_json(project, configuration, layout), "project_v2", sensor_project_context(project));
    if (QJsonDocument(sensor_project_json(project, configuration, layout)).toJson(QJsonDocument::Indented).size() > 2 * 1024 * 1024)
        throw std::invalid_argument("Sensor project exceeds the editor's 2 MiB save/load limit.");
    for (const auto& value : layout) {
        for (const auto& coordinate : value.toObject()) {
            if (std::abs(coordinate.toDouble()) > 10000.0)
                throw std::invalid_argument("Sensor marker coordinates exceed 10000 scene units.");
        }
    }
}
QJsonObject sensor_anchor(const ProjectDocument& project, const QJsonObject& sensor) {
    const auto attachment = sensor.value("attachment").toObject();
    const auto id = attachment.value("id").toString();
    auto position = project.positions.value(id);
    double height = 90.0;
    if (attachment.value("kind") == "route") {
        height = 25.0;
        for (const auto& route : project.network.routes) if (QString::fromStdString(route.id) == id) {
            position = (project.positions.value(QString::fromStdString(route.source_id)) +
                        project.positions.value(QString::fromStdString(route.target_id))) / 2.0F;
            break;
        }
    }
    return {{"x", position.x()}, {"y", height}, {"z", position.z()}};
}
} // namespace breakwater::desktop::telemetry

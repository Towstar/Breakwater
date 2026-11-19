#pragma once
#include "telemetry/TelemetryStream.hpp"
#include "telemetry/TelemetryContracts.hpp"
#include "telemetry/SensorProject.hpp"
#include "ProjectDocument.hpp"
#include <QFile>
#include <QJsonDocument>
#include <algorithm>
#include <stdexcept>

namespace breakwater::desktop::test_observation {
using namespace telemetry;
inline QJsonObject sourceJson(const char* name) {
    QFile file(QStringLiteral(BREAKWATER_SOURCE_DIR)+QString::fromLatin1(name));
    if (!file.open(QIODevice::ReadOnly)) throw std::runtime_error("Missing capture fixture");
    return decode_document(file.readAll());
}
inline QByteArray bytes(const QJsonObject& value) { return QJsonDocument(value).toJson(QJsonDocument::Compact)+'\n'; }
struct Fixture {
    ProjectDocument project=parse_project(sourceJson("/contracts/telemetry/examples/canonical-project-v1.json"));
    QJsonObject configuration=sourceJson("/contracts/telemetry/examples/canonical-sensors.json");
    QJsonObject context;
    QJsonArray readings,commands;
    TelemetryStream stream;
    explicit Fixture(bool renamed=false) {
        if (renamed) {
            for (auto& asset:project.network.assets) { asset.id="custom_"+asset.id; asset.label="Custom "+asset.label; }
            for (auto& route:project.network.routes) { route.id="custom_"+route.id; route.source_id="custom_"+route.source_id; route.target_id="custom_"+route.target_id; }
            project.network.routes[0].target_id="custom_tank_b";
            project.network.routes[1].target_id="custom_tank_a";
            const auto parameters=project.pipe_parameters; project.pipe_parameters.clear();
            for (auto i=parameters.begin();i!=parameters.end();++i) project.pipe_parameters["custom_"+i.key()]=i.value();
            configuration["physical_hash"]=physics_hash(project);
        }
        const auto prototype=configuration.value("sensors").toArray().first().toObject();
        QJsonArray sensors;
        const auto add=[&](const QString& quantity,const QString& unit,const QString& attachment,const QJsonValue& value) {
            auto sensor=prototype; const auto id=quantity+"-"+attachment;
            sensor["sensor_id"]=id; sensor["channel_id"]=id; sensor["group_id"]=id; sensor["label"]=id;
            sensor["quantity"]=quantity; sensor["unit"]=unit;
            sensor["attachment"]=QJsonObject{{"kind",quantity=="inventory" || quantity=="source_flow" ? "asset":"route"},{"id",attachment}};
            sensors.append(sensor);
            readings.append(QJsonObject{{"sensor_id",id},{"event_time_s",100},{"acquisition_start_s",100},{"acquisition_end_s",100},
                {"value",value},{"unit",unit},{"quality","good"}});
        };
        for (const auto& asset:project.network.assets) {
            if (asset.kind==breakwater::AssetKind::tank) add("inventory","m3",QString::fromStdString(asset.id),asset.initial_m3);
            if (asset.kind==breakwater::AssetKind::source) add("source_flow","m3/h",QString::fromStdString(asset.id),asset.inflow_m3h-asset.curtailment_m3h);
        }
        for (const auto& route:project.network.routes) {
            const auto source=std::find_if(project.network.assets.begin(),project.network.assets.end(),[&](const auto& a){return a.id==route.source_id;});
            if (source->kind!=breakwater::AssetKind::tank) continue;
            add("route_flow","m3/h",QString::fromStdString(route.id),route.setpoint_m3h);
            if (route.has_pump) add("pump_state","state",QString::fromStdString(route.id),route.available ? "running":"unavailable");
        }
        configuration["sensors"]=sensors;
        context=sensor_project_context(project); context["configuration"]=configuration;
        for (const auto& control:breakwater::control_catalog(project.network)) commands.append(QJsonObject{
            {"control_id",QString::fromStdString(control.id)},{"value",control.value},{"unit",QString::fromStdString(control.unit)}});
    }
    QJsonObject frame(const QString& type,int sequence,double event,const QJsonObject& payload) const {
        return {{"telemetry_version",1},{"kind","stream_frame"},{"type",type},{"stream_id","local-plant"},{"epoch_id","epoch-one"},
            {"sequence",sequence},{"configuration_hash",sensor_semantic_hash(configuration)},{"physical_hash",physics_hash(project)},
            {"event_time_s",event},{"payload",payload}};
    }
    void begin() {
        stream.configure(configuration,context); stream.beginEpoch("synthetic_live");
        if (!stream.ingest(bytes(frame("start",0,100,{{"mode","synthetic_live"},{"clock_rate",1},{"paused",false}})),100,10,"2026-11-18T00:00:00.000Z") ||
            !stream.ingest(bytes(frame("readings",1,100,{{"readings",readings}})),100,10.01,"2026-11-18T00:00:00.010Z"))
            throw std::runtime_error("Cannot begin capture fixture");
    }
};
}

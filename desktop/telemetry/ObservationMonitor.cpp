#include "ObservationMonitor.hpp"
#include "ObservedMapping.hpp"
#include "TelemetryContracts.hpp"
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>
#include <QUuid>
#include <cmath>
#include <stdexcept>

namespace breakwater::desktop::telemetry {
namespace {
QString q(const std::string& value) { return QString::fromStdString(value); }
QJsonArray strings(const std::vector<std::string>& values) { QJsonArray a; for (const auto& v:values) a.append(q(v)); return a; }
QJsonObject eventJson(const breakwater::telemetry::DetectionEvent& e,const QJsonObject& context) {
    QJsonArray evidence;
    for (const auto& v:e.evidence) evidence.append(QJsonObject{{"metric",q(v.metric)},{"unit",q(v.unit)},{"value",v.value},{"threshold",v.threshold}});
    QJsonObject result{{"telemetry_version",1},{"kind","detection_event"},{"event_id",q(e.event_id)},{"revision",static_cast<qint64>(e.revision)},
        {"physical_hash",q(e.physical_hash)},{"configuration_hash",q(e.configuration_hash)},{"stream_id",q(e.stream_id)},{"epoch_id",q(e.epoch_id)},
        {"snapshot_id",q(e.snapshot_id)},{"detector_id",q(e.detector_id)},{"detector_version",q(e.detector_version)},{"threshold_version",q(e.threshold_version)},
        {"status",q(e.status)},{"severity",q(e.severity)},{"onset_event_time_s",e.onset_event_time_s},{"updated_event_time_s",e.updated_event_time_s},
        {"cleared_event_time_s",e.cleared_event_time_s ? QJsonValue(*e.cleared_event_time_s):QJsonValue(QJsonValue::Null)},
        {"acknowledgement",e.acknowledgement ? QJsonValue(QJsonObject{{"actor",q(e.acknowledgement->actor)},{"acknowledged_utc",q(e.acknowledgement->acknowledged_utc)}}):QJsonValue(QJsonValue::Null)},
        {"sensor_ids",strings(e.sensor_ids)},{"asset_ids",strings(e.asset_ids)},{"evidence",evidence},{"diagnosis",q(e.diagnosis)},{"explanation",q(e.explanation)},{"actionable",false}};
    (void)parse_detection_event(result,context); return result;
}
QJsonObject bound(const observations::BoundedValue& v) { return {{"value",v.value},{"absolute_bound",v.bound}}; }
}
void validate_detector_capture(const QJsonObject& report,const QJsonObject& snapshot) {
    if (report.isEmpty()) return; // Older/direct stream captures have no detector report.
    const auto require=[](bool condition) { if (!condition) throw std::invalid_argument("Frozen detector coverage is malformed or belongs to another capture."); };
    const QSet<QString> fields{"detector_version","balance_status","temporal_model","actionable","window_s","window_points","window_scalars",
        "snapshot_id","mode","as_of_s","physical_hash","configuration_hash","stream_id","epoch_id","numerical_revision","error"};
    const QSet<QString> statuses{"waiting_for_samples","unavailable","invalid_clock","temporal_model_unavailable","coverage_unavailable",
        "unchanged_samples","warming_up","mapping_unavailable","available","arithmetic_unavailable","window_resource_limit"};
    require(QJsonDocument(report).toJson(QJsonDocument::Compact).size()<=32768);
    for (auto i=report.begin();i!=report.end();++i) require(fields.contains(i.key()));
    const auto version=report.value("detector_version").toString();
    require((version==QString::fromLatin1(observations::detector_version) || version=="bounded-volume-monitor-0.1.0") &&
        statuses.contains(report.value("balance_status").toString()) && report.value("actionable")==false &&
        (report.value("temporal_model")=="unavailable" || report.value("temporal_model")=="declared_piecewise_constant_delivery"));
    for (const auto* field:{"as_of_s","window_s","window_points","window_scalars"}) if (report.contains(QLatin1String(field))) {
        const auto v=report.value(QLatin1String(field)); require(v.isDouble() && std::isfinite(v.toDouble()) && v.toDouble()>=0);
    }
    if (report.contains("window_points") || report.contains("window_scalars")) {
        const auto points=report.value("window_points"),scalars=report.value("window_scalars");
        require(points.isDouble() && scalars.isDouble() && points.toDouble()<=4096 && scalars.toDouble()<=32768 &&
            std::floor(points.toDouble())==points.toDouble() && std::floor(scalars.toDouble())==scalars.toDouble());
    }
    if (report.contains("as_of_s")) {
        require(report.value("as_of_s").toDouble()<=snapshot.value("captured_event_time_s").toDouble() &&
            report.value("snapshot_id").isString() && !report.value("snapshot_id").toString().isEmpty() &&
            report.value("snapshot_id").toString().size()<=128 && report.value("mode")==snapshot.value("mode"));
        for (const auto* field:{"physical_hash","configuration_hash","stream_id","epoch_id","numerical_revision"})
            if (version==QString::fromLatin1(observations::detector_version) || report.contains(QLatin1String(field)))
                require(report.value(QLatin1String(field))==snapshot.value(QLatin1String(field)));
    } else require(report.value("balance_status")=="waiting_for_samples");
    if (report.contains("error")) require(report.value("error").isString() && report.value("error").toString().size()<=1024);
}
ObservationMonitor::ObservationMonitor() { reset(); }
void ObservationMonitor::reset(bool hold) {
    observations::DetectionPolicy policy; policy.piecewise_constant_delivery=hold;
    detector_=std::make_unique<observations::ObservationDetector>("event-"+QUuid::createUuid().toString(QUuid::Id128).toStdString(),policy);
    context_={}; report_={{"detector_version",observations::detector_version},{"balance_status","waiting_for_samples"},
        {"temporal_model",hold ? "declared_piecewise_constant_delivery":"unavailable"},{"events",QJsonArray{}},{"actionable",false}};
}
QJsonObject ObservationMonitor::observe(const ObservedEvidence& capture) {
    const auto mapped=native_volume_capture(capture.document,capture.context,capture.diagnostics.value("source_available").toBool());
    const auto update=detector_->observe(mapped); context_=capture.context;
    QJsonArray events,changes,witness;
    for (const auto& e:update.events) events.append(eventJson(e,context_));
    for (const auto& e:update.changed_events) changes.append(eventJson(e,context_));
    for (const auto& point:update.witness) {
        QJsonObject channels;
        for (const auto& [id,value]:point.delivery_channels) channels[q(id)]=bound(value);
        witness.append(QJsonObject{{"time_s",point.time_s},{"snapshot_id",q(point.snapshot_id)},{"tank_id",q(point.tank_id)},
            {"inventory_m3",bound(point.inventory)},{"net_delivery_m3h",bound(point.net_rate)},
            {"timing_volume_bound_m3",point.timing_volume_bound},{"delivery_channels_m3h",channels}});
    }
    report_["events"]=events; report_["balance_status"]=q(update.balance_status); report_["window_s"]=update.observed_window_s;
    report_["window_points"]=static_cast<qint64>(update.window_point_count); report_["window_scalars"]=static_cast<qint64>(update.window_scalar_count);
    report_["snapshot_id"]=capture.document.value("snapshot_id"); report_["mode"]=capture.document.value("mode");
    report_["as_of_s"]=capture.document.value("captured_event_time_s");
    for (const auto* field:{"physical_hash","configuration_hash","stream_id","epoch_id","numerical_revision"})
        report_[QLatin1String(field)]=capture.document.value(QLatin1String(field));
    return {{"events",changes},{"balance_witness",witness}};
}
QJsonObject ObservationMonitor::acknowledge(const QString& id,const QString& actor,const QString& utc) {
    const auto found=detector_->acknowledge(id.toStdString(),{actor.toStdString(),utc.toStdString()});
    if (!found) return {};
    const auto event=eventJson(*found,context_); auto events=report_.value("events").toArray();
    for (qsizetype i=0;i<events.size();++i) if (events[i].toObject().value("event_id")==id) { events[i]=event; break; }
    report_["events"]=events; return event;
}
}

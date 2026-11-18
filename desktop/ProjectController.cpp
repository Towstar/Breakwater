#include "ProjectController.hpp"
#include "telemetry/SensorProject.hpp"
#include "telemetry/TelemetryContracts.hpp"
#include <QtConcurrentRun>
#include <QFile>
#include <QFutureWatcher>
#include <QJsonDocument>
#include <QJsonArray>
#include <QSaveFile>
#include <QSet>
#include <QUuid>
#include <algorithm>
#include <cmath>

namespace breakwater::desktop {
namespace {
QString q(const std::string& s) { return QString::fromStdString(s); }
std::string fresh_id(const char* prefix) { return std::string(prefix) + QUuid::createUuid().toString(QUuid::Id128).toStdString(); }
struct JobResult { SimulationResult result; ValidationReport validation; QString error; };
}

ProjectController::ProjectController(QObject* parent) : QObject(parent), project_(example_project()) {}
void ProjectController::reportAction(const QString& code, const QString& title, const QString& message,
                                     const QStringList& assetIds, const QString& field, const QString& severity) {
    const auto id = QString::number(++action_feedback_sequence_);
    action_feedback_ = {{"id", id}, {"operation_id", id}, {"code", code}, {"severity", severity},
        {"category", "editor_action"}, {"title", title}, {"message", message}, {"asset_ids", assetIds},
        {"field", field}, {"model_changed", false}, {"recovery_action", "view_issues"}};
    summary_ = message;
    emit actionFeedbackChanged(); emit changed();
}
void ProjectController::clearActionFeedback() {
    if (action_feedback_.isEmpty()) return;
    action_feedback_.clear(); emit actionFeedbackChanged();
}
QJsonObject ProjectController::editorDocument() const {
    return telemetry::sensor_project_json(project_, sensor_configuration_, sensor_layout_);
}
QJsonObject ProjectController::sensorConfiguration() const {
    if (!sensor_configuration_.isEmpty()) return sensor_configuration_;
    auto result = telemetry::empty_sensor_configuration(project_);
    result["configuration_id"] = "unconfigured";
    return result;
}
QJsonObject ProjectController::sensorContext() const {
    auto context = telemetry::sensor_project_context(project_);
    context["configuration"] = sensorConfiguration();
    return context;
}
QString ProjectController::sensorConfigurationHash() const {
    return telemetry::sensor_semantic_hash(sensorConfiguration());
}
QVariantList ProjectController::sensorMarkers() const {
    QVariantList rows;
    for (const auto& value : sensor_configuration_.value("sensors").toArray()) {
        const auto sensor = value.toObject(); const auto id = sensor.value("sensor_id").toString();
        const auto position = sensor_layout_.contains(id) ? sensor_layout_.value(id).toObject() : telemetry::sensor_anchor(project_, sensor);
        rows.append(QVariantMap{{"id", id}, {"label", sensor.value("label").toString()},
            {"quantity", sensor.value("quantity").toString()}, {"unit", sensor.value("unit").toString()},
            {"attachment_id", sensor.value("attachment").toObject().value("id").toString()},
            {"x", position.value("x").toDouble()}, {"y", position.value("y").toDouble()}, {"z", position.value("z").toDouble()},
            {"selected", selected_sensor_id_ == id}, {"enabled", sensor.value("enabled").toBool()}});
    }
    return rows;
}
QVariantMap ProjectController::selectedSensor() const {
    for (const auto& value : sensor_configuration_.value("sensors").toArray()) {
        auto sensor = value.toObject();
        if (sensor.value("sensor_id") == selected_sensor_id_) {
            sensor["marker"] = sensor_layout_.contains(selected_sensor_id_) ? sensor_layout_.value(selected_sensor_id_) : telemetry::sensor_anchor(project_, sensor);
            return sensor.toVariantMap();
        }
    }
    return {};
}
void ProjectController::selectSensor(const QString& id) {
    selected_sensor_id_.clear();
    for (const auto& value : sensor_configuration_.value("sensors").toArray())
        if (value.toObject().value("sensor_id") == id) selected_sensor_id_ = id;
    emit changed();
}
bool ProjectController::addSensor(const QString& quantity, const QString& attachmentId, const QVariantMap& fields) {
    const auto id = QString::fromStdString(fresh_id("sensor_"));
    const QMap<QString, QString> units{{"inventory", "m3"}, {"level", "m"}, {"source_flow", "m3/h"},
        {"route_flow", "m3/h"}, {"pump_state", "state"}, {"concentration", "mg/L"},
        {"pressure", "kPa"}, {"temperature", "degC"}, {"custom_numeric", "custom"}};
    if (!units.contains(quantity)) { summary_ = "Choose a supported sensor quantity."; emit changed(); return false; }
    const auto kind = (quantity == "route_flow" || quantity == "pump_state" || quantity == "concentration") ? "route" : "asset";
    QJsonObject sensor{{"sensor_id", id}, {"label", quantity + " " + attachmentId}, {"quantity", quantity}, {"unit", units.value(quantity)},
        {"source_id", "synthetic-publisher"}, {"channel_id", id}, {"attachment", QJsonObject{{"kind", kind}, {"id", attachmentId}}},
        {"enabled", true}, {"expected_period_s", 1.0}, {"max_age_s", 5.0}, {"uncertainty_bound", 0.0},
        {"group_id", id}, {"role", "primary"},
        {"physical_support", QJsonObject{{"kind", "none"}, {"acquisition_window_s", 0.0}, {"delay_s", 0.0}}},
        {"calibration", QJsonObject{{"calibration_id", id}, {"version", 1}, {"kind", "identity"}, {"error_bound", 0.0}}}};
    const auto patch = QJsonObject::fromVariantMap(fields);
    for (auto it = patch.begin(); it != patch.end(); ++it) sensor[it.key()] = it.value();
    sensor["sensor_id"] = id;
    return edit([&] {
        if (sensor_configuration_.isEmpty()) sensor_configuration_ = telemetry::empty_sensor_configuration(project_);
        auto sensors = sensor_configuration_.value("sensors").toArray(); sensors.append(sensor);
        sensor_configuration_["sensors"] = sensors; sensor_layout_[id] = telemetry::sensor_anchor(project_, sensor);
        selected_sensor_id_ = id;
    });
}
bool ProjectController::configureSensor(const QString& id, const QVariantMap& fields) {
    auto sensors = sensor_configuration_.value("sensors").toArray();
    for (qsizetype i = 0; i < sensors.size(); ++i) {
        auto sensor = sensors[i].toObject();
        if (sensor.value("sensor_id") != id) continue;
        const auto previousAttachment = sensor.value("attachment");
        const auto previousCalibration = sensor.value("calibration").toObject();
        const auto patch = QJsonObject::fromVariantMap(fields);
        for (auto it = patch.begin(); it != patch.end(); ++it) {
            if (it.value().isNull() && (it.key() == "instrument_min" || it.key() == "instrument_max")) sensor.remove(it.key());
            else sensor[it.key()] = it.value();
        }
        if (sensor.value("sensor_id") != id) { summary_ = "Sensor identity is immutable; create another sensor to change it."; emit changed(); return false; }
        auto calibration = sensor.value("calibration").toObject();
        if (calibration != previousCalibration && calibration.value("version").isDouble() &&
            std::isfinite(calibration.value("version").toDouble()) && calibration.value("version").toDouble() >= 1 &&
            std::floor(calibration.value("version").toDouble()) == calibration.value("version").toDouble() &&
            calibration.value("version").toDouble() <= previousCalibration.value("version").toDouble())
            calibration["version"] = previousCalibration.value("version").toDouble() + 1.0;
        sensor["calibration"] = calibration; sensors[i] = sensor;
        if (sensor.value("role") == "primary") {
            for (qsizetype other = 0; other < sensors.size(); ++other) {
                auto peer = sensors[other].toObject();
                if (other != i && peer.value("group_id") == sensor.value("group_id") && peer.value("role") == "primary") {
                    peer["role"] = "redundant"; sensors[other] = peer;
                }
            }
        }
        return edit([&] {
            sensor_configuration_["sensors"] = sensors;
            if (sensor.value("attachment") != previousAttachment) sensor_layout_[id] = telemetry::sensor_anchor(project_, sensor);
        });
    }
    return false;
}
bool ProjectController::deleteSensor(const QString& id) {
    auto sensors = sensor_configuration_.value("sensors").toArray();
    for (qsizetype i = 0; i < sensors.size(); ++i) if (sensors[i].toObject().value("sensor_id") == id) {
        sensors.removeAt(i);
        return edit([&] { sensor_configuration_["sensors"] = sensors; sensor_layout_.remove(id); if (selected_sensor_id_ == id) selected_sensor_id_.clear(); });
    }
    return false;
}
bool ProjectController::moveSensor(const QString& id, double x, double y, double z) {
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) || std::abs(x) > 10000 || std::abs(y) > 10000 || std::abs(z) > 10000) return false;
    for (const auto& value : sensor_configuration_.value("sensors").toArray()) if (value.toObject().value("sensor_id") == id)
        return edit([&] { sensor_layout_[id] = QJsonObject{{"x", x}, {"y", y}, {"z", z}}; });
    return false;
}
void ProjectController::synchronizeSensorConfiguration() {
    if (sensor_configuration_.isEmpty()) return;
    sensor_configuration_["physical_hash"] = networkHash();
    auto sensors = sensor_configuration_.value("sensors").toArray();
    for (qsizetype i = 0; i < sensors.size(); ++i) {
        auto sensor = sensors[i].toObject();
        if (sensor.value("quantity") == "concentration") {
            const auto route = sensor.value("attachment").toObject().value("id").toString();
            if (!project_.pipe_parameters.contains(route)) throw std::invalid_argument("Concentration needs explicitly configured pipe transport geometry first.");
            sensor["pipe_parameter_hash"] = quality::pipe_parameters_hash(project_.pipe_parameters.value(route));
            sensors[i] = sensor;
        }
    }
    sensor_configuration_["sensors"] = sensors;
}
QJsonObject ProjectController::pipeParameters(const QString& routeId) const {
    return quality::pipe_parameters_json(project_.pipe_parameters.value(routeId));
}
bool ProjectController::setPipeParameters(const QString& routeId, const QJsonObject& parameters, bool requireFreshForecast) {
    const bool exists = std::any_of(project_.network.routes.begin(), project_.network.routes.end(), [&](const Route& r) { return q(r.id) == routeId; });
    if (!exists) return false;
    try {
        const auto parsed = quality::parse_pipe_parameters(parameters);
        return edit([&] { project_.pipe_parameters[routeId] = parsed; }, requireFreshForecast);
    } catch (const std::exception&) { return false; }
}
QJsonObject ProjectController::capturedRun() const {
    const auto captured = capturedForecast();
    if (!captured || captured->result.time_h.empty()) return {};
    return {{"run_id", captured->id}, {"snapshot_id", captured->id}, {"physical_hash", captured->physical_hash},
        {"numerical_revision", static_cast<qint64>(captured->revision)}, {"horizon_h", captured->config.horizon_h},
        {"sample_step_h", captured->config.step_h},
        {"network", network_json(captured->network)}, {"controls", QJsonArray{}}, {"historical", false},
        {"volume_feasible", run_->validation.feasible}};
}
std::shared_ptr<const CapturedForecast> ProjectController::capturedForecast() const {
    return !busy_ && run_ && run_->revision == revision_ ? run_ : nullptr;
}
std::size_t ProjectController::sampleIndex() const {
    return run_ ? static_cast<std::size_t>(std::llround(forecast_fraction_ * static_cast<double>(run_->result.time_h.size() - 1))) : 0;
}
double ProjectController::forecastTimeH() const { return run_ ? run_->result.time_h[sampleIndex()] : 0.0; }
QString ProjectController::runId() const { return run_ ? run_->id : QString{}; }
bool ProjectController::canApprove() const { return run_ && run_->revision == revision_ && run_->validation.feasible && !approved_; }
QVariantMap ProjectController::camera() const { return {{"yaw", project_.camera_yaw}, {"pitch", project_.camera_pitch}, {"distance", project_.camera_distance}, {"x", project_.camera_x}, {"z", project_.camera_z}}; }
void ProjectController::setCamera(double yaw, double pitch, double distance, double x, double z) {
    if (!std::isfinite(yaw) || !std::isfinite(pitch) || !std::isfinite(distance) || !std::isfinite(x) || !std::isfinite(z)) return;
    project_.camera_yaw = yaw; project_.camera_pitch = std::clamp(pitch, -85.0, -15.0); project_.camera_distance = std::clamp(distance, 250.0, 1600.0);
    project_.camera_x = std::clamp(x, -10000.0, 10000.0); project_.camera_z = std::clamp(z, -10000.0, 10000.0);
}
void ProjectController::setForecastFraction(double value) {
    if (!std::isfinite(value)) return;
    forecast_fraction_ = std::clamp(value, 0.0, 1.0); emit changed();
}
QVariantList ProjectController::assets() const {
    QVariantList list;
    for (const auto& a : project_.network.assets) {
        const auto position = project_.positions.value(q(a.id));
        double volume = a.initial_m3;
        if (run_ && a.kind == AssetKind::tank) {
            const auto it = std::find(run_->result.tank_ids.begin(), run_->result.tank_ids.end(), a.id);
            if (it != run_->result.tank_ids.end()) volume = run_->result.volumes_m3[static_cast<std::size_t>(std::distance(run_->result.tank_ids.begin(), it))][sampleIndex()];
        }
        list.append(QVariantMap{{"id", q(a.id)}, {"label", q(a.label)}, {"kind", q(kind_name(a.kind))},
            {"x", position.x()}, {"z", position.z()}, {"capacity", a.capacity_m3}, {"volume", volume},
            {"fill", a.capacity_m3 > 0.0 ? std::clamp(volume / a.capacity_m3, 0.0, 1.0) : 0.0},
            {"violated", a.kind == AssetKind::tank && (volume < -1e-9 || volume > a.capacity_m3 + 1e-9)},
            {"selected", selected_id_ == q(a.id)}});
    }
    return list;
}
QVariantList ProjectController::routes() const {
    QVariantList list;
    for (const auto& r : project_.network.routes) {
        const auto s = project_.positions.value(q(r.source_id)), d = project_.positions.value(q(r.target_id));
        double rate = r.setpoint_m3h;
        for (const auto& a : project_.network.assets) if (a.id == r.source_id && a.kind == AssetKind::source) rate = a.inflow_m3h - a.curtailment_m3h;
        list.append(QVariantMap{{"id", q(r.id)}, {"source", q(r.source_id)}, {"target", q(r.target_id)}, {"sx", s.x()}, {"sz", s.z()}, {"tx", d.x()}, {"tz", d.z()},
            {"flow", rate}, {"pump", r.has_pump}, {"available", r.available}, {"selected", selected_id_ == q(r.id)}});
    }
    return list;
}
QVariantMap ProjectController::selected() const {
    for (const auto& a : project_.network.assets) if (q(a.id) == selected_id_) return {
        {"id", q(a.id)}, {"label", q(a.label)}, {"kind", q(kind_name(a.kind))}, {"capacity", a.capacity_m3},
        {"initial", a.initial_m3}, {"inflow", a.inflow_m3h}, {"curtailment", a.curtailment_m3h},
        {"curtailment_max", a.curtailment_max_m3h}, {"cost", a.cost_per_m3}};
    for (const auto& r : project_.network.routes) if (q(r.id) == selected_id_) {
        bool source_driven = false;
        for (const auto& a : project_.network.assets) if (a.id == r.source_id) source_driven = a.kind == AssetKind::source;
        return {{"id", q(r.id)}, {"label", q(r.id)}, {"kind", "route"}, {"source", q(r.source_id)}, {"target", q(r.target_id)},
            {"capacity", r.capacity_m3h}, {"flow", r.setpoint_m3h}, {"cost", r.cost_per_m3}, {"pump", r.has_pump}, {"available", r.available}, {"sourceDriven", source_driven}};
    }
    return {};
}
QStringList ProjectController::draftIssues() const {
    QStringList out;
    for (const auto& i : inspect_network(project_.network)) out.append(q(i.asset_id + ": " + i.message));
    return out;
}
void ProjectController::refresh(bool physical) {
    if (physical) {
        ++revision_; ++job_generation_; run_.reset(); approved_ = false; busy_ = false; forecast_fraction_ = 0.0;
        summary_ = "Numerical revision changed. Run a new forecast; previous approval is cleared.";
    }
    emit changed();
}
bool ProjectController::edit(const std::function<void()>& mutation, bool requireFreshForecast) {
    const auto before = editorDocument(); const auto previous_hash = networkHash();
    const auto previousSensorHash = sensorConfigurationHash();
    const auto previousProject = project_; const auto previousConfiguration = sensor_configuration_, previousLayout = sensor_layout_;
    const auto previousSelection = selected_id_, previousSensorSelection = selected_sensor_id_;
    try {
        mutation();
        // Stored markers are world-space presentation coordinates. Follow layout-only
        // attachment movement while retaining their configured visual offset.
        for (const auto& value : sensor_configuration_.value("sensors").toArray()) {
            const auto sensor = value.toObject(); const auto id = sensor.value("sensor_id").toString();
            if (!previousLayout.contains(id) || sensor_layout_.value(id) != previousLayout.value(id)) continue;
            const auto oldAnchor = telemetry::sensor_anchor(previousProject, sensor), newAnchor = telemetry::sensor_anchor(project_, sensor);
            auto marker = sensor_layout_.value(id).toObject();
            for (const auto* axis : {"x", "y", "z"}) marker[axis] = marker.value(axis).toDouble() + newAnchor.value(axis).toDouble() - oldAnchor.value(axis).toDouble();
            sensor_layout_[id] = marker;
        }
        synchronizeSensorConfiguration();
        if (!sensor_configuration_.isEmpty() && sensorConfigurationHash() != previousSensorHash)
            sensor_configuration_["revision"] = previousConfiguration.isEmpty() ? 1.0 : previousConfiguration.value("revision").toDouble() + 1.0;
        telemetry::validate_sensor_project(project_, sensor_configuration_, sensor_layout_);
    } catch (const std::exception& e) {
        project_ = previousProject; sensor_configuration_ = previousConfiguration; sensor_layout_ = previousLayout;
        selected_id_ = previousSelection; selected_sensor_id_ = previousSensorSelection;
        reportAction("edit.rejected", "Property needs attention", QString::fromUtf8(e.what()), {selected_id_}); return false;
    }
    clearActionFeedback();
    if (editorDocument() == before) { emit changed(); return true; }
    undo_.push_back(before); if (undo_.size() > 64) undo_.erase(undo_.begin()); redo_.clear();
    refresh(requireFreshForecast || networkHash() != previous_hash);
    if (sensorConfigurationHash() != previousSensorHash) emit sensorConfigurationChanged();
    return true;
}
bool ProjectController::restore(const QJsonObject& object) {
    try {
        const bool wrapper = object.value("version") == 2;
        const auto parsed = parse_project(wrapper ? object.value("project").toObject() : object);
        const auto configuration = wrapper ? object.value("sensor_configuration").toObject() : QJsonObject{};
        const auto layout = wrapper ? object.value("sensor_layout").toObject() : QJsonObject{};
        if (wrapper) telemetry::validate_document(object, "project_v2", telemetry::sensor_project_context(parsed));
        telemetry::validate_sensor_project(parsed, configuration, layout);
        const auto previous_hash = networkHash(), previousSensorHash = sensorConfigurationHash();
        project_ = parsed; sensor_configuration_ = configuration; sensor_layout_ = layout;
        clearActionFeedback();
        selected_id_.clear(); selected_sensor_id_.clear(); refresh(networkHash() != previous_hash); emit cameraChanged();
        if (sensorConfigurationHash() != previousSensorHash) emit sensorConfigurationChanged();
        return true;
    } catch (const std::exception& e) { summary_ = QString::fromUtf8(e.what()); emit changed(); return false; }
}
void ProjectController::select(const QString& id) { selected_id_ = id; emit changed(); }
void ProjectController::addAsset(const QString& kind_string, double x, double z) {
    const auto kind = parse_kind(kind_string.toStdString());
    if (!kind) { reportAction("equipment.unsupported_kind", "Choose supported equipment", "Use the equipment palette. Pumps attach to routes leaving a storage tank."); return; }
    if (project_.network.assets.size() >= 128) { reportAction("equipment.limit", "Equipment limit reached", "This editor supports 128 assets. Remove unused equipment before adding another asset."); return; }
    if (!std::isfinite(x) || !std::isfinite(z) || std::abs(x) > 10000.0 || std::abs(z) > 10000.0) {
        reportAction("placement.out_of_range", "Choose a position in the work area", "Use a finite position within the existing 10,000-unit layout bounds. No equipment was added.", {}, "position"); return;
    }
    edit([&] {
        Asset a; a.id = fresh_id("asset_"); a.kind = *kind; a.label = kind_name(*kind);
        if (*kind == AssetKind::tank) { a.capacity_m3 = 120.0; a.initial_m3 = 0.0; }
        if (*kind == AssetKind::source) { a.inflow_m3h = 52.0; a.curtailment_max_m3h = 15.0; a.cost_per_m3 = 250.0; }
        if (*kind == AssetKind::treatment) a.capacity_m3 = 60.0;
        if (*kind == AssetKind::trucking) { a.capacity_m3 = 22.0; a.cost_per_m3 = 80.0; }
        project_.positions[q(a.id)] = QVector3D(static_cast<float>(std::round(x / 20.0) * 20.0), 0, static_cast<float>(std::round(z / 20.0) * 20.0));
        project_.network.assets.push_back(a); selected_id_ = q(a.id);
    });
}
void ProjectController::moveAsset(const QString& id, double x, double z) {
    if (!project_.positions.contains(id)) { reportAction("placement.missing_target", "Target no longer exists", "Choose existing equipment before moving it.", {id}); return; }
    if (!std::isfinite(x) || !std::isfinite(z) || std::abs(x) > 10000.0 || std::abs(z) > 10000.0) {
        reportAction("placement.out_of_range", "Choose a position in the work area", "Use a finite position within the existing 10,000-unit layout bounds. The layout is unchanged.", {id}, "position"); return;
    }
    edit([&] { project_.positions[id] = QVector3D(static_cast<float>(std::round(x / 20.0) * 20.0), 0, static_cast<float>(std::round(z / 20.0) * 20.0)); });
}
void ProjectController::connectAssets(const QString& source, const QString& target) {
    if (project_.network.routes.size() >= 256) { reportAction("connection.limit", "Route limit reached", "This editor supports 256 routes. Remove an unused route before adding another.", {source, target}); return; }
    const Asset* s = nullptr; const Asset* d = nullptr;
    for (const auto& a : project_.network.assets) { if (q(a.id) == source) s = &a; if (q(a.id) == target) d = &a; }
    if (!s || !d) { reportAction("connection.missing_target", "Target no longer exists", "Choose existing source and destination equipment. No route was added.", {source, target}); return; }
    if (source == target) { reportAction("connection.self", "Choose a different destination", "An asset cannot connect to itself. No route was added.", {source}); return; }
    if (!((s->kind == AssetKind::source && d->kind == AssetKind::tank) ||
        (s->kind == AssetKind::tank && d->kind != AssetKind::source))) {
        reportAction("connection.unsupported_types", "Connection not supported",
            "Connect source -> tank, or tank -> tank/treatment/trucking. These are the supported connections in this model; no route was added.", {source, target}); return;
    }
    for (const auto& r : project_.network.routes) if (r.source_id == s->id && s->kind == AssetKind::source) {
        reportAction("connection.source_already_connected", "Source already connected", "Each source feeds exactly one tank. Delete its existing route first; no connection was replaced.", {source, target}); return;
    }
    edit([&] {
        Route r; r.id = fresh_id("route_"); r.source_id = source.toStdString(); r.target_id = target.toStdString();
        r.capacity_m3h = s->kind == AssetKind::source ? s->inflow_m3h : 25.0;
        r.setpoint_m3h = s->kind == AssetKind::source ? s->inflow_m3h - s->curtailment_m3h : 0.0;
        project_.network.routes.push_back(r); selected_id_ = q(r.id);
    });
}
void ProjectController::attachPump(const QString& id) {
    const auto route = std::find_if(project_.network.routes.cbegin(), project_.network.routes.cend(), [&](const Route& r) { return q(r.id) == id; });
    if (route == project_.network.routes.cend()) {
        reportAction("pump.choose_route", "Choose a pipe route", "Attach the pump to a route that starts at a storage tank. No pump was attached.", id.isEmpty() ? QStringList{} : QStringList{id}); return;
    }
    const auto source = std::find_if(project_.network.assets.cbegin(), project_.network.assets.cend(), [&](const Asset& a) { return a.id == route->source_id; });
    if (source == project_.network.assets.cend() || source->kind != AssetKind::tank) {
        reportAction("pump.source_feed", "Pump cannot attach here", "This route is source-driven. Choose a route leaving a storage tank. No pump was attached.", {id}); return;
    }
    if (route->has_pump) { selected_id_ = id; reportAction("pump.already_attached", "Pump already attached", "This route already has a pump. No second pump was created.", {id}, {}, "info"); return; }
    edit([&] {
        for (auto& r : project_.network.routes) if (q(r.id) == id)
            for (const auto& a : project_.network.assets) if (a.id == r.source_id && a.kind == AssetKind::tank) { r.has_pump = true; selected_id_ = id; }
    });
}
void ProjectController::editSelected(const QString& field, const QString& value) {
    if (selected().isEmpty()) { reportAction("property.missing_target", "Select existing equipment", "The edited item no longer exists. Choose a new selection; the document is unchanged.", {selected_id_}, field); return; }
    bool numeric_ok = false; const double number = value.toDouble(&numeric_ok);
    if (field != "label" && field != "available" && (!numeric_ok || !std::isfinite(number) || number < 0.0)) {
        reportAction("property.invalid_number", "Enter a valid number", "Enter a finite, nonnegative value in the field's displayed units. The accepted model value is unchanged.", {selected_id_}, field); return;
    }
    edit([&] {
        for (auto& a : project_.network.assets) if (q(a.id) == selected_id_) {
            if (field == "label") a.label = value.left(256).toStdString();
            else if (field == "capacity") a.capacity_m3 = number;
            else if (field == "initial") a.initial_m3 = number;
            else if (field == "inflow") a.inflow_m3h = number;
            else if (field == "curtailment") a.curtailment_m3h = number;
            else if (field == "curtailment_max") a.curtailment_max_m3h = number;
            else if (field == "cost") a.cost_per_m3 = number;
        }
        for (auto& r : project_.network.routes) if (q(r.id) == selected_id_) {
            if (field == "capacity") r.capacity_m3h = number;
            else if (field == "flow") r.setpoint_m3h = number;
            else if (field == "cost") r.cost_per_m3 = number;
            else if (field == "available" && r.has_pump) { r.available = value == "true"; if (!r.available) r.setpoint_m3h = 0.0; }
        }
    });
}
void ProjectController::deleteSelected() {
    if (selected().isEmpty()) { reportAction("delete.missing_selection", "Select equipment or a route", "Choose an existing item before deleting. The document is unchanged."); return; }
    const auto id = selected_id_.toStdString();
    QSet<QString> removed{selected_id_};
    for (const auto& route : project_.network.routes)
        if (route.source_id == id || route.target_id == id) removed.insert(q(route.id));
    for (const auto& value : sensor_configuration_.value("sensors").toArray()) {
        const auto sensor = value.toObject();
        if (removed.contains(sensor.value("attachment").toObject().value("id").toString())) {
            reportAction("delete.sensor_attached", "Sensor still attached", "Remove or reattach sensor " + sensor.value("label").toString() + " before deleting its equipment or route.", {selected_id_}); return;
        }
    }
    edit([&] {
        std::erase_if(project_.network.assets, [&](const Asset& a) { return a.id == id; });
        std::erase_if(project_.network.routes, [&](const Route& r) { return r.id == id || r.source_id == id || r.target_id == id; });
        for (auto it = project_.pipe_parameters.begin(); it != project_.pipe_parameters.end();) {
            const bool exists = std::any_of(project_.network.routes.begin(), project_.network.routes.end(), [&](const Route& r) { return q(r.id) == it.key(); });
            if (!exists) it = project_.pipe_parameters.erase(it); else ++it;
        }
        project_.positions.remove(selected_id_); selected_id_.clear();
    });
}
void ProjectController::failSelectedPump() {
    const auto route = std::find_if(project_.network.routes.cbegin(), project_.network.routes.cend(), [&](const Route& r) { return q(r.id) == selected_id_; });
    if (route == project_.network.routes.cend() || !route->has_pump || !route->available) {
        reportAction("pump.not_available", "Select an available pump", "Choose an available route-mounted pump. No different scenario or equipment was changed.", {selected_id_}); return;
    }
    try { const auto n = main_pump_failure(project_.network, selected_id_.toStdString()); edit([&] { project_.network = n; }); }
    catch (const std::exception& e) { reportAction("pump.not_available", "Select an available pump", QString::fromUtf8(e.what()), {selected_id_}); }
}
void ProjectController::undo() {
    if (undo_.empty()) return;
    const auto current = editorDocument(), previous = undo_.back(); undo_.pop_back(); redo_.push_back(current); restore(previous);
}
void ProjectController::redo() {
    if (redo_.empty()) return;
    const auto current = editorDocument(), next = redo_.back(); redo_.pop_back(); undo_.push_back(current); restore(next);
}
void ProjectController::reset() {
    clearActionFeedback();
    const auto previousSensorHash = sensorConfigurationHash();
    project_ = example_project(); sensor_configuration_ = {}; sensor_layout_ = {}; selected_sensor_id_.clear();
    selected_id_.clear(); undo_.clear(); redo_.clear(); refresh(true); emit cameraChanged();
    if (sensorConfigurationHash() != previousSensorHash) emit sensorConfigurationChanged();
    summary_ = "Healthy 3 h preview: main 44, backup 16 m3/h. Tank A accumulates 8 m3/h."; emit changed();
}
void ProjectController::loadExample(const QString& name) {
    try {
        auto p = example_project();
        // Presets change operating conditions, not declared pipe measurement geometry.
        for (auto it = project_.pipe_parameters.cbegin(); it != project_.pipe_parameters.cend(); ++it)
            for (const auto& route : p.network.routes) if (q(route.id) == it.key()) p.pipe_parameters[it.key()] = it.value();
        if (name != "healthy") p.network = compile(main_pump_failure(p.network), incident_controls(name.toStdString())).network;
        edit([&] { project_ = p; selected_id_.clear(); }); emit cameraChanged();
    } catch (const std::exception& e) { summary_ = QString::fromUtf8(e.what()); emit changed(); }
}
bool ProjectController::save(const QUrl& url) {
    if (!url.isLocalFile()) { summary_ = "Choose a local JSON project file."; emit changed(); return false; }
    QSaveFile file(url.toLocalFile());
    if (!file.open(QIODevice::WriteOnly)) { summary_ = file.errorString(); emit changed(); return false; }
    const auto bytes = QJsonDocument(editorDocument()).toJson(QJsonDocument::Indented);
    if (file.write(bytes) != bytes.size() || !file.commit()) { summary_ = file.errorString(); emit changed(); return false; }
    summary_ = "Project saved."; emit changed(); return true;
}
bool ProjectController::load(const QUrl& url) {
    QFile file(url.toLocalFile());
    if (!url.isLocalFile() || !file.open(QIODevice::ReadOnly) || file.size() > 2 * 1024 * 1024) { summary_ = "Cannot read local project (limit 2 MiB)."; emit changed(); return false; }
    QJsonObject object;
    try { object = telemetry::decode_document(file.readAll(), 2 * 1024 * 1024); }
    catch (const std::exception& e) { summary_ = QString::fromUtf8(e.what()); emit changed(); return false; }
    const auto before = editorDocument();
    if (!restore(object)) return false;
    undo_.push_back(before); if (undo_.size() > 64) undo_.erase(undo_.begin()); redo_.clear(); emit changed(); return true;
}
void ProjectController::forecast() {
    forecastWithConfig({});
}
void ProjectController::forecastWithConfig(SimulationConfig configuration) {
    if (busy_) return;
    if (!draftIssues().empty()) { reportAction("forecast.incomplete_draft", "Network needs attention", "Fix draft issues before forecasting. Editing and draft saving remain available."); return; }
    busy_ = true; run_.reset(); approved_ = false; summary_ = "Calculating native forecast..."; emit changed();
    const auto generation = ++job_generation_; const auto revision = revision_; const auto network = project_.network;
    const auto physicalHash = networkHash();
    auto* watcher = new QFutureWatcher<JobResult>(this);
    connect(watcher, &QFutureWatcher<JobResult>::finished, this, [this, watcher, generation, revision, network, configuration, physicalHash] {
        auto outcome = watcher->result(); watcher->deleteLater();
        if (generation != job_generation_ || revision != revision_) return;
        busy_ = false;
        if (!outcome.error.isEmpty()) { summary_ = outcome.error; emit changed(); return; }
        run_ = std::make_shared<const CapturedForecast>(CapturedForecast{network, configuration,
            std::move(outcome.result), std::move(outcome.validation),
            QUuid::createUuid().toString(QUuid::WithoutBraces), physicalHash, revision});
        forecast_fraction_ = 0.0;
        summary_ = QString("%1 | Nominal %2 h volume forecast | Cost %3 synthetic units | Quality not evaluated\n")
            .arg(run_->validation.feasible ? "PASS" : "FAIL").arg(configuration.horizon_h).arg(run_->result.cost, 0, 'f', 2);
        for (const auto& issue : run_->validation.violations)
            summary_ += q(issue.asset_id + ": " + issue.message) + (issue.time_h ? QString(" Limit reached at %1 h.").arg(*issue.time_h, 0, 'f', 5) : QString{}) + "\n";
        if (run_->validation.feasible) summary_ += "Remaining minimum headroom: " + QString::number(run_->validation.minimum_headroom_m3, 'f', 2) + " m3.";
        emit changed();
    });
    watcher->setFuture(QtConcurrent::run([network, configuration] {
        JobResult outcome;
        try { const auto compiled = compile(network); outcome.result = simulate(compiled, configuration); outcome.validation = validate(compiled, outcome.result, configuration); }
        catch (const std::exception& e) { outcome.error = QString::fromUtf8(e.what()); }
        return outcome;
    }));
}
void ProjectController::approve() {
    if (!canApprove()) return;
    approved_ = true; summary_ += "\nOperator approved this simulated control preview. No equipment commands are sent."; emit changed();
}
bool ProjectController::applyControlsForApproval(const std::vector<ControlValue>& controls, const QString& expectedHash) {
    if (networkHash() != expectedHash) return false;
    try {
        const auto candidate = compile(project_.network, controls).network;
        if (!edit([&] { project_.network = candidate; })) return false;
        summary_ = "Operator applied a validated simulated intervention. Computing the updated forecast.";
        emit changed();
        return true;
    } catch (const std::exception&) { return false; }
}
}

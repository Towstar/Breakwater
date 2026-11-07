#include "ProjectController.hpp"
#include <QtConcurrentRun>
#include <QFile>
#include <QFutureWatcher>
#include <QJsonDocument>
#include <QSaveFile>
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
void ProjectController::edit(const std::function<void()>& mutation) {
    const auto before = document(); const auto previous_hash = networkHash();
    mutation();
    if (document() == before) { emit changed(); return; }
    undo_.push_back(before); if (undo_.size() > 64) undo_.erase(undo_.begin()); redo_.clear();
    refresh(networkHash() != previous_hash);
}
bool ProjectController::restore(const QJsonObject& object) {
    try {
        const auto parsed = parse_project(object); const auto previous_hash = networkHash();
        project_ = parsed; selected_id_.clear(); refresh(networkHash() != previous_hash); emit cameraChanged(); return true;
    } catch (const std::exception& e) { summary_ = QString::fromUtf8(e.what()); emit changed(); return false; }
}
void ProjectController::select(const QString& id) { selected_id_ = id; emit changed(); }
void ProjectController::addAsset(const QString& kind_string, double x, double z) {
    const auto kind = parse_kind(kind_string.toStdString());
    if (!kind || project_.network.assets.size() >= 128 || !std::isfinite(x) || !std::isfinite(z) || std::abs(x) > 10000.0 || std::abs(z) > 10000.0) return;
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
    if (!project_.positions.contains(id) || !std::isfinite(x) || !std::isfinite(z) || std::abs(x) > 10000.0 || std::abs(z) > 10000.0) return;
    edit([&] { project_.positions[id] = QVector3D(static_cast<float>(std::round(x / 20.0) * 20.0), 0, static_cast<float>(std::round(z / 20.0) * 20.0)); });
}
void ProjectController::connectAssets(const QString& source, const QString& target) {
    if (project_.network.routes.size() >= 256) return;
    const Asset* s = nullptr; const Asset* d = nullptr;
    for (const auto& a : project_.network.assets) { if (q(a.id) == source) s = &a; if (q(a.id) == target) d = &a; }
    if (!s || !d || source == target || !((s->kind == AssetKind::source && d->kind == AssetKind::tank) ||
        (s->kind == AssetKind::tank && d->kind != AssetKind::source))) { summary_ = "Connect source -> tank, or tank -> tank/treatment/trucking."; emit changed(); return; }
    for (const auto& r : project_.network.routes) if (r.source_id == s->id && s->kind == AssetKind::source) {
        summary_ = "Each source feeds exactly one tank. Delete its existing route first."; emit changed(); return;
    }
    edit([&] {
        Route r; r.id = fresh_id("route_"); r.source_id = source.toStdString(); r.target_id = target.toStdString();
        r.capacity_m3h = s->kind == AssetKind::source ? s->inflow_m3h : 25.0;
        r.setpoint_m3h = s->kind == AssetKind::source ? s->inflow_m3h - s->curtailment_m3h : 0.0;
        project_.network.routes.push_back(r); selected_id_ = q(r.id);
    });
}
void ProjectController::attachPump(const QString& id) {
    edit([&] {
        for (auto& r : project_.network.routes) if (q(r.id) == id)
            for (const auto& a : project_.network.assets) if (a.id == r.source_id && a.kind == AssetKind::tank) { r.has_pump = true; selected_id_ = id; }
    });
}
void ProjectController::editSelected(const QString& field, const QString& value) {
    bool numeric_ok = false; const double number = value.toDouble(&numeric_ok);
    if (field != "label" && field != "available" && (!numeric_ok || !std::isfinite(number) || number < 0.0)) {
        summary_ = "Enter a finite, nonnegative value."; emit changed(); return;
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
    const auto id = selected_id_.toStdString();
    edit([&] {
        std::erase_if(project_.network.assets, [&](const Asset& a) { return a.id == id; });
        std::erase_if(project_.network.routes, [&](const Route& r) { return r.id == id || r.source_id == id || r.target_id == id; });
        project_.positions.remove(selected_id_); selected_id_.clear();
    });
}
void ProjectController::failSelectedPump() {
    try { const auto n = main_pump_failure(project_.network, selected_id_.toStdString()); edit([&] { project_.network = n; }); }
    catch (const std::exception& e) { summary_ = QString::fromUtf8(e.what()); emit changed(); }
}
void ProjectController::undo() {
    if (undo_.empty()) return;
    const auto current = document(), previous = undo_.back(); undo_.pop_back(); redo_.push_back(current); restore(previous);
}
void ProjectController::redo() {
    if (redo_.empty()) return;
    const auto current = document(), next = redo_.back(); redo_.pop_back(); undo_.push_back(current); restore(next);
}
void ProjectController::reset() {
    project_ = example_project(); selected_id_.clear(); undo_.clear(); redo_.clear(); refresh(true); emit cameraChanged();
    summary_ = "Healthy 3 h preview: main 44, backup 16 m3/h. Tank A accumulates 8 m3/h."; emit changed();
}
void ProjectController::loadExample(const QString& name) {
    try {
        auto p = example_project();
        if (name != "healthy") p.network = compile(main_pump_failure(p.network), incident_controls(name.toStdString())).network;
        edit([&] { project_ = p; selected_id_.clear(); }); emit cameraChanged();
    } catch (const std::exception& e) { summary_ = QString::fromUtf8(e.what()); emit changed(); }
}
bool ProjectController::save(const QUrl& url) {
    if (!url.isLocalFile()) { summary_ = "Choose a local JSON project file."; emit changed(); return false; }
    QSaveFile file(url.toLocalFile());
    if (!file.open(QIODevice::WriteOnly)) { summary_ = file.errorString(); emit changed(); return false; }
    const auto bytes = QJsonDocument(document()).toJson(QJsonDocument::Indented);
    if (file.write(bytes) != bytes.size() || !file.commit()) { summary_ = file.errorString(); emit changed(); return false; }
    summary_ = "Project saved."; emit changed(); return true;
}
bool ProjectController::load(const QUrl& url) {
    QFile file(url.toLocalFile());
    if (!url.isLocalFile() || !file.open(QIODevice::ReadOnly) || file.size() > 2 * 1024 * 1024) { summary_ = "Cannot read local project (limit 2 MiB)."; emit changed(); return false; }
    QJsonParseError error; const auto json = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !json.isObject()) { summary_ = "Invalid project JSON."; emit changed(); return false; }
    const auto before = document();
    if (!restore(json.object())) return false;
    undo_.push_back(before); redo_.clear(); emit changed(); return true;
}
void ProjectController::forecast() {
    if (busy_) return;
    if (!draftIssues().empty()) { summary_ = "Fix draft issues before forecasting."; emit changed(); return; }
    busy_ = true; approved_ = false; summary_ = "Calculating native forecast..."; emit changed();
    const auto generation = ++job_generation_; const auto revision = revision_; const auto network = project_.network;
    auto* watcher = new QFutureWatcher<JobResult>(this);
    connect(watcher, &QFutureWatcher<JobResult>::finished, this, [this, watcher, generation, revision] {
        const auto outcome = watcher->result(); watcher->deleteLater();
        if (generation != job_generation_ || revision != revision_) return;
        busy_ = false;
        if (!outcome.error.isEmpty()) { summary_ = outcome.error; emit changed(); return; }
        run_ = StoredRun{outcome.result, outcome.validation, QUuid::createUuid().toString(QUuid::WithoutBraces), revision};
        forecast_fraction_ = 0.0;
        summary_ = QString("%1 | Nominal 3 h volume forecast | Cost %2 synthetic units | Quality not evaluated\n")
            .arg(outcome.validation.feasible ? "PASS" : "FAIL").arg(outcome.result.cost, 0, 'f', 2);
        for (const auto& issue : outcome.validation.violations)
            summary_ += q(issue.asset_id + ": " + issue.message) + (issue.time_h ? QString(" Limit reached at %1 h.").arg(*issue.time_h, 0, 'f', 5) : QString{}) + "\n";
        if (outcome.validation.feasible) summary_ += "Remaining minimum headroom: " + QString::number(outcome.validation.minimum_headroom_m3, 'f', 2) + " m3.";
        emit changed();
    });
    watcher->setFuture(QtConcurrent::run([network] {
        JobResult outcome;
        try { const auto compiled = compile(network); outcome.result = simulate(compiled); outcome.validation = validate(compiled, outcome.result); }
        catch (const std::exception& e) { outcome.error = QString::fromUtf8(e.what()); }
        return outcome;
    }));
}
void ProjectController::approve() {
    if (!canApprove()) return;
    approved_ = true; summary_ += "\nOperator approved this simulated control preview. No equipment commands are sent."; emit changed();
}
}

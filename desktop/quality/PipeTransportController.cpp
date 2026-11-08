#include "PipeTransportController.hpp"
#include "PipeTransportEvidence.hpp"
#include <QDateTime>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QJsonDocument>
#include <QSaveFile>
#include <QUuid>
#include <QtConcurrentRun>
#include <algorithm>
#include <cmath>

namespace breakwater::desktop::quality {
namespace {
QJsonObject readEvidence(const QString& path) {
    const QFileInfo info(path);
    if (!info.isFile() || info.size() <= 0 || info.size() > maximumPipeEvidenceBytes)
        return {{"error", "Choose a native transport JSON file no larger than 1 MiB."}};
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {{"error", "Cannot open native transport evidence."}};
    const auto bytes = file.read(maximumPipeEvidenceBytes + 1);
    if (file.error() != QFileDevice::NoError || bytes.size() > maximumPipeEvidenceBytes || !file.atEnd())
        return {{"error", "Cannot read native transport evidence within the 1 MiB limit."}};
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(bytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return {{"error", "Native transport evidence must be a valid JSON object."}};
    QString error;
    if (!validate_pipe_transport_evidence(document.object(), &error)) return {{"error", error}};
    return {{"evidence", document.object()}};
}
std::vector<std::size_t> sampleIndices(std::size_t size, std::size_t maximum) {
    const auto count = std::min(size, maximum);
    std::vector<std::size_t> result;
    for (std::size_t i = 0; i < count; ++i) result.push_back(count == 1 ? 0 : i * (size - 1) / (count - 1));
    return result;
}
QJsonObject calculate(const QJsonObject& context, const QString& routeId, const PipeParameters& parameters, quint64 revision) {
    const auto project = parse_project({{"version", source_allocation_project_version}, {"network", context.value("network")}, {"layout", QJsonObject{}}});
    std::vector<ControlValue> controls;
    for (const auto& value : context.value("controls").toArray()) {
        const auto item = value.toObject();
        controls.push_back({item.value("control_id").toString().toStdString(), item.value("value").toDouble()});
    }
    const auto compiled = compile(project.network, controls);
    const auto route = std::find_if(compiled.network.routes.begin(), compiled.network.routes.end(), [&](const Route& r) { return QString::fromStdString(r.id) == routeId; });
    if (route == compiled.network.routes.end()) throw std::invalid_argument("The captured route no longer exists.");
    const double flow = route->available ? route_flow_m3h(compiled.network,*route) : 0.0;
    TransportConfig configuration;
    configuration.horizon_h = context.value("horizon_h").toDouble();
    configuration.output_step_h = parameters.output_step_h;
    configuration.cfl_safety = parameters.cfl_safety;
    configuration.boundary = parameters.boundary;
    const auto native = transport(parameters.pipe, std::vector<double>(parameters.pipe.cells, parameters.initial_mg_l),
        std::vector<FlowKnot>{{0.0, flow}}, std::vector<InletKnot>{{0.0, parameters.inlet_mg_l}}, configuration,
        std::vector<double>(parameters.pipe.cells, parameters.source_mg_l_h));
    const auto frames = sampleIndices(native.time_h.size(), 256), cells = sampleIndices(native.x_m.size(), 128);
    QJsonArray times, positions, timeIndices, cellIndices, field, outlet, inventory, inletMass, outletMass, sourceMass, decayMass, residual;
    for (const auto index : cells) { positions.append(native.x_m[index]); cellIndices.append(static_cast<qint64>(index)); }
    for (const auto index : frames) {
        timeIndices.append(static_cast<qint64>(index));
        times.append(native.time_h[index]);
        QJsonArray row;
        for (const auto cell : cells) row.append(native.concentration_mg_l[index][cell]);
        field.append(row); outlet.append(native.concentration_mg_l[index].back());
        inventory.append(native.inventory_mg[index]); inletMass.append(native.cumulative_inlet_mg[index]);
        outletMass.append(native.cumulative_outlet_mg[index]); sourceMass.append(native.cumulative_source_mg[index]);
        decayMass.append(native.cumulative_decay_mg[index]); residual.append(native.mass_balance_residual_mg[index]);
    }
    return {{"result_version", 1}, {"result_id", QUuid::createUuid().toString(QUuid::WithoutBraces)},
        {"created_at", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
        {"route_id", routeId}, {"run_id", context.value("run_id")}, {"volume_run_id", context.value("run_id")}, {"snapshot_id", context.value("snapshot_id")},
        {"physical_hash", context.value("physical_hash")}, {"parameter_hash", pipe_parameters_hash(parameters)},
        {"numerical_revision", static_cast<qint64>(revision)}, {"method", "native_pipe_transport"},
        {"method_version", QString::fromStdString(native.solver_version)}, {"quality_status", "native_scalar_reference"},
        {"horizon_h", configuration.horizon_h}, {"flow_m3h", flow}, {"historical", context.value("historical").toBool()},
        {"volume_feasible", context.value("volume_feasible")}, {"parameters", pipe_parameters_json(parameters)},
        {"preview", QJsonObject{{"time_h", times}, {"x_m", positions}, {"time_indices", timeIndices}, {"cell_indices", cellIndices}, {"concentration_mg_l", field},
            {"outlet_mg_l", outlet}, {"inventory_mg", inventory}, {"cumulative_inlet_mg", inletMass},
            {"cumulative_outlet_mg", outletMass}, {"cumulative_source_mg", sourceMass}, {"cumulative_decay_mg", decayMass},
            {"mass_balance_residual_mg", residual}, {"source_frames", static_cast<qint64>(native.time_h.size())},
            {"source_cells", static_cast<qint64>(native.x_m.size())}, {"sampling", "selected native frames and cell centers; no interpolation"}}},
        {"diagnostics", QJsonObject{{"substeps", static_cast<qint64>(native.diagnostics.substeps)},
            {"max_cfl_number", native.diagnostics.max_cfl_number}, {"minimum_concentration_mg_l", native.diagnostics.minimum_concentration_mg_l},
            {"max_mass_balance_residual_mg", native.diagnostics.max_mass_balance_residual_mg}}}};
}
}

PipeTransportController::PipeTransportController(ProjectController* project, history::HistoryController* history, QObject* parent)
    : QObject(parent), project_(project), history_(history) {
    if (project_) connect(project_, &ProjectController::changed, this, &PipeTransportController::refresh);
    if (history_) connect(history_, &history::HistoryController::changed, this, &PipeTransportController::refresh);
    refresh();
}
QJsonObject PipeTransportController::context(const QString& runId) const {
    if (!project_ || runId.isEmpty()) return {};
    return contexts_.value(runId);
}
QVariantMap PipeTransportController::parameters() const { return project_ && !route_.isEmpty() ? project_->pipeParameters(route_).toVariantMap() : QVariantMap{}; }
bool PipeTransportController::canRun() const { return !busy_ && !viewing_imported_ && !route_.isEmpty() && !context(selected_run_).isEmpty(); }
QString PipeTransportController::currentBinding() const {
    if (!project_) return {};
    auto source = context(selected_run_);
    source.remove("network");
    const QJsonObject identity{{"network_hash", project_->networkHash()},
        {"revision", static_cast<qint64>(project_->simulationRevision())}, {"route_id", route_},
        {"source", source}, {"parameter_hash", pipe_parameters_hash(parse_pipe_parameters(project_->pipeParameters(route_)))}};
    return QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(identity).toJson(QJsonDocument::Compact), QCryptographicHash::Sha256).toHex());
}
void PipeTransportController::invalidate(const QString& message) {
    ++generation_; result_.clear(); retired_status_ = message;
    if (!viewing_imported_ && !importing_)
        status_ = message + (busy_ ? " Previous native calculation is finishing." : QString{});
}
void PipeTransportController::refresh() {
    if (!project_) return;
    const auto selected = project_->selected();
    route_ = selected.value("kind").toString() == "route" ? project_->selectedId() : QString{};
    runs_.clear(); contexts_.clear();
    const auto manual = project_->capturedRun();
    const auto manualId = manual.value("run_id").toString();
    if (!manualId.isEmpty()) {
        contexts_.insert(manualId, manual);
        runs_.append(QVariantMap{{"run_id", manualId}, {"label", "Current forecast · " + manualId.left(8)}, {"historical", false}});
    }
    const auto archive = history_ ? history_->selectedArchive() : QJsonObject{};
    const auto snapshot = archive.value("snapshot").toObject();
    if (history_ && snapshot.value("physical_hash").toString() == project_->networkHash()) for (const auto& value : archive.value("runs").toArray()) {
        const auto run = value.toObject().value("run").toObject();
        const auto runId = run.value("run_id").toString();
        if (runId.isEmpty() || contexts_.contains(runId) || run.value("physical_hash") != snapshot.value("physical_hash") ||
            run.value("snapshot_id") != snapshot.value("snapshot_id") || run.value("horizon_h") != snapshot.value("horizon_h")) continue;
        contexts_.insert(runId, {{"run_id", runId}, {"snapshot_id", snapshot.value("snapshot_id")}, {"physical_hash", snapshot.value("physical_hash")},
            {"horizon_h", snapshot.value("horizon_h")}, {"network", snapshot.value("network")}, {"controls", run.value("controls")},
            {"historical", history_->historical()}, {"volume_feasible", run.value("validation").toObject().value("feasible")}});
        runs_.append(QVariantMap{{"run_id", runId}, {"label", (history_->historical() ? "Historical · " : "Investigation · ") + run.value("candidate_id").toString() + " · " + runId.left(8)},
            {"historical", history_->historical()}});
    }
    const auto& options = runs_;
    bool found = false;
    for (const auto& value : options) found |= value.toMap().value("run_id").toString() == selected_run_;
    if (!found) {
        selected_run_.clear();
        const auto recommended = history_ ? history_->candidateRunId() : QString{};
        for (const auto& value : options) if (value.toMap().value("run_id").toString() == recommended) selected_run_ = recommended;
        if (selected_run_.isEmpty() && !options.isEmpty()) selected_run_ = options.first().toMap().value("run_id").toString();
    }
    const auto fingerprint = currentBinding();
    if (fingerprint != binding_) {
        binding_ = fingerprint;
        invalidate(route_.isEmpty() || selected_run_.isEmpty() ? "Select a pipe and capture a volume forecast." : "Ready for native scalar transport.");
    }
    emit changed();
}
void PipeTransportController::selectRun(const QString& id) {
    if (viewing_imported_ || context(id).isEmpty() || id == selected_run_) return;
    selected_run_ = id; refresh();
}
void PipeTransportController::setParameter(const QString& field, const QString& value) {
    if (viewing_imported_ || !project_ || route_.isEmpty()) return;
    auto object = project_->pipeParameters(route_);
    if (!object.contains(field)) return;
    if (field == "boundary") object[field] = value;
    else {
        bool ok = false; const auto number = value.toDouble(&ok);
        if (!ok || !std::isfinite(number)) { status_ = "Enter a finite numeric pipe parameter."; emit changed(); return; }
        object[field] = number;
    }
    try {
        (void)parse_pipe_parameters(object);
        if (!project_->setPipeParameters(route_, object)) { status_ = "The pipe parameter update was rejected."; emit changed(); }
    } catch (const std::exception& error) { status_ = QString::fromUtf8(error.what()); emit changed(); }
}
void PipeTransportController::run() {
    if (!canRun()) return;
    const auto input = context(selected_run_);
    const auto parameters = parse_pipe_parameters(project_->pipeParameters(route_));
    const auto revision = project_->simulationRevision();
    const auto route = route_, binding = binding_;
    const auto epoch = ++generation_;
    result_.clear(); busy_ = true; retired_status_.clear(); status_ = "Calculating native scalar transport…";
    auto* watcher = new QFutureWatcher<QJsonObject>(this);
    connect(watcher, &QFutureWatcher<QJsonObject>::finished, this, [this, watcher, epoch, binding] {
        const auto outcome = watcher->result(); watcher->deleteLater(); busy_ = false;
        if (epoch == generation_ && binding == currentBinding()) {
            if (outcome.contains("error")) status_ = outcome.value("error").toString();
            else { result_ = outcome.toVariantMap(); status_ = "Native scalar reference complete. Quality is separate from volume approval."; }
        } else status_ = retired_status_.isEmpty() ? "Inputs changed; calculate again from a current captured run." : retired_status_;
        emit changed();
    });
    watcher->setFuture(QtConcurrent::run([input, parameters, route, revision] {
        try { return calculate(input, route, parameters, revision); }
        catch (const std::exception& error) { return QJsonObject{{"error", QString::fromUtf8(error.what())}}; }
        catch (...) { return QJsonObject{{"error", "The native transport calculation failed."}}; }
    }));
    emit changed();
}
void PipeTransportController::cancel() {
    if (!busy_) return;
    if (importing_) {
        ++import_generation_;
        status_ = "Import cancelled. Previous evidence is retained.";
        emit changed(); return;
    }
    invalidate("Cancelled."); emit changed();
}
void PipeTransportController::importResult(const QUrl& source) {
    if (busy_) return;
    if (!source.isLocalFile()) {
        status_ = "Choose a local native transport evidence file."; emit changed(); return;
    }
    const auto epoch = ++import_generation_;
    importing_ = busy_ = true;
    status_ = "Reading historical native transport evidence…";
    auto* watcher = new QFutureWatcher<QJsonObject>(this);
    connect(watcher, &QFutureWatcher<QJsonObject>::finished, this, [this, watcher, epoch] {
        const auto outcome = watcher->result(); watcher->deleteLater();
        importing_ = busy_ = false;
        if (epoch == import_generation_) {
            if (outcome.contains("error")) status_ = outcome.value("error").toString();
            else {
                imported_result_ = outcome.value("evidence").toObject().toVariantMap();
                viewing_imported_ = true;
                status_ = "Imported historical evidence. File contents are unverified and cannot approve a plan.";
            }
        }
        emit changed();
    });
    watcher->setFuture(QtConcurrent::run([path = source.toLocalFile()] {
        try { return readEvidence(path); }
        catch (...) { return QJsonObject{{"error", "Cannot load native transport evidence."}}; }
    }));
    emit changed();
}
void PipeTransportController::showCurrentResult() {
    if (busy_ || !viewing_imported_) return;
    viewing_imported_ = false; imported_result_.clear();
    status_ = !result_.isEmpty() ? "Native scalar reference complete. Quality is separate from volume approval."
        : canRun() ? "Ready for native scalar transport." : "Select a pipe and capture a volume forecast.";
    emit changed();
}
bool PipeTransportController::exportResult(const QUrl& destination) {
    const auto displayed = result();
    if (busy_ || displayed.isEmpty() || !destination.isLocalFile()) return false;
    const auto bytes = QJsonDocument(QJsonObject::fromVariantMap(displayed)).toJson(QJsonDocument::Compact);
    if (bytes.size() > maximumPipeEvidenceBytes) { status_ = "Transport preview exceeds the 1 MiB export limit."; emit changed(); return false; }
    QSaveFile file(destination.toLocalFile());
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        status_ = "Cannot export transport evidence."; emit changed(); return false;
    }
    status_ = viewing_imported_ ? "Imported historical evidence exported; file contents remain unverified."
        : "Native transport evidence exported."; emit changed(); return true;
}

} // namespace breakwater::desktop::quality

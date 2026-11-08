#include "PipeTransportController.hpp"
#include "PipeTransportEvidence.hpp"
#include <QDateTime>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QJsonDocument>
#include <QSaveFile>
#include <QUuid>
#include <QtConcurrentRun>
#include <algorithm>
#include <cmath>

namespace breakwater::desktop::quality {
namespace {
QJsonObject fitDecay(const QByteArray& request, const std::shared_ptr<std::atomic_bool>& cancelled) {
    const auto python = qEnvironmentVariable("BREAKWATER_PYTHON").trimmed();
    if (python.isEmpty()) return {{"status", "unavailable"}, {"message", "Configure BREAKWATER_PYTHON with PhysicsNeMo and PyTorch."}};
    auto script = QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("../share/breakwater/science/transport_calibration.py");
#if defined(BREAKWATER_CALIBRATION_PATH) && defined(BREAKWATER_DEVELOPMENT_BIN_DIR)
    if (!QFileInfo(script).isFile() && QFileInfo(QCoreApplication::applicationDirPath()).canonicalFilePath().compare(
            QFileInfo(QString::fromUtf8(BREAKWATER_DEVELOPMENT_BIN_DIR)).canonicalFilePath(), Qt::CaseInsensitive) == 0)
        script = QString::fromUtf8(BREAKWATER_CALIBRATION_PATH);
#endif
    if (!QFileInfo(script).isFile()) return {{"status", "unavailable"}, {"message", "The calibration worker is not installed."}};
    QProcess process;
    QProcessEnvironment environment;
    for (const auto* name : {"SystemRoot", "WINDIR", "PATH", "PATHEXT", "TEMP", "TMP", "USERPROFILE", "LOCALAPPDATA"})
        if (qEnvironmentVariableIsSet(name)) environment.insert(name, qEnvironmentVariable(name));
    environment.insert("PYTHONDONTWRITEBYTECODE", "1");
    environment.insert("PYTHONNOUSERSITE", "1");
    process.setProcessEnvironment(environment);
    process.start(python, {"-B", "-u", script});
    if (!process.waitForStarted(5000)) return {{"status", "unavailable"}, {"message", "The configured Python could not start."}};
    process.write(request); process.closeWriteChannel();
    QElapsedTimer timer; timer.start();
    QByteArray output; qsizetype errors = 0;
    while (process.state() != QProcess::NotRunning) {
        process.waitForFinished(100);
        output += process.readAllStandardOutput(); errors += process.readAllStandardError().size();
        if (cancelled->load() || timer.elapsed() > 45000 || output.size() > 65536 || errors > 65536) {
            process.kill(); process.waitForFinished(5000);
            return {{"status", "error"}, {"message", cancelled->load() ? "Calibration cancelled." : "Calibration exceeded its time or output budget."}};
        }
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
        return {{"status", "error"}, {"message", "The calibration worker failed."}};
    QJsonParseError parse;
    const auto document = QJsonDocument::fromJson(output, &parse);
    if (parse.error != QJsonParseError::NoError || !document.isObject())
        return {{"status", "error"}, {"message", "The calibration response was invalid."}};
    return document.object();
}
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
    const auto project = parse_project({{"version", project_version}, {"network", context.value("network")}, {"layout", QJsonObject{}}});
    std::vector<ControlValue> controls;
    for (const auto& value : context.value("controls").toArray()) {
        const auto item = value.toObject();
        controls.push_back({item.value("control_id").toString().toStdString(), item.value("value").toDouble()});
    }
    const auto compiled = compile(project.network, controls);
    const auto route = std::find_if(compiled.network.routes.begin(), compiled.network.routes.end(), [&](const Route& r) { return QString::fromStdString(r.id) == routeId; });
    if (route == compiled.network.routes.end()) throw std::invalid_argument("The captured route no longer exists.");
    double flow = route->available ? route->setpoint_m3h : 0.0;
    for (const auto& asset : compiled.network.assets)
        if (asset.id == route->source_id && asset.kind == AssetKind::source) flow = asset.inflow_m3h - asset.curtailment_m3h;
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
PipeTransportController::~PipeTransportController() {
    if (calibration_cancel_) calibration_cancel_->store(true);
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
QString PipeTransportController::calibrationBinding() const {
    if (!project_ || route_.isEmpty() || viewing_imported_) return {};
    return project_->networkHash() + ":" + QString::number(project_->simulationRevision()) + ":" + route_ + ":" +
        pipe_parameters_hash(parse_pipe_parameters(project_->pipeParameters(route_)));
}
bool PipeTransportController::canApplyCalibration() const {
    return !busy_ && !calibration_busy_ && calibration_result_.value("status").toString() == "ok" &&
        !calibration_binding_.isEmpty() && calibration_binding_ == calibrationBinding();
}
void PipeTransportController::discardCalibration() {
    calibration_result_.clear(); calibration_binding_.clear(); emit changed();
}
void PipeTransportController::cancelCalibration() {
    if (calibration_cancel_) calibration_cancel_->store(true);
}
void PipeTransportController::calibrateDecay(const QString& initial, const QString& observations) {
    if (calibration_busy_ || busy_ || calibrationBinding().isEmpty()) return;
    bool valid = false; const auto concentration = initial.toDouble(&valid);
    if (!valid || !std::isfinite(concentration) || concentration <= 0 || observations.size() > 8192) {
        calibration_status_ = "Enter a positive initial concentration and at most 64 observation rows."; emit changed(); return;
    }
    QJsonArray rows;
    const auto lines = observations.split(QRegularExpression("[\\r\\n]+"), Qt::SkipEmptyParts);
    for (const auto& line : lines) {
        const auto fields = line.trimmed().split(QRegularExpression("[,;\\s]+"), Qt::SkipEmptyParts);
        bool timeOk = false, valueOk = false;
        const auto time = fields.size() == 2 ? fields[0].toDouble(&timeOk) : 0.0;
        const auto value = fields.size() == 2 ? fields[1].toDouble(&valueOk) : 0.0;
        if (!timeOk || !valueOk || !std::isfinite(time) || !std::isfinite(value)) {
            calibration_status_ = "Each row must contain elapsed hours, concentration mg/L."; emit changed(); return;
        }
        rows.append(QJsonArray{time, value});
    }
    if (rows.size() < 3 || rows.size() > 64) {
        calibration_status_ = "Enter 3–64 observation rows."; emit changed(); return;
    }
    const auto request = QJsonDocument(QJsonObject{{"initial_mg_l", concentration}, {"observations", rows}}).toJson(QJsonDocument::Compact) + '\n';
    calibration_result_.clear(); calibration_binding_ = calibrationBinding();
    const auto binding = calibration_binding_;
    calibration_busy_ = true; calibration_status_ = "Fitting sparse observations with PhysicsNeMo (up to 45 seconds)…";
    calibration_cancel_ = std::make_shared<std::atomic_bool>(false);
    const auto cancelled = calibration_cancel_;
    auto* watcher = new QFutureWatcher<QJsonObject>(this);
    connect(watcher, &QFutureWatcher<QJsonObject>::finished, this, [this, watcher, binding, cancelled] {
        const auto outcome = watcher->result(); watcher->deleteLater(); calibration_busy_ = false; calibration_cancel_.reset();
        if (cancelled->load() || binding != calibrationBinding() || binding != calibration_binding_) {
            calibration_result_.clear(); calibration_status_ = "Calibration cancelled or inputs changed. Fit again.";
        } else {
            const auto decay = outcome.value("decay_per_h"), oracle = outcome.value("reference_decay_per_h"), rmse = outcome.value("relative_rmse");
            const bool accepted = outcome.value("status").toString() == "ok" && outcome.value("method").toString() == "physicsnemo-sparse-decay-calibration-0.1.0" &&
                decay.isDouble() && oracle.isDouble() && rmse.isDouble() && std::isfinite(decay.toDouble()) && decay.toDouble() >= 0 && decay.toDouble() < 10 &&
                std::isfinite(oracle.toDouble()) && oracle.toDouble() >= 0 && std::abs(decay.toDouble() - oracle.toDouble()) <= 1e-6 + 1e-5 * std::abs(oracle.toDouble()) &&
                std::isfinite(rmse.toDouble()) && rmse.toDouble() >= 0 && rmse.toDouble() <= 0.05;
            if (accepted) {
                calibration_result_ = outcome.toVariantMap();
                calibration_status_ = "PhysicsNeMo estimate passed the independent fit checks. Review before applying.";
            } else {
                calibration_result_.clear();
                calibration_status_ = outcome.value("message").toString("The estimate failed its independent fit checks.");
            }
        }
        emit changed();
    });
    watcher->setFuture(QtConcurrent::run([request, cancelled] { return fitDecay(request, cancelled); }));
    emit changed();
}
void PipeTransportController::applyCalibration() {
    if (!canApplyCalibration()) return;
    auto parameters = project_->pipeParameters(route_);
    parameters["decay_per_h"] = calibration_result_.value("decay_per_h").toDouble();
    if (project_->setPipeParameters(route_, parameters)) {
        calibration_status_ = "Decay applied to this pipe. Run a new native forecast and transport calculation.";
        calibration_result_.clear(); calibration_binding_.clear();
    } else calibration_status_ = "The parameter update was rejected.";
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

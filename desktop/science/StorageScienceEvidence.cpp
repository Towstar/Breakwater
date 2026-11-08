#include "StorageScienceEvidence.hpp"
#include "WorkerProtocol.hpp"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSet>
#include <QUuid>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace breakwater::desktop::science {
namespace {
void require(bool condition, const char* message = "Scientific evidence is malformed or inconsistent with its captured native run.") {
    if (!condition) throw std::invalid_argument(message);
}
void keys(const QJsonObject& object, QSet<QString> required, const QSet<QString>& optional = {}) {
    for (const auto& key : required) require(object.contains(key));
    required.unite(optional);
    for (auto it = object.begin(); it != object.end(); ++it) require(required.contains(it.key()), "Scientific evidence contains an unsupported field.");
}
double number(const QJsonValue& value) {
    require(value.isDouble() && std::isfinite(value.toDouble())); return value.toDouble();
}
qint64 integer(const QJsonValue& value, qint64 low, qint64 high) {
    const auto n = number(value);
    require(n >= static_cast<double>(low) && n <= static_cast<double>(high) && std::floor(n) == n);
    return static_cast<qint64>(n);
}
QString text(const QJsonValue& value, qsizetype limit = 256) {
    require(value.isString()); const auto result = value.toString();
    require(!result.trimmed().isEmpty() && result.size() <= limit);
    for (const auto character : result) require(character.category() != QChar::Other_Control);
    return result;
}
QJsonArray array(const QJsonValue& value, qsizetype low, qsizetype high) {
    require(value.isArray()); const auto values = value.toArray();
    require(values.size() >= low && values.size() <= high); return values;
}
QJsonArray numbers(const std::vector<double>& values) {
    QJsonArray result; for (const auto value : values) result.append(value); return result;
}
QJsonObject validation(const ValidationReport& report) {
    QJsonArray issues, limits;
    for (const auto& issue : report.violations) {
        QJsonObject value{{"code", QString::fromStdString(issue.code)}, {"asset_id", QString::fromStdString(issue.asset_id)},
            {"message", QString::fromStdString(issue.message)}};
        if (issue.time_h) value["time_h"] = *issue.time_h;
        issues.append(value);
    }
    for (const auto& limit : report.limits)
        limits.append(QJsonObject{{"asset_id", QString::fromStdString(limit.asset_id)}, {"limit", QString::fromStdString(limit.limit)}, {"time_h", limit.time_h}});
    return {{"feasible", report.feasible}, {"violations", issues}, {"limits", limits},
        {"minimum_headroom_m3", report.minimum_headroom_m3}, {"max_conservation_residual_m3", report.max_conservation_residual_m3}};
}
double scaledError(double left, double right, double& absolute) {
    absolute = std::abs(left - right); require(std::isfinite(absolute));
    return absolute / (1e-9 + 1e-10 * std::max(std::abs(left), std::abs(right)));
}
void checkGrid(const QJsonArray& times, double horizon, double step) {
    require(horizon > 0 && horizon <= 24 && step > 0);
    require(times.size() >= 2 && times.size() <= 10001 && number(times[0]) == 0.0);
    qsizetype index = 1;
    double previous = 0.0;
    for (qsizetype update = 1; update <= 10000; ++update) {
        const double interior = static_cast<double>(update) * step;
        if (interior >= horizon) {
            require(index == times.size() - 1 && number(times[index]) == horizon && horizon > previous); return;
        }
        require(index < times.size() - 1 && interior > previous && number(times[index]) == interior);
        previous = interior; ++index;
    }
    require(false, "Scientific evidence exceeds the native time-grid budget.");
}
}

QJsonObject storage_science_archive(const CapturedForecast& capture, const QJsonObject& evidence) {
    const auto id = evidence.value("result_id").toString(), created = evidence.value("created_at").toString();
    QJsonArray tankIds, tankSummaries, volumes;
    for (std::size_t i = 0; i < capture.result.tank_ids.size(); ++i) {
        const auto tank = QString::fromStdString(capture.result.tank_ids[i]);
        tankIds.append(tank); const auto& samples = capture.result.volumes_m3.at(i);
        require(!samples.empty()); volumes.append(numbers(samples));
        tankSummaries.append(QJsonObject{{"asset_id", tank}, {"initial_m3", samples.front()}, {"final_m3", samples.back()}});
    }
    const char* solver = capture.result.solver == Solver::dense_reference ? "dense_reference" :
        capture.result.solver == Solver::affine ? "affine" : "sparse";
    const QJsonObject run{{"run_id", capture.id}, {"snapshot_id", capture.id}, {"physical_hash", capture.physical_hash},
        {"numerical_revision", static_cast<qint64>(capture.revision)},
        {"candidate_id", "baseline"}, {"created_at", created}, {"horizon_h", capture.config.horizon_h},
        {"sample_step_h", capture.config.step_h}, {"cost", capture.result.cost}, {"cost_unit", "synthetic units"},
        {"controls", QJsonArray{}}, {"tanks", tankSummaries}, {"solver", solver}, {"solver_version", QString::fromLatin1(solver_version)},
        {"validation_status", "not_requested"}, {"validation", validation(capture.validation)}};
    const QJsonObject details{{"ok", true}, {"run", run}, {"time_h", numbers(capture.result.time_h)},
        {"tank_ids", tankIds}, {"volumes_m3", volumes}};
    QJsonObject archive{{"archive_version", 1}, {"session_id", id}, {"session_kind", "scientific"}, {"checkpoint_sequence", 1},
        {"created_at", created}, {"updated_at", created}, {"status", "completed"},
        {"objective", evidence.value("analysis_mode").toString() == "sensitivities" ? "Captured-control PyTorch sensitivities" : "Native/PyTorch storage comparison"},
        {"snapshot", QJsonObject{{"snapshot_id", capture.id}, {"physical_hash", capture.physical_hash},
            {"numerical_revision", static_cast<qint64>(capture.revision)},
            {"horizon_h", capture.config.horizon_h}, {"network", network_json(capture.network)}}},
        {"model", QJsonObject{}}, {"runs", QJsonArray{details}}, {"events", QJsonArray{}},
        {"recommendation", QJsonObject{}}, {"approvals", QJsonArray{}}, {"actionable", false},
        {"scientific_evidence_version", 1}, {"scientific_evidence", QJsonArray{evidence}}};
    QString error;
    require(validate_storage_science_evidence(evidence, archive, &error), "Completed scientific evidence could not be archived.");
    return archive;
}

bool validate_storage_science_evidence(const QJsonObject& evidence, const QJsonObject& archive, QString* error) {
    if (error) error->clear();
    try {
        require(QJsonDocument(evidence).toJson(QJsonDocument::Compact).size() <= maximumStorageScienceEvidenceBytes,
            "Scientific evidence exceeds the 1 MiB limit.");
        require(integer(archive.value("scientific_evidence_version"), 1, 1) == 1 &&
            archive.value("scientific_evidence").isArray() && archive.value("scientific_evidence").toArray() == QJsonArray{evidence});
        keys(evidence, {"result_version", "result_id", "created_at", "method", "run_id", "snapshot_id", "physical_hash",
            "numerical_revision", "horizon_h", "step_h", "analysis_mode", "selected_controls", "native_solver_version",
            "native_cost", "worker_cost", "cost_difference", "cost_absolute_error", "cost_scaled_error", "within_tolerance",
            "comparison_scope", "absolute_tolerance", "relative_tolerance", "native_volume_feasible", "tanks", "tank_errors",
            "provenance", "warnings", "preview"}, {"derivatives"});
        const auto resultVersion = integer(evidence.value("result_version"), 1, 2);
        require(evidence.value("method") == "storage_rollout");
        const auto resultId = text(evidence.value("result_id")); require(!QUuid::fromString(resultId).isNull());
        const auto created = text(evidence.value("created_at"), 40);
        require(created.endsWith('Z') && QDateTime::fromString(created, Qt::ISODateWithMs).isValid());
        require(archive.value("session_kind") == "scientific" && archive.value("session_id") == resultId &&
            integer(archive.value("checkpoint_sequence"), 1, 1) == 1 && archive.value("status") == "completed" &&
            archive.value("created_at") == created && archive.value("updated_at") == created &&
            archive.value("actionable").isBool() && !archive.value("actionable").toBool());
        require(archive.value("events").isArray() && archive.value("events").toArray().isEmpty() &&
            archive.value("approvals").isArray() && archive.value("approvals").toArray().isEmpty() &&
            archive.value("model").isObject() && archive.value("model").toObject().isEmpty() && archive.value("recommendation").isObject());
        const auto recommendation = archive.value("recommendation").toObject();
        require(recommendation.isEmpty() || recommendation == QJsonObject{{"actionable", false}, {"approval_status", "historical"}});
        const auto runId = text(evidence.value("run_id")), snapshotId = text(evidence.value("snapshot_id"));
        require(runId == snapshotId);
        const auto hash = text(evidence.value("physical_hash"), 64);
        static const QRegularExpression hashPattern(QStringLiteral("^[0-9a-f]{64}$")); require(hashPattern.match(hash).hasMatch());
        if (archive.contains("agent_origin")) {
            require(archive.value("agent_origin").isObject()); const auto origin = archive.value("agent_origin").toObject();
            keys(origin, {"incident_snapshot_id", "incident_physical_hash", "run_id", "effective_physical_hash",
                "agent_session_id", "tool_call_id"});
            require(!QUuid::fromString(text(origin.value("incident_snapshot_id"))).isNull() &&
                !QUuid::fromString(text(origin.value("agent_session_id"))).isNull());
            require(hashPattern.match(text(origin.value("incident_physical_hash"), 64)).hasMatch());
            require(origin.value("run_id") == runId && origin.value("effective_physical_hash") == hash &&
                evidence.value("analysis_mode") == "sensitivities");
            (void)text(origin.value("tool_call_id"));
        }
        const auto revision = integer(evidence.value("numerical_revision"), 0, 9007199254740991LL);
        const auto horizon = number(evidence.value("horizon_h")), step = number(evidence.value("step_h"));
        require(evidence.value("native_solver_version") == QString::fromLatin1(solver_version) ||
            (resultVersion == 1 && evidence.value("native_solver_version") == "constant-flow-0.1.1"));
        require(evidence.value("comparison_scope") == "Returned worker preview samples versus their original native indices; not a feasibility verdict" &&
            number(evidence.value("absolute_tolerance")) == 1e-9 && number(evidence.value("relative_tolerance")) == 1e-10);
        const auto mode = evidence.value("analysis_mode").toString(); const bool derivatives = mode == "sensitivities";
        require(derivatives || mode == "forward"); require(derivatives == evidence.contains("derivatives"));
        require(archive.value("snapshot").isObject()); const auto snapshot = archive.value("snapshot").toObject();
        require(snapshot.value("snapshot_id") == snapshotId && snapshot.value("physical_hash") == hash && number(snapshot.value("horizon_h")) == horizon &&
            integer(snapshot.value("numerical_revision"), 0, 9007199254740991LL) == revision);
        const auto project = parse_project({{"version", resultVersion}, {"network", snapshot.value("network")}, {"layout", QJsonObject{}}});
        require(resultVersion == (requires_source_allocations(project.network) ? 2 : 1));
        require(physics_hash(project) == hash);
        QJsonObject details;
        for (const auto& candidate : array(archive.value("runs"), 1, 1)) {
            require(candidate.isObject()); const auto object = candidate.toObject();
            if (object.value("run").toObject().value("run_id") == runId) { require(details.isEmpty()); details = object; }
        }
        require(!details.isEmpty() && details.value("ok").isBool() && details.value("ok").toBool());
        const auto run = details.value("run").toObject();
        require(run.value("validation_status") == "not_requested" && !run.contains("validation_id"));
        require(run.value("snapshot_id") == snapshotId && run.value("physical_hash") == hash && run.value("solver_version") == evidence.value("native_solver_version") &&
            integer(run.value("numerical_revision"), 0, 9007199254740991LL) == revision &&
            number(run.value("horizon_h")) == horizon && number(run.value("sample_step_h")) == step &&
            number(run.value("cost")) == number(evidence.value("native_cost")) && array(run.value("controls"), 0, 0).isEmpty());
        require(evidence.value("native_volume_feasible").isBool() && run.value("validation").toObject().value("feasible").isBool() &&
            evidence.value("native_volume_feasible") == run.value("validation").toObject().value("feasible"));
        const auto fullTimes = array(details.value("time_h"), 2, 10001); checkGrid(fullTimes, horizon, step);
        QJsonArray expectedTanks, controls;
        for (const auto& asset : project.network.assets) if (asset.kind == AssetKind::tank)
            expectedTanks.append(QJsonObject{{"id", QString::fromStdString(asset.id)}, {"label", QString::fromStdString(asset.label)}, {"capacity_m3", asset.capacity_m3}});
        require(evidence.value("tanks").isArray() && evidence.value("tanks").toArray() == expectedTanks);
        QJsonArray tankIds; for (const auto& tank : expectedTanks) tankIds.append(tank.toObject().value("id"));
        require(details.value("tank_ids").isArray() && details.value("tank_ids").toArray() == tankIds);
        const auto fullVolumes = array(details.value("volumes_m3"), tankIds.size(), tankIds.size());
        for (const auto& value : fullVolumes) for (const auto& sample : array(value, fullTimes.size(), fullTimes.size())) (void)number(sample);
        const auto catalog = control_catalog(project.network);
        std::ptrdiff_t previous = -1;
        for (const auto& value : array(evidence.value("selected_controls"), derivatives ? 1 : 0, derivatives ? 384 : 0)) {
            require(value.isObject()); const auto selected = value.toObject(); keys(selected, {"control_id", "captured_value", "unit"});
            const auto id = text(selected.value("control_id"), 512);
            const auto found = std::find_if(catalog.begin(), catalog.end(), [&](const ControlDescriptor& c) { return QString::fromStdString(c.id) == id; });
            require(found != catalog.end() && std::distance(catalog.begin(), found) > previous && number(selected.value("captured_value")) == found->value &&
                selected.value("unit") == QString::fromStdString(found->unit));
            previous = std::distance(catalog.begin(), found);
            controls.append(QJsonObject{{"control_id", id}, {"value", found->value}});
        }
        require(evidence.value("preview").isObject()); const auto preview = evidence.value("preview").toObject();
        keys(preview, {"time_h", "time_indices", "native_volumes_m3", "worker_volumes_m3", "source_frames"});
        require(integer(preview.value("source_frames"), 2, 10001) == fullTimes.size());
        const QJsonObject identity{{"snapshot_id", snapshotId}, {"volume_run_id", runId}, {"physical_hash", hash}, {"numerical_revision", revision}};
        const QJsonObject payload{{"method_version", resultVersion}, {"identity", identity}, {"network", snapshot.value("network")},
            {"controls", controls}, {"horizon_h", horizon}, {"step_h", step}, {"derivatives", derivatives}, {"preview", QJsonObject{{"max_times", 256}}}};
        QJsonObject response{{"method", "storage_rollout"}, {"method_version", resultVersion}, {"identity", identity}, {"provenance", evidence.value("provenance")},
            {"time_h", preview.value("time_h")}, {"time_indices", preview.value("time_indices")}, {"tank_ids", tankIds},
            {"volumes_m3", preview.value("worker_volumes_m3")}, {"cost", evidence.value("worker_cost")}, {"warnings", evidence.value("warnings")}};
        if (derivatives) response["derivatives"] = evidence.value("derivatives");
        require(protocol::storagePayload(payload) && protocol::storage(response, payload));
        static const QRegularExpression versionPattern(QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._+\\-]{0,127}$"));
        require(versionPattern.match(evidence.value("provenance").toObject().value("implementation_version").toString()).hasMatch());
        const auto times = preview.value("time_h").toArray(), indices = preview.value("time_indices").toArray();
        const auto nativePreview = array(preview.value("native_volumes_m3"), tankIds.size(), tankIds.size());
        const auto workerPreview = preview.value("worker_volumes_m3").toArray();
        const auto errors = array(evidence.value("tank_errors"), tankIds.size(), tankIds.size());
        bool agrees = true;
        for (qsizetype tank = 0; tank < tankIds.size(); ++tank) {
            const auto native = array(nativePreview[tank], times.size(), times.size()), worker = workerPreview[tank].toArray(), full = fullVolumes[tank].toArray();
            double maximumAbsolute = 0, maximumScaled = 0;
            for (qsizetype sample = 0; sample < times.size(); ++sample) {
                const auto index = integer(indices[sample], 0, fullTimes.size() - 1);
                require(number(times[sample]) == number(fullTimes[index]) && number(native[sample]) == number(full[index]));
                double absolute = 0; const auto scaled = scaledError(number(native[sample]), number(worker[sample]), absolute);
                maximumAbsolute = std::max(maximumAbsolute, absolute); maximumScaled = std::max(maximumScaled, scaled);
            }
            require(errors[tank].isObject()); const auto diagnostic = errors[tank].toObject();
            keys(diagnostic, {"tank_id", "max_absolute_error_m3", "max_scaled_error", "within_tolerance"});
            require(diagnostic.value("tank_id") == tankIds[tank] && number(diagnostic.value("max_absolute_error_m3")) == maximumAbsolute &&
                number(diagnostic.value("max_scaled_error")) == maximumScaled && diagnostic.value("within_tolerance").isBool() &&
                diagnostic.value("within_tolerance").toBool() == (maximumScaled <= 1));
            agrees = agrees && maximumScaled <= 1;
        }
        double absoluteCost = 0;
        const auto nativeCost = number(evidence.value("native_cost")), workerCost = number(evidence.value("worker_cost"));
        const auto scaledCost = scaledError(nativeCost, workerCost, absoluteCost);
        require(number(evidence.value("cost_difference")) == workerCost - nativeCost && number(evidence.value("cost_absolute_error")) == absoluteCost &&
            number(evidence.value("cost_scaled_error")) == scaledCost && evidence.value("within_tolerance").isBool() &&
            evidence.value("within_tolerance").toBool() == (agrees && scaledCost <= 1));
        return true;
    } catch (const std::exception& failure) { if (error) *error = QString::fromUtf8(failure.what()); return false; }
}
} // namespace breakwater::desktop::science

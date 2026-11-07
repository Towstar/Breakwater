#include "SimulationGateway.hpp"
#include "ProjectDocument.hpp"

#include <QDateTime>
#include <QSet>
#include <QUuid>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace breakwater::desktop::agent {
namespace {
constexpr std::size_t max_runs = 128;
constexpr qsizetype max_candidates = 32;
constexpr qsizetype max_controls = 384;
QString q(const std::string& value) { return QString::fromStdString(value); }
QString uuid() { return QUuid::createUuid().toString(QUuid::WithoutBraces); }
struct InvalidCall { QString code; QString message; };
[[noreturn]] void reject(const QString& code, const QString& message) { throw InvalidCall{code, message}; }
QJsonObject failure(const QString& code, const QString& message) {
    return {{"ok", false}, {"error", QJsonObject{{"code", code}, {"message", message}}}};
}
void keys(const QJsonObject& object, std::initializer_list<const char*> allowed, std::initializer_list<const char*> required) {
    for (auto it = object.begin(); it != object.end(); ++it) {
        const bool found = std::any_of(allowed.begin(), allowed.end(), [&](const char* name) { return it.key() == QLatin1String(name); });
        if (!found) reject("invalid_arguments", "Unknown argument: " + it.key().left(128));
    }
    for (const auto* name : required)
        if (!object.contains(QLatin1String(name))) reject("invalid_arguments", QStringLiteral("Missing argument: ") + QLatin1String(name));
}
QString string(const QJsonObject& object, const QString& name) {
    const auto value = object.value(name);
    if (!value.isString() || value.toString().trimmed().isEmpty() || value.toString().size() > 256)
        reject("invalid_arguments", "Expected nonempty string (maximum 256 characters): " + name);
    return value.toString();
}
QJsonArray issues_json(const std::vector<Issue>& issues) {
    QJsonArray result;
    for (const auto& issue : issues) {
        QJsonObject item{{"code", q(issue.code)}, {"asset_id", q(issue.asset_id)}, {"message", q(issue.message)}};
        if (issue.time_h) item["time_h"] = *issue.time_h;
        result.append(item);
    }
    return result;
}
QJsonObject validation_json(const ValidationReport& report) {
    QJsonArray limits;
    for (const auto& limit : report.limits)
        limits.append(QJsonObject{{"asset_id", q(limit.asset_id)}, {"limit", q(limit.limit)}, {"time_h", limit.time_h}});
    return {{"feasible", report.feasible}, {"violations", issues_json(report.violations)}, {"limits", limits},
        {"minimum_headroom_m3", report.minimum_headroom_m3},
        {"max_conservation_residual_m3", report.max_conservation_residual_m3}};
}
QJsonArray controls_json(const std::vector<ControlValue>& controls) {
    QJsonArray result;
    for (const auto& control : controls) result.append(QJsonObject{{"control_id", q(control.id)}, {"value", control.value}});
    return result;
}
QJsonObject schema(const QJsonObject& properties, const QJsonArray& required) {
    return {{"type", "object"}, {"properties", properties}, {"required", required}, {"additionalProperties", false}};
}
QJsonObject text_schema() { return {{"type", "string"}, {"minLength", 1}, {"maxLength", 256}}; }
QJsonObject tool(const char* name, const char* description, const QJsonObject& parameters) {
    return {{"type", "function"}, {"function", QJsonObject{{"name", name}, {"description", description}, {"parameters", parameters}}}};
}
}

SimulationGateway::SimulationGateway(const Network& network, const QString& physicalHash, double horizonH)
    : network_(network), snapshot_id_(uuid()), physical_hash_(physicalHash), config_{horizonH, 0.025} {
    if (!std::isfinite(horizonH) || horizonH <= 0.0 || horizonH > 24.0)
        throw std::invalid_argument("Agent horizon must be finite and within (0, 24] hours.");
    if (network.assets.size() > 128 || network.routes.size() > 256 || physicalHash.isEmpty())
        throw std::invalid_argument("Agent snapshot needs a physical hash and a bounded network.");
}

void SimulationGateway::invalidate() { current_.store(false); }

std::vector<ControlValue> SimulationGateway::parseControls(const QJsonArray& controls) const {
    if (controls.size() > max_controls) reject("resource_limit", "A candidate exceeds the control-count limit.");
    const auto catalog = control_catalog(network_);
    QSet<QString> seen;
    std::vector<ControlValue> result;
    result.reserve(static_cast<std::size_t>(controls.size()));
    for (const auto& entry : controls) {
        if (!entry.isObject()) reject("invalid_arguments", "Each control must be an object.");
        const auto object = entry.toObject();
        keys(object, {"control_id", "value"}, {"control_id", "value"});
        const auto id = string(object, "control_id");
        if (seen.contains(id)) reject("invalid_control", "Duplicate control ID: " + id);
        seen.insert(id);
        const auto value = object.value("value");
        if (!value.isDouble() || !std::isfinite(value.toDouble())) reject("invalid_control", "Control values must be finite numbers.");
        const auto descriptor = std::find_if(catalog.begin(), catalog.end(), [&](const ControlDescriptor& item) { return q(item.id) == id; });
        if (descriptor == catalog.end()) reject("invalid_control", "Unknown control ID: " + id);
        if (value.toDouble() < descriptor->minimum || value.toDouble() > descriptor->maximum)
            reject("invalid_control", "Control value is outside its catalog bounds: " + id);
        result.push_back({id.toStdString(), value.toDouble()});
    }
    return result;
}

SimulationGateway::StoredRun SimulationGateway::compute(const QString& candidateId, const std::vector<ControlValue>& controls) const {
    StoredRun run;
    run.id = uuid(); run.candidate_id = candidateId; run.overrides = controls;
    run.compiled = compile(network_, controls);
    run.result = simulate(run.compiled, config_);
    run.reported_validation = validate(run.compiled, run.result, config_);
    run.created_at = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    return run;
}

QJsonObject SimulationGateway::summary(const StoredRun& run) const {
    QJsonArray tanks;
    for (std::size_t i = 0; i < run.result.tank_ids.size(); ++i) {
        const auto& samples = run.result.volumes_m3[i];
        QJsonArray trajectory;
        const auto middle = samples.size() / 2;
        for (const auto index : {std::size_t{0}, middle, samples.size() - 1})
            trajectory.append(QJsonObject{{"time_h", run.result.time_h[index]}, {"volume_m3", samples[index]}});
        tanks.append(QJsonObject{{"asset_id", q(run.result.tank_ids[i])}, {"initial_m3", samples.front()},
            {"final_m3", samples.back()}, {"trajectory_summary", trajectory}});
    }
    const auto& report = run.explicit_validation ? *run.explicit_validation : run.reported_validation;
    const char* solver = run.result.solver == Solver::dense_reference ? "dense_reference" :
        (run.result.solver == Solver::affine ? "affine" : "sparse");
    QJsonObject result{{"run_id", run.id}, {"candidate_id", run.candidate_id}, {"created_at", run.created_at}, {"snapshot_id", snapshot_id_},
        {"physical_hash", physical_hash_}, {"horizon_h", config_.horizon_h}, {"sample_step_h", config_.step_h},
        {"cost", run.result.cost}, {"cost_unit", "synthetic units"}, {"controls", controls_json(run.overrides)},
        {"tanks", tanks}, {"solver", solver}, {"solver_version", solver_version},
        {"validation_status", run.explicit_validation ? "validated" : "not_requested"},
        {"validation", validation_json(report)}, {"current", current_.load()},
        {"warnings", QJsonArray{"Synthetic constant-flow model; nominal volume feasibility only.",
            "Quality is not evaluated. Finite candidate evaluation does not establish global optimality."}}};
    if (!run.validation_id.isEmpty()) result["validation_id"] = run.validation_id;
    return result;
}

QJsonObject SimulationGateway::recommendationLocked(const QString& runId) const {
    if (!current_.load()) return failure("stale_snapshot", "The physical snapshot changed; start a new investigation.");
    if (!network_inspected_ || !controls_inspected_) return failure("inspection_required", "Inspect the network and its control catalog before recommending a plan.");
    const auto it = runs_.find(runId);
    if (it == runs_.end()) return failure("unknown_run", "The run ID is not in this session's immutable run store.");
    const auto& run = it->second;
    if (!run.explicit_validation) return failure("validation_required", "Call validate_run before recommending or staging this run.");
    if (!run.explicit_validation->feasible) return failure("infeasible_run", "Independent validation rejected this candidate.");
    auto result = summary(run);
    result["ok"] = true;
    result["selected_run_id"] = run.id;
    result["verified_feasible"] = true;
    result["verified_cost"] = run.result.cost;
    result["approval_status"] = staged_run_id_ == run.id ? "pending_operator" : "advisory";
    if (!current_.load()) return failure("stale_snapshot", "The physical snapshot changed before the recommendation completed.");
    return result;
}

QJsonObject SimulationGateway::recommendation(const QString& runId) const {
    std::lock_guard lock(mutex_);
    return recommendationLocked(runId);
}

QJsonObject SimulationGateway::stagedPlan() const {
    std::lock_guard lock(mutex_);
    if (staged_run_id_.isEmpty()) return failure("no_staged_plan", "No validated plan has been staged for operator approval.");
    return recommendationLocked(staged_run_id_);
}

std::vector<ControlValue> SimulationGateway::stagedControls() const {
    std::lock_guard lock(mutex_);
    if (!current_.load() || staged_run_id_.isEmpty()) return {};
    const auto it = runs_.find(staged_run_id_);
    if (it == runs_.end() || !it->second.explicit_validation || !it->second.explicit_validation->feasible) return {};
    return it->second.overrides;
}

QJsonArray SimulationGateway::runs() const {
    std::lock_guard lock(mutex_);
    QJsonArray result;
    for (const auto& [id, run] : runs_) { (void)id; result.append(summary(run)); }
    return result;
}

QJsonObject SimulationGateway::runDetails(const QString& runId) const {
    std::lock_guard lock(mutex_);
    const auto it = runs_.find(runId);
    if (it == runs_.end()) return failure("unknown_run", "The run ID is not in this session's immutable run store.");
    const auto& run = it->second;
    QJsonArray times, tank_ids, trajectories;
    for (const auto time : run.result.time_h) times.append(time);
    for (const auto& id : run.result.tank_ids) tank_ids.append(q(id));
    for (const auto& samples : run.result.volumes_m3) {
        QJsonArray series;
        for (const auto volume : samples) series.append(volume);
        trajectories.append(series);
    }
    return {{"ok", true}, {"run", summary(run)}, {"time_h", times}, {"tank_ids", tank_ids}, {"volumes_m3", trajectories}};
}

QJsonObject SimulationGateway::execute(const QString& name, const QJsonObject& args) {
    try {
        if (!current_.load()) reject("stale_snapshot", "The physical snapshot changed; start a new investigation.");
        if (name == "get_network_state") {
            keys(args, {}, {});
            const auto issues = inspect_network(network_);
            { std::lock_guard lock(mutex_); network_inspected_ = true; }
            return {{"ok", true}, {"snapshot_id", snapshot_id_}, {"physical_hash", physical_hash_}, {"horizon_h", config_.horizon_h},
                {"network", network_json(network_)}, {"draft_issues", issues_json(issues)},
                {"capabilities", QJsonObject{{"native_simulation", true}, {"independent_validation", true}, {"search_interventions", false}}},
                {"model_limitations", QJsonArray{"Synthetic constant-flow network; geometry does not determine flow.", "Quality is not evaluated."}}};
        }
        if (name == "get_control_catalog") {
            keys(args, {}, {});
            QJsonArray controls;
            for (const auto& descriptor : control_catalog(network_))
                controls.append(QJsonObject{{"control_id", q(descriptor.id)}, {"label", q(descriptor.id)}, {"asset_id", q(descriptor.asset_id)},
                    {"quantity", descriptor.id.starts_with("source:") ? "production_curtailment" : "flow_rate"},
                    {"unit", q(descriptor.unit)}, {"value", descriptor.value}, {"minimum", descriptor.minimum},
                    {"maximum", descriptor.maximum}, {"available", descriptor.maximum > 0.0}, {"cost_per_m3", descriptor.cost_per_m3}});
            { std::lock_guard lock(mutex_); controls_inspected_ = true; }
            return {{"ok", true}, {"snapshot_id", snapshot_id_}, {"controls", controls},
                {"unspecified_controls", "All unspecified controls retain their immutable snapshot values."}};
        }
        const QSet<QString> supported{"forecast_baseline", "simulate_candidates", "validate_run", "compare_runs", "search_interventions", "request_approval"};
        if (!supported.contains(name)) reject("unknown_tool", "This tool is not registered.");
        if (string(args, "snapshot_id") != snapshot_id_) reject("snapshot_mismatch", "The requested snapshot is not this session's incident snapshot.");
        if (name == "forecast_baseline") {
            keys(args, {"snapshot_id"}, {"snapshot_id"});
            { std::lock_guard lock(mutex_); if (runs_.size() >= max_runs) reject("resource_limit", "The session run-store limit was reached."); }
            auto run = compute("baseline", {});
            std::lock_guard lock(mutex_);
            if (runs_.size() >= max_runs) reject("resource_limit", "The session run-store limit was reached.");
            auto result = summary(run);
            const auto id = run.id;
            runs_.emplace(id, std::move(run));
            if (!current_.load()) {
                auto error = failure("stale_snapshot", "The physical snapshot changed during the forecast.");
                error["completed_runs"] = QJsonArray{result}; return error;
            }
            return {{"ok", true}, {"run", result}};
        }
        if (name == "simulate_candidates") {
            keys(args, {"snapshot_id", "candidates"}, {"snapshot_id", "candidates"});
            if (!args.value("candidates").isArray()) reject("invalid_arguments", "candidates must be an array.");
            const auto candidates = args.value("candidates").toArray();
            if (candidates.empty() || candidates.size() > max_candidates) reject("resource_limit", "Evaluate between 1 and 32 candidates per call.");
            { std::lock_guard lock(mutex_); if (runs_.size() + static_cast<std::size_t>(candidates.size()) > max_runs) reject("resource_limit", "The session run-store limit would be exceeded."); }
            QSet<QString> candidate_ids;
            std::vector<std::pair<QString, std::vector<ControlValue>>> parsed;
            for (const auto& candidate : candidates) {
                if (!candidate.isObject()) reject("invalid_arguments", "Each candidate must be an object.");
                const auto object = candidate.toObject();
                keys(object, {"candidate_id", "controls"}, {"candidate_id", "controls"});
                const auto id = string(object, "candidate_id");
                if (candidate_ids.contains(id)) reject("invalid_arguments", "Candidate IDs must be unique within a batch.");
                candidate_ids.insert(id);
                if (!object.value("controls").isArray()) reject("invalid_arguments", "Candidate controls must be an array.");
                parsed.emplace_back(id, parseControls(object.value("controls").toArray()));
            }
            QJsonArray results;
            for (const auto& [id, controls] : parsed) {
                if (!current_.load()) break;
                auto run = compute(id, controls);
                std::lock_guard lock(mutex_);
                if (runs_.size() >= max_runs) reject("resource_limit", "The session run-store limit was reached.");
                results.append(summary(run));
                const auto run_id = run.id;
                runs_.emplace(run_id, std::move(run));
            }
            if (!current_.load()) {
                auto error = failure("stale_snapshot", "The physical snapshot changed during candidate evaluation.");
                error["completed_runs"] = results; return error;
            }
            return {{"ok", true}, {"snapshot_id", snapshot_id_}, {"runs", results}};
        }
        if (name == "validate_run") {
            keys(args, {"snapshot_id", "run_id"}, {"snapshot_id", "run_id"});
            const auto id = string(args, "run_id");
            StoredRun copy;
            {
                std::lock_guard lock(mutex_);
                const auto it = runs_.find(id);
                if (it == runs_.end()) reject("unknown_run", "The run ID is not in this session's immutable run store.");
                copy = it->second;
            }
            const auto report = validate(copy.compiled, copy.result, config_);
            std::lock_guard lock(mutex_);
            auto& run = runs_.at(id);
            run.explicit_validation = report;
            run.validation_id = uuid();
            auto result = summary(run);
            if (!current_.load()) {
                auto error = failure("stale_snapshot", "The physical snapshot changed during validation.");
                error["completed_runs"] = QJsonArray{result}; return error;
            }
            return {{"ok", true}, {"run", result}};
        }
        if (name == "compare_runs") {
            keys(args, {"snapshot_id", "run_ids"}, {"snapshot_id", "run_ids"});
            if (!args.value("run_ids").isArray()) reject("invalid_arguments", "run_ids must be an array.");
            const auto ids = args.value("run_ids").toArray();
            if (ids.size() < 2 || ids.size() > 32) reject("resource_limit", "Compare between 2 and 32 run IDs.");
            std::lock_guard lock(mutex_);
            QJsonArray results; QSet<QString> seen;
            for (const auto& value : ids) {
                if (!value.isString() || value.toString().isEmpty() || value.toString().size() > 256) reject("invalid_arguments", "Run IDs must be nonempty bounded strings.");
                const auto id = value.toString();
                if (seen.contains(id)) reject("invalid_arguments", "Run IDs must be unique.");
                seen.insert(id);
                const auto it = runs_.find(id);
                if (it == runs_.end()) reject("unknown_run", "A run ID is not in this session's immutable run store.");
                results.append(summary(it->second));
            }
            if (!current_.load()) reject("stale_snapshot", "The physical snapshot changed before comparison completed.");
            return {{"ok", true}, {"snapshot_id", snapshot_id_}, {"horizon_h", config_.horizon_h}, {"runs", results},
                {"scope", "Same immutable snapshot and planning horizon; explicit validation is required before recommendation."}};
        }
        if (name == "request_approval") {
            keys(args, {"snapshot_id", "run_id"}, {"snapshot_id", "run_id"});
            const auto id = string(args, "run_id");
            std::lock_guard lock(mutex_);
            const auto eligibility = recommendationLocked(id);
            if (!eligibility.value("ok").toBool()) return eligibility;
            if (!current_.load()) reject("stale_snapshot", "The physical snapshot changed before staging.");
            staged_run_id_ = id;
            auto plan = recommendationLocked(id);
            if (!plan.value("ok").toBool()) { staged_run_id_.clear(); return plan; }
            return {{"ok", true}, {"plan", plan}, {"run_id", id}, {"approval_status", "pending_operator"},
                {"applied", false}, {"message", "Staged for an explicit human action. No operating controls were changed."}};
        }
        return failure("capability_unavailable", "Numerical search has not been integrated; no search was executed.");
    } catch (const InvalidCall& error) {
        return failure(error.code, error.message);
    } catch (const std::invalid_argument&) {
        return failure("engine_rejected", "The native engine rejected the network or control configuration. Inspect its topology and control catalog.");
    } catch (const std::exception&) {
        return failure("engine_failed", "The native numerical operation failed; no successful result is claimed.");
    }
}

QJsonArray SimulationGateway::toolDefinitions() {
    const QJsonObject snapshot{{"snapshot_id", text_schema()}};
    const auto control = schema({{"control_id", text_schema()}, {"value", QJsonObject{{"type", "number"}, {"minimum", 0}}}}, {"control_id", "value"});
    const auto candidate = schema({{"candidate_id", text_schema()}, {"controls", QJsonObject{{"type", "array"}, {"maxItems", static_cast<int>(max_controls)}, {"items", control}}}}, {"candidate_id", "controls"});
    auto candidate_args = snapshot;
    candidate_args["candidates"] = QJsonObject{{"type", "array"}, {"minItems", 1}, {"maxItems", static_cast<int>(max_candidates)}, {"items", candidate}};
    auto run_args = snapshot; run_args["run_id"] = text_schema();
    auto compare_args = snapshot;
    compare_args["run_ids"] = QJsonObject{{"type", "array"}, {"minItems", 2}, {"maxItems", 32}, {"uniqueItems", true}, {"items", text_schema()}};
    return {
        tool("get_network_state", "Read the actual immutable incident topology, IDs, state, fixed planning horizon and capabilities. Required before recommending.", schema({}, {})),
        tool("get_control_catalog", "Read actual stable control IDs, units, snapshot values and bounds. Required before recommending; unspecified controls retain snapshot values.", schema({}, {})),
        tool("forecast_baseline", "Run the native baseline forecast from the incident snapshot. Preserve unclipped violations; explicit validate_run is still required for recommendations.", schema(snapshot, {"snapshot_id"})),
        tool("simulate_candidates", "Evaluate 1..32 intervention candidates through the real native engine from the same immutable snapshot and fixed horizon. Only specified controls change.", schema(candidate_args, {"snapshot_id", "candidates"})),
        tool("validate_run", "Independently validate a stored run using the authoritative native validator. Only an explicitly validated feasible run may be recommended or staged.", schema(run_args, {"snapshot_id", "run_id"})),
        tool("compare_runs", "Compare real stored run costs, validation, limit times, headroom, controls and trajectories from this snapshot and horizon.", schema(compare_args, {"snapshot_id", "run_ids"})),
        tool("request_approval", "Stage an inspected, explicitly validated feasible run for human review. This does not approve or apply controls.", schema(run_args, {"snapshot_id", "run_id"}))
    };
}

} // namespace breakwater::desktop::agent

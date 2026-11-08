#include "HistoryController.hpp"
#include "ProjectDocument.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace breakwater::desktop::history {
namespace {
QString idOf(const QJsonObject& detail) { return detail.value("run").toObject().value("run_id").toString(); }
QJsonArray changes(const QJsonObject& snapshot, const QJsonObject& before, const QJsonObject& after) {
    QMap<QString, double> left, right;
    try {
        const auto project = parse_project({{"version", source_allocation_project_version}, {"network", snapshot.value("network")}, {"layout", QJsonObject{}}});
        for (const auto& control : control_catalog(project.network)) {
            left[QString::fromStdString(control.id)] = control.value;
            right[QString::fromStdString(control.id)] = control.value;
        }
    } catch (...) { return {}; }
    for (const auto& value : before.value("controls").toArray()) {
        const auto item = value.toObject(); left[item.value("control_id").toString()] = item.value("value").toDouble();
    }
    for (const auto& value : after.value("controls").toArray()) {
        const auto item = value.toObject(); right[item.value("control_id").toString()] = item.value("value").toDouble();
    }
    QJsonArray result;
    for (auto it = right.cbegin(); it != right.cend(); ++it)
        if (left.value(it.key()) != it.value()) result.append(QJsonObject{{"control_id", it.key()}, {"before", left.value(it.key())}, {"after", it.value()}, {"unit", "m3/h"}});
    return result;
}
QJsonObject firstViolation(const QJsonObject& run) {
    QJsonObject first;
    for (const auto& value : run.value("validation").toObject().value("violations").toArray()) {
        const auto issue = value.toObject();
        if (first.isEmpty() || (issue.value("time_h").isDouble() && issue.value("time_h").toDouble() < first.value("time_h").toDouble(std::numeric_limits<double>::infinity()))) first = issue;
    }
    return first;
}
}

HistoryController::HistoryController(SessionRepository* repository, QObject* parent) : QObject(parent), repository_(repository) {
    if (!repository_) return;
    connect(repository_, &SessionRepository::ready, this, &HistoryController::refresh);
    connect(repository_, &SessionRepository::sessionSaved, this, [this] { refresh(); });
    connect(repository_, &SessionRepository::sessionsListed, this, [this](const QJsonArray& sessions) { sessions_ = sessions.toVariantList(); emit changed(); });
    connect(repository_, &SessionRepository::sessionLoaded, this, [this](const QJsonObject& archive) {
        archive_ = archive; historical_ = true; busy_ = false; status_ = "Historical evidence — approval is unavailable";
        defaultSelection(true); emit changed();
    });
    connect(repository_, &SessionRepository::operationCompleted, this, [this](const QString& operation, const QString&) {
        if (operation == "import" || operation == "export") { busy_ = false; status_ = operation == "import" ? "Session imported as historical evidence" : "Session exported"; refresh(); emit changed(); }
    });
    connect(repository_, &SessionRepository::failed, this, [this](const QString&, const QString& message) { busy_ = false; status_ = message; emit changed(); });
}

QJsonObject HistoryController::detail(const QString& runId) const {
    for (const auto& value : archive_.value("runs").toArray()) if (idOf(value.toObject()) == runId) return value.toObject();
    return {};
}
QVariantList HistoryController::runs() const {
    QVariantList result;
    QList<QJsonObject> summaries;
    const auto details = archive_.value("runs").toArray();
    summaries.reserve(details.size());
    for (const auto& value : details) summaries.append(value.toObject().value("run").toObject());
    std::sort(summaries.begin(), summaries.end(), [](const QJsonObject& a, const QJsonObject& b) {
        return a.value("created_at").toString() < b.value("created_at").toString();
    });
    for (const auto& summary : summaries) {
        auto run = summary.toVariantMap(); run["historical"] = historical_;
        result.append(run);
    }
    return result;
}
void HistoryController::defaultSelection(bool reset) {
    if (reset) { baseline_.clear(); candidate_.clear(); tank_.clear(); manual_selection_ = false; }
    const auto entries = runs();
    if (baseline_.isEmpty()) for (const auto& value : entries) {
        const auto run = value.toMap(); if (run.value("candidate_id").toString() == "baseline") { baseline_ = run.value("run_id").toString(); break; }
    }
    if (!manual_selection_) {
        const auto selected = archive_.value("recommendation").toObject().value("selected_run_id").toString();
        if (!selected.isEmpty() && !detail(selected).isEmpty()) candidate_ = selected;
    }
    if (candidate_.isEmpty() && !entries.isEmpty()) candidate_ = entries.last().toMap().value("run_id").toString();
    const auto options = tanks();
    bool found = false; for (const auto& option : options) found |= option.toMap().value("id").toString() == tank_;
    if (!found) tank_ = options.isEmpty() ? QString{} : options.first().toMap().value("id").toString();
    defaultScientificSelection(reset);
}
void HistoryController::setLiveSession(const QJsonObject& archive) {
    if (archive.value("session_id") == live_.value("session_id") && archive.value("checkpoint_sequence").toDouble() < live_.value("checkpoint_sequence").toDouble()) return;
    live_ = archive;
    if (!historical_) {
        const bool reset = selectedSessionId() != archive.value("session_id").toString();
        archive_ = archive; defaultSelection(reset);
        status_ = archive.value("status").toString() + " · current session"; emit changed();
    }
}
void HistoryController::refresh() { if (repository_) repository_->listSessions(); }
void HistoryController::loadSession(const QString& id) { if (repository_ && !id.isEmpty()) { busy_ = true; repository_->loadSession(id); emit changed(); } }
void HistoryController::showLive() {
    const bool reset = historical_ || selectedSessionId() != live_.value("session_id").toString();
    historical_ = false; archive_ = live_; defaultSelection(reset); status_ = "Current session"; emit changed();
}
void HistoryController::selectRuns(const QString& baseline, const QString& candidate) {
    if ((!baseline.isEmpty() && detail(baseline).isEmpty()) || (!candidate.isEmpty() && detail(candidate).isEmpty())) return;
    baseline_ = baseline; candidate_ = candidate; manual_selection_ = true; defaultSelection(false); emit changed();
}
void HistoryController::selectTank(const QString& id) {
    for (const auto& value : tanks()) if (value.toMap().value("id").toString() == id) { tank_ = id; emit changed(); return; }
}
QJsonObject HistoryController::scientificDetail(const QString& id) const {
    for (const auto& value : archive_.value("scientific_evidence").toArray()) {
        const auto evidence = value.toObject();
        if (!id.isEmpty() && evidence.value("result_id").toString() == id) return evidence;
    }
    return {};
}
QVariantList HistoryController::scientificEvidence() const {
    QVariantList rows;
    for (const auto& value : archive_.value("scientific_evidence").toArray()) {
        const auto evidence = value.toObject();
        rows.append(QVariantMap{{"result_id", evidence.value("result_id").toString()},
            {"run_id", evidence.value("run_id").toString()}, {"created_at", evidence.value("created_at").toString()},
            {"analysis_mode", evidence.value("analysis_mode").toString()},
            {"within_tolerance", evidence.value("within_tolerance").toBool()}});
    }
    return rows;
}
QVariantMap HistoryController::scientificResult() const { return scientificDetail(scientific_result_).toVariantMap(); }
QVariantList HistoryController::scientificTanks() const { return scientificDetail(scientific_result_).value("tanks").toArray().toVariantList(); }
void HistoryController::defaultScientificSelection(bool reset) {
    const auto evidence = archive_.value("scientific_evidence").toArray();
    if (reset || scientificDetail(scientific_result_).isEmpty()) {
        scientific_result_ = evidence.isEmpty() ? QString{} : evidence.first().toObject().value("result_id").toString();
        scientific_tank_.clear();
    }
    const auto options = scientificTanks();
    bool found = false;
    for (const auto& value : options) found |= value.toMap().value("id").toString() == scientific_tank_;
    if (!found) scientific_tank_ = options.isEmpty() ? QString{} : options.first().toMap().value("id").toString();
}
void HistoryController::selectScientificResult(const QString& id) {
    if (id == scientific_result_ || scientificDetail(id).isEmpty()) return;
    scientific_result_ = id; scientific_tank_.clear(); defaultScientificSelection(false); emit changed();
}
void HistoryController::selectScientificTank(const QString& id) {
    if (id == scientific_tank_) return;
    for (const auto& value : scientificTanks()) if (value.toMap().value("id").toString() == id) {
        scientific_tank_ = id; emit changed(); return;
    }
}
QVariantMap HistoryController::scientificChart() const {
    const auto result = scientificDetail(scientific_result_);
    const auto descriptors = result.value("tanks").toArray();
    qsizetype tank = -1;
    for (qsizetype i = 0; i < descriptors.size(); ++i) if (descriptors[i].toObject().value("id").toString() == scientific_tank_) tank = i;
    if (tank < 0) return {{"available", false}};
    const auto preview = result.value("preview").toObject();
    const auto times = preview.value("time_h").toArray(), native = preview.value("native_volumes_m3").toArray(), worker = preview.value("worker_volumes_m3").toArray();
    if (tank >= native.size() || tank >= worker.size()) return {{"available", false}};
    const auto first = native[tank].toArray(), second = worker[tank].toArray();
    if (times.isEmpty() || first.size() != times.size() || second.size() != times.size()) return {{"available", false}};
    const auto capacity = descriptors[tank].toObject().value("capacity_m3").toDouble();
    double low = 0.0, high = capacity;
    QVariantList nativePoints, workerPoints;
    for (qsizetype i = 0; i < times.size(); ++i) {
        const auto a = first[i].toDouble(), b = second[i].toDouble(), time = times[i].toDouble();
        low = std::min({low, a, b}); high = std::max({high, a, b});
        nativePoints.append(QVariantMap{{"time_h", time}, {"volume_m3", a}});
        workerPoints.append(QVariantMap{{"time_h", time}, {"volume_m3", b}});
    }
    if (high == low) high = low + 1.0;
    return {{"available", true}, {"historical", historical_}, {"horizon_h", result.value("horizon_h").toDouble()},
        {"capacity_m3", capacity}, {"y_min", low}, {"y_max", high}, {"series", QVariantList{
            QVariantMap{{"label", "Native"}, {"reference", true}, {"points", nativePoints}},
            QVariantMap{{"label", "PyTorch"}, {"points", workerPoints}}}}};
}
QVariantList HistoryController::scientificSensitivityRows() const {
    const auto result = scientificDetail(scientific_result_);
    if (result.value("analysis_mode").toString() != "sensitivities") return {};
    const auto descriptors = result.value("tanks").toArray();
    qsizetype tank = -1;
    for (qsizetype i = 0; i < descriptors.size(); ++i) if (descriptors[i].toObject().value("id").toString() == scientific_tank_) tank = i;
    const auto selected = result.value("selected_controls").toArray();
    const auto derivative = result.value("derivatives").toObject();
    const auto matrix = derivative.value("terminal_volume_jacobian_h").toArray(), gradient = derivative.value("cost_gradient").toArray();
    if (tank < 0 || tank >= matrix.size() || gradient.size() != selected.size()) return {};
    const auto values = matrix[tank].toArray();
    if (values.size() != selected.size()) return {};
    QVariantList rows;
    for (qsizetype i = 0; i < selected.size(); ++i) {
        auto row = selected[i].toObject().toVariantMap(); row["terminal_volume_jacobian_h"] = values[i].toDouble(); row["cost_gradient"] = gradient[i].toDouble();
        rows.append(row);
    }
    return rows;
}
void HistoryController::exportSession(const QUrl& destination) { if (repository_ && !selectedSessionId().isEmpty()) { busy_ = true; repository_->exportSession(selectedSessionId(), destination); emit changed(); } }
void HistoryController::importSession(const QUrl& source) { if (repository_) { busy_ = true; repository_->importSession(source); emit changed(); } }

QVariantMap HistoryController::comparison() const {
    const auto before = detail(baseline_).value("run").toObject(), after = detail(candidate_).value("run").toObject();
    const bool compatible = !before.isEmpty() && !after.isEmpty() && before.value("snapshot_id") == after.value("snapshot_id") &&
        before.value("physical_hash") == after.value("physical_hash") && before.value("horizon_h") == after.value("horizon_h");
    QJsonObject result{{"compatible", compatible}, {"historical", historical_}, {"baseline", before}, {"candidate", after},
        {"baseline_violation", firstViolation(before)}, {"candidate_violation", firstViolation(after)}};
    result["reason"] = compatible ? "Same captured snapshot and horizon" : "Choose two runs from the same snapshot and horizon";
    if (compatible) { result["cost_delta"] = after.value("cost").toDouble() - before.value("cost").toDouble(); result["control_changes"] = changes(archive_.value("snapshot").toObject(), before, after); }
    return result.toVariantMap();
}
QVariantList HistoryController::tanks() const {
    QVariantList result;
    const auto snapshot = archive_.value("snapshot").toObject();
    for (const auto& value : snapshot.value("network").toObject().value("assets").toArray()) {
        const auto asset = value.toObject();
        if (asset.value("kind") == "tank") result.append(QVariantMap{{"id", asset.value("id").toString()}, {"label", asset.value("label").toString()}, {"capacity_m3", asset.value("capacity_m3").toDouble()}});
    }
    return result;
}
QVariantMap HistoryController::chart() const {
    QJsonArray series;
    double capacity = 0.0;
    for (const auto& value : tanks()) if (value.toMap().value("id").toString() == tank_) capacity = value.toMap().value("capacity_m3").toDouble();
    double low = 0.0, high = capacity, horizon = 0.0;
    const bool compatible = comparison().value("compatible").toBool();
    const QStringList ids = compatible && baseline_ != candidate_ ? QStringList{baseline_, candidate_} : QStringList{candidate_};
    for (const auto& id : ids) {
        const auto stored = detail(id); const auto tankIds = stored.value("tank_ids").toArray();
        qsizetype index = -1; for (qsizetype i = 0; i < tankIds.size(); ++i) if (tankIds[i].toString() == tank_) index = i;
        if (index < 0) continue;
        const auto times = stored.value("time_h").toArray(), volumes = stored.value("volumes_m3").toArray();
        if (index >= volumes.size()) continue;
        const auto values = volumes[index].toArray(); if (values.size() != times.size()) continue;
        QJsonArray points;
        for (qsizetype i = 0; i < times.size(); ++i) {
            const double volume = values[i].toDouble(); low = std::min(low, volume); high = std::max(high, volume);
            points.append(QJsonObject{{"time_h", times[i]}, {"volume_m3", values[i]}});
        }
        horizon = std::max(horizon, stored.value("run").toObject().value("horizon_h").toDouble());
        series.append(QJsonObject{{"run_id", id}, {"label", id == baseline_ ? "Baseline" : "Candidate"}, {"points", points}});
    }
    const double margin = std::max(1.0, (high - low) * 0.06);
    return QJsonObject{{"available", !series.isEmpty()}, {"asset_id", tank_}, {"capacity_m3", capacity}, {"horizon_h", horizon},
        {"y_min", low - margin}, {"y_max", high + margin}, {"series", series}, {"historical", historical_}}.toVariantMap();
}
}

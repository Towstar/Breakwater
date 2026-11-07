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
        const auto project = parse_project({{"version", 1}, {"network", snapshot.value("network")}, {"layout", QJsonObject{}}});
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

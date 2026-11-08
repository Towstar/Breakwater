#include "StorageScienceController.hpp"
#include "StorageScienceAnalysis.hpp"
#include "StorageScienceEvidence.hpp"
#include <QDateTime>
#include <QFutureWatcher>
#include <QJsonArray>
#include <QUuid>
#include <QtConcurrentRun>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace breakwater::desktop::science {
StorageScienceController::StorageScienceController(ProjectController* project, PythonWorkerClient* worker, QObject* parent)
    : QObject(parent), project_(project), worker_(worker) {
    if (project_) {
        connect(project_, &ProjectController::changed, this, &StorageScienceController::refresh);
        connect(project_, &QObject::destroyed, this, [this] {
            const QPointer<StorageScienceController> guard(this);
            rebuildCatalog(nullptr);
            retire("The native project was closed; scientific evidence was retired."); if (guard) emit changed();
        });
    }
    if (worker_) {
        connect(worker_, &PythonWorkerClient::changed, this, &StorageScienceController::changed);
        connect(worker_, &PythonWorkerClient::storageReady, this, &StorageScienceController::accept);
        connect(worker_, &PythonWorkerClient::failed, this, [this](const QString& code, const QString& message) {
            if (!waiting_) return;
            waiting_ = false; captured_.reset(); status_ = message;
            const auto epoch = generation_;
            const QPointer<StorageScienceController> guard(this);
            emit changed(); if (guard && epoch == generation_ && !busy()) emit failed(code, message);
        });
        connect(worker_, &QObject::destroyed, this, [this] {
            retire("The scientific worker was closed."); emit changed();
        });
    }
    refresh();
}
StorageScienceController::~StorageScienceController() {
    const auto wasWaiting = waiting_; waiting_ = false;
    if (worker_) disconnect(worker_, nullptr, this, nullptr);
    if (wasWaiting && worker_) worker_->cancel();
}
QString StorageScienceController::status() const { return waiting_ && worker_ ? worker_->status() : status_; }
void StorageScienceController::setHistoryRepository(history::SessionRepository* repository) {
    if (repository_ == repository) return;
    if (repository_) disconnect(repository_, nullptr, this, nullptr);
    repository_ = repository; pending_history_ids_.clear();
    history_failed_ = !repository; history_saved_result_id_.clear();
    history_status_ = repository ? "Completed scientific evidence is saved automatically to History." : "Scientific history is unavailable.";
    if (repository) {
        connect(repository, &history::SessionRepository::sessionSaved, this, [this](const QString& id) {
            if (!pending_history_ids_.remove(id)) return;
            history_failed_ = false; history_saved_result_id_ = id;
            history_status_ = historyPending() ? "Saving scientific evidence to History…" : "Scientific evidence saved to History.";
            emit historyChanged();
        });
        connect(repository, &history::SessionRepository::checkpointFailed, this,
            [this](const QString& id, const QString&, const QString& message) {
            if (!pending_history_ids_.remove(id)) return;
            history_failed_ = true;
            history_status_ = "History storage error: " + message;
            emit historyChanged();
        });
        connect(repository, &history::SessionRepository::failed, this, [this](const QString& code, const QString& message) {
            if (historyPending() || !history_saved_result_id_.isEmpty() ||
                (code != "storage_unavailable" && code != "storage_corrupt" &&
                 code != "unsupported_database_version" && code != "storage_failed")) return;
            history_failed_ = true; history_status_ = "History storage error: " + message;
            emit historyChanged();
        });
        connect(repository, &QObject::destroyed, this, [this] {
            pending_history_ids_.clear(); history_failed_ = true; history_status_ = "Scientific history is unavailable."; emit historyChanged();
        });
    }
    emit historyChanged();
}
bool StorageScienceController::canRun() const {
    return project_ && worker_ && !busy() && !worker_->busy() && static_cast<bool>(project_->capturedForecast());
}
QVariantList StorageScienceController::controls() const {
    QVariantList rows;
    for (const auto& control : catalog_) {
        const auto id = QString::fromStdString(control.id);
        QString label = QString::fromStdString(control.asset_id) + QStringLiteral(" · route flow");
        if (catalog_capture_) {
            const auto& assets = catalog_capture_->network.assets;
            const auto asset = std::find_if(assets.begin(), assets.end(), [&](const Asset& item) { return item.id == control.asset_id; });
            if (asset != assets.end()) label = QString::fromStdString(asset->label) + QStringLiteral(" · production curtailment");
        }
        const auto reason = ineligible_reasons_.value(id);
        rows.append(QVariantMap{{"control_id", id}, {"label", label}, {"captured_value", control.value},
            {"unit", QString::fromStdString(control.unit)}, {"minimum", control.minimum}, {"maximum", control.maximum},
            {"eligible", reason.isEmpty()}, {"reason", reason}, {"selected", selected_controls_.contains(id)}});
    }
    return rows;
}
QStringList StorageScienceController::selectedControlIds() const {
    QStringList ids;
    for (const auto& control : catalog_) {
        const auto id = QString::fromStdString(control.id);
        if (selected_controls_.contains(id)) ids.append(id);
    }
    return ids;
}
bool StorageScienceController::canRunSensitivities() const {
    if (!canRun() || selected_controls_.isEmpty() || catalog_capture_ != project_->capturedForecast()) return false;
    for (const auto& id : selected_controls_) if (!ineligible_reasons_.contains(id) || !ineligible_reasons_.value(id).isEmpty()) return false;
    return true;
}
QJsonArray StorageScienceController::selectedControls() const {
    QJsonArray controls;
    for (const auto& control : catalog_) {
        const auto id = QString::fromStdString(control.id);
        if (selected_controls_.contains(id)) controls.append(QJsonObject{{"control_id", id}, {"value", control.value}});
    }
    return controls;
}
void StorageScienceController::rebuildCatalog(const std::shared_ptr<const CapturedForecast>& capture) {
    catalog_capture_ = capture; catalog_.clear(); ineligible_reasons_.clear(); selected_controls_.clear();
    if (!capture) return;
    try {
        catalog_ = control_catalog(capture->network);
        for (const auto& control : catalog_) ineligible_reasons_.insert(QString::fromStdString(control.id), sensitivity_ineligible_reason(capture->network, control));
    } catch (const std::exception&) {
        catalog_.clear(); ineligible_reasons_.clear(); status_ = "The captured native control catalog is unavailable.";
    }
}
bool StorageScienceController::selectControl(const QString& id, bool selected) {
    if (!ineligible_reasons_.contains(id)) return false;
    if (selected && !ineligible_reasons_.value(id).isEmpty()) {
        status_ = ineligible_reasons_.value(id); emit changed(); return false;
    }
    if (selected_controls_.contains(id) == selected) return true;
    if (selected) selected_controls_.insert(id); else selected_controls_.remove(id);
    const QPointer<StorageScienceController> guard(this);
    if (sensitivities_ && captured_) retire("Sensitivity selection changed; run a fresh calculation at the captured values.");
    if (guard) emit changed();
    return true;
}
void StorageScienceController::refresh() {
    const auto current = project_ ? project_->capturedForecast() : nullptr;
    if (current != catalog_capture_) rebuildCatalog(current);
    const QPointer<StorageScienceController> guard(this);
    if (captured_ && current != captured_)
        retire("The captured native run changed; run a fresh PyTorch comparison.");
    if (guard) emit changed();
}
void StorageScienceController::retire(const QString& message) {
    ++generation_; result_ = {}; tank_.clear(); captured_.reset(); job_controls_ = {}; status_ = message;
    const auto wasWaiting = waiting_; waiting_ = false;
    if (wasWaiting && worker_) worker_->cancel();
}
void StorageScienceController::run() {
    begin(false);
}
void StorageScienceController::runSensitivities() {
    begin(true);
}
void StorageScienceController::begin(bool sensitivities) {
    if (sensitivities ? !canRunSensitivities() : !canRun()) return;
    captured_ = project_->capturedForecast();
    sensitivities_ = sensitivities; job_controls_ = sensitivities ? selectedControls() : QJsonArray{};
    ++generation_; waiting_ = true; result_ = {}; tank_.clear();
    status_ = sensitivities ? "Calculating selected sensitivities at unchanged captured values…" : "Starting the captured native/PyTorch comparison…";
    const auto request = storage_science_payload(*captured_, sensitivities_, job_controls_);
    const QPointer<StorageScienceController> guard(this);
    worker_->runStorage(request);
    if (guard) emit changed();
}
void StorageScienceController::cancel() {
    if (!busy()) return;
    const QPointer<StorageScienceController> guard(this);
    retire(comparing_ ? "Cancelled. The bounded comparison is finishing." : "Scientific comparison cancelled.");
    if (guard) emit changed();
}
void StorageScienceController::accept(const QJsonObject& response) {
    if (!waiting_ || !captured_) return;
    if (!project_ || project_->capturedForecast() != captured_) { refresh(); return; }
    waiting_ = false; comparing_ = true;
    status_ = "Comparing returned samples with the retained native trajectory…";
    const auto captured = captured_; const auto epoch = generation_;
    const auto selected = job_controls_; const auto sensitivities = sensitivities_;
    auto* watcher = new QFutureWatcher<QJsonObject>(this);
    connect(watcher, &QFutureWatcher<QJsonObject>::finished, this, [this, watcher, captured, epoch] {
        const auto outcome = watcher->result(); watcher->deleteLater(); comparing_ = false;
        if (epoch == generation_ && project_ && project_->capturedForecast() == captured) {
            if (outcome.contains("error")) {
                captured_.reset(); status_ = outcome.value("error").toString();
                const auto message = status_; const QPointer<StorageScienceController> guard(this);
                emit changed(); if (guard && epoch == generation_ && !busy()) emit failed("science_comparison_invalid", message); return;
            }
            result_ = outcome;
            tank_ = result_.value("tanks").toArray().first().toObject().value("id").toString();
            status_ = result_.value("within_tolerance").toBool() ?
                "PyTorch executed. Returned samples agree with native within tolerance; approval remains separate." :
                "PyTorch executed. Numerical disagreement found; approval remains separate.";
            const auto evidence = result_; const QPointer<StorageScienceController> guard(this);
            if (repository_) {
                pending_history_ids_.insert(evidence.value("result_id").toString());
                history_failed_ = false; history_saved_result_id_.clear();
                history_status_ = "Saving scientific evidence to History…";
                // Retain the accepted immutable capture before any observer can
                // edit/reset the project. Serialization and SQL stay off the UI.
                repository_->enqueueCheckpoint([captured, evidence] { return storage_science_archive(*captured, evidence); },
                    evidence.value("result_id").toString());
            }
            emit changed();
            if (guard) emit historyChanged();
            if (guard && epoch == generation_ && project_ && project_->capturedForecast() == captured && !result_.isEmpty()) emit completed(evidence);
            return;
        }
        if (status_ == "Cancelled. The bounded comparison is finishing.") status_ = "Scientific comparison cancelled.";
        emit changed();
    });
    watcher->setFuture(QtConcurrent::run([captured, response, sensitivities, selected] {
        try { return compare_storage_science(captured, response, sensitivities, selected); }
        catch (const std::exception& error) { return QJsonObject{{"error", QString::fromUtf8(error.what())}}; }
        catch (...) { return QJsonObject{{"error", "The scientific comparison could not be completed."}}; }
    }));
    emit changed();
}
void StorageScienceController::selectTank(const QString& id) {
    for (const auto& value : result_.value("tanks").toArray()) if (value.toObject().value("id").toString() == id) {
        if (tank_ != id) { tank_ = id; emit changed(); } return;
    }
}
QVariantList StorageScienceController::sensitivityRows() const {
    if (result_.value("analysis_mode").toString() != "sensitivities") return {};
    const auto tanks = result_.value("tanks").toArray();
    qsizetype tank = -1;
    for (qsizetype i = 0; i < tanks.size(); ++i) if (tanks[i].toObject().value("id").toString() == tank_) tank = i;
    if (tank < 0) return {};
    const auto selected = result_.value("selected_controls").toArray();
    const auto derivative = result_.value("derivatives").toObject();
    const auto matrix = derivative.value("terminal_volume_jacobian_h").toArray();
    const auto gradient = derivative.value("cost_gradient").toArray();
    const auto row = matrix[tank].toArray();
    QVariantList rows;
    for (qsizetype i = 0; i < selected.size(); ++i) {
        auto item = selected[i].toObject().toVariantMap();
        item["terminal_volume_jacobian_h"] = row[i].toDouble(); item["cost_gradient"] = gradient[i].toDouble();
        rows.append(item);
    }
    return rows;
}
QVariantMap StorageScienceController::chart() const {
    const auto descriptors = result_.value("tanks").toArray();
    qsizetype index = -1;
    for (qsizetype i = 0; i < descriptors.size(); ++i) if (descriptors[i].toObject().value("id").toString() == tank_) index = i;
    if (index < 0) return {{"available", false}};
    const auto preview = result_.value("preview").toObject();
    const auto times = preview.value("time_h").toArray();
    const auto native = preview.value("native_volumes_m3").toArray()[index].toArray();
    const auto worker = preview.value("worker_volumes_m3").toArray()[index].toArray();
    const auto capacity = descriptors[index].toObject().value("capacity_m3").toDouble();
    double low = 0.0, high = capacity;
    QVariantList first, second;
    for (qsizetype i = 0; i < times.size(); ++i) {
        const auto a = native[i].toDouble(), b = worker[i].toDouble(), time = times[i].toDouble();
        low = std::min({low, a, b}); high = std::max({high, a, b});
        first.append(QVariantMap{{"time_h", time}, {"volume_m3", a}});
        second.append(QVariantMap{{"time_h", time}, {"volume_m3", b}});
    }
    if (high == low) high = low + 1.0;
    return {{"available", true}, {"horizon_h", result_.value("horizon_h").toDouble()}, {"capacity_m3", capacity},
        {"y_min", low}, {"y_max", high}, {"series", QVariantList{
            QVariantMap{{"label", "Native"}, {"reference", true}, {"points", first}},
            QVariantMap{{"label", "PyTorch"}, {"points", second}}}}};
}

} // namespace breakwater::desktop::science

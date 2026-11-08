#include "StorageScienceController.hpp"
#include <QDateTime>
#include <QFutureWatcher>
#include <QJsonArray>
#include <QUuid>
#include <QtConcurrentRun>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace breakwater::desktop::science {
namespace {
QJsonObject identity(const CapturedForecast& run) {
    return {{"snapshot_id", run.id}, {"volume_run_id", run.id}, {"physical_hash", run.physical_hash},
        {"numerical_revision", static_cast<qint64>(run.revision)}};
}
QJsonObject payload(const CapturedForecast& run) {
    return {{"method_version", 1}, {"identity", identity(run)}, {"network", network_json(run.network)},
        {"controls", QJsonArray{}}, {"horizon_h", run.config.horizon_h}, {"step_h", run.config.step_h},
        {"derivatives", false}, {"preview", QJsonObject{{"max_times", 256}}}};
}
void require(bool condition, const char* message) {
    if (!condition) throw std::invalid_argument(message);
}
double finiteNumber(const QJsonValue& value) {
    require(value.isDouble() && std::isfinite(value.toDouble()), "Scientific comparison requires finite numeric values.");
    return value.toDouble();
}
double scaledError(double first, double second, double& absolute) {
    absolute = std::abs(first - second);
    require(std::isfinite(absolute), "Scientific comparison exceeded the finite diagnostic range.");
    return absolute / (1e-9 + 1e-10 * std::max(std::abs(first), std::abs(second)));
}
QJsonObject compare(const std::shared_ptr<const CapturedForecast>& run, const QJsonObject& response) {
    const auto worker = response.value("result").toObject();
    require(response.value("status").toString() == "completed" && worker.value("identity").toObject() == identity(*run),
        "Scientific response does not bind to this captured native forecast.");
    require(worker.value("method").toString() == "storage_rollout" && worker.value("method_version").toDouble() == 1.0 &&
        !worker.contains("derivatives") && !worker.contains("artifact"), "Scientific response contains unsupported analysis output.");
    const auto times = worker.value("time_h").toArray(), indices = worker.value("time_indices").toArray();
    const auto ids = worker.value("tank_ids").toArray(), volumes = worker.value("volumes_m3").toArray();
    const auto& native = run->result;
    require(times.size() >= 2 && times.size() <= 256 && indices.size() == times.size() &&
        ids.size() == static_cast<qsizetype>(native.tank_ids.size()) && volumes.size() == ids.size(),
        "Scientific preview dimensions disagree with the native reference.");
    require(indices.first().toInteger(-1) == 0 && indices.last().toInteger(-1) == static_cast<qint64>(native.time_h.size() - 1),
        "Scientific preview does not retain the captured native endpoints.");
    qint64 previous = -1;
    for (qsizetype sample = 0; sample < times.size(); ++sample) {
        const auto rawIndex = finiteNumber(indices[sample]);
        require(rawIndex >= 0 && std::floor(rawIndex) == rawIndex && rawIndex < static_cast<double>(native.time_h.size()),
            "Scientific preview contains an invalid native sample index.");
        const auto index = static_cast<qint64>(rawIndex);
        require(index > previous && finiteNumber(times[sample]) == native.time_h[static_cast<std::size_t>(index)],
            "Scientific preview times do not match the original indexed native grid.");
        previous = index;
    }
    bool agrees = true;
    QJsonArray descriptors, errors, nativePreview;
    for (qsizetype tank = 0; tank < ids.size(); ++tank) {
        const auto id = QString::fromStdString(native.tank_ids[static_cast<std::size_t>(tank)]);
        require(ids[tank].isString() && ids[tank].toString() == id && volumes[tank].isArray(),
            "Scientific tank order disagrees with the captured native run.");
        const auto values = volumes[tank].toArray();
        require(values.size() == times.size(), "Scientific tank trajectory has an invalid length.");
        double maximumAbsolute = 0.0, maximumScaled = 0.0;
        QJsonArray nativeValues;
        for (qsizetype sample = 0; sample < times.size(); ++sample) {
            const auto index = static_cast<std::size_t>(indices[sample].toInteger());
            const auto reference = native.volumes_m3[static_cast<std::size_t>(tank)][index];
            double absolute = 0.0;
            const auto scaled = scaledError(reference, finiteNumber(values[sample]), absolute);
            maximumAbsolute = std::max(maximumAbsolute, absolute); maximumScaled = std::max(maximumScaled, scaled);
            nativeValues.append(reference);
        }
        const auto asset = std::find_if(run->network.assets.begin(), run->network.assets.end(),
            [&](const Asset& value) { return value.id == native.tank_ids[static_cast<std::size_t>(tank)]; });
        require(asset != run->network.assets.end(), "Captured native tank metadata is unavailable.");
        descriptors.append(QJsonObject{{"id", id}, {"label", QString::fromStdString(asset->label)}, {"capacity_m3", asset->capacity_m3}});
        errors.append(QJsonObject{{"tank_id", id}, {"max_absolute_error_m3", maximumAbsolute},
            {"max_scaled_error", maximumScaled}, {"within_tolerance", maximumScaled <= 1.0}});
        nativePreview.append(nativeValues); agrees = agrees && maximumScaled <= 1.0;
    }
    const auto workerCost = finiteNumber(worker.value("cost"));
    double absoluteCost = 0.0;
    const auto costScaled = scaledError(native.cost, workerCost, absoluteCost);
    agrees = agrees && costScaled <= 1.0;
    return {{"result_version", 1}, {"result_id", QUuid::createUuid().toString(QUuid::WithoutBraces)},
        {"created_at", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}, {"method", "storage_rollout"},
        {"run_id", run->id}, {"snapshot_id", run->id}, {"physical_hash", run->physical_hash},
        {"numerical_revision", static_cast<qint64>(run->revision)}, {"horizon_h", run->config.horizon_h}, {"step_h", run->config.step_h},
        {"native_cost", native.cost}, {"worker_cost", workerCost}, {"cost_difference", workerCost - native.cost},
        {"cost_absolute_error", absoluteCost}, {"cost_scaled_error", costScaled}, {"within_tolerance", agrees},
        {"comparison_scope", "Returned worker preview samples versus their original native indices; not a feasibility verdict"},
        {"absolute_tolerance", 1e-9}, {"relative_tolerance", 1e-10}, {"native_volume_feasible", run->validation.feasible},
        {"tanks", descriptors}, {"tank_errors", errors}, {"provenance", worker.value("provenance")}, {"warnings", worker.value("warnings")},
        {"preview", QJsonObject{{"time_h", times}, {"time_indices", indices}, {"native_volumes_m3", nativePreview},
            {"worker_volumes_m3", volumes}, {"source_frames", static_cast<qint64>(native.time_h.size())}}}};
}
}

StorageScienceController::StorageScienceController(ProjectController* project, PythonWorkerClient* worker, QObject* parent)
    : QObject(parent), project_(project), worker_(worker) {
    if (project_) {
        connect(project_, &ProjectController::changed, this, &StorageScienceController::refresh);
        connect(project_, &QObject::destroyed, this, [this] {
            retire("The native project was closed; scientific evidence was retired."); emit changed();
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
}
StorageScienceController::~StorageScienceController() {
    const auto wasWaiting = waiting_; waiting_ = false;
    if (worker_) disconnect(worker_, nullptr, this, nullptr);
    if (wasWaiting && worker_) worker_->cancel();
}
QString StorageScienceController::status() const { return waiting_ && worker_ ? worker_->status() : status_; }
bool StorageScienceController::canRun() const {
    return project_ && worker_ && !busy() && !worker_->busy() && static_cast<bool>(project_->capturedForecast());
}
void StorageScienceController::refresh() {
    if (captured_ && (!project_ || project_->capturedForecast() != captured_))
        retire("The captured native run changed; run a fresh PyTorch comparison.");
    emit changed();
}
void StorageScienceController::retire(const QString& message) {
    ++generation_; result_ = {}; tank_.clear(); captured_.reset(); status_ = message;
    const auto wasWaiting = waiting_; waiting_ = false;
    if (wasWaiting && worker_) worker_->cancel();
}
void StorageScienceController::run() {
    if (!canRun()) return;
    captured_ = project_->capturedForecast();
    ++generation_; waiting_ = true; result_ = {}; tank_.clear();
    status_ = "Starting the captured native/PyTorch comparison…";
    const auto request = payload(*captured_);
    const QPointer<StorageScienceController> guard(this);
    worker_->runStorage(request);
    if (guard) emit changed();
}
void StorageScienceController::cancel() {
    if (!busy()) return;
    retire(comparing_ ? "Cancelled. The bounded comparison is finishing." : "Scientific comparison cancelled.");
    emit changed();
}
void StorageScienceController::accept(const QJsonObject& response) {
    if (!waiting_ || !captured_) return;
    if (!project_ || project_->capturedForecast() != captured_) { refresh(); return; }
    waiting_ = false; comparing_ = true;
    status_ = "Comparing returned samples with the retained native trajectory…";
    const auto captured = captured_; const auto epoch = generation_;
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
            emit changed();
            if (guard && epoch == generation_ && project_ && project_->capturedForecast() == captured && !result_.isEmpty()) emit completed(evidence);
            return;
        }
        if (status_ == "Cancelled. The bounded comparison is finishing.") status_ = "Scientific comparison cancelled.";
        emit changed();
    });
    watcher->setFuture(QtConcurrent::run([captured, response] {
        try { return compare(captured, response); }
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

#include "EogDatasetController.hpp"
#include <QFutureWatcher>
#include <QJsonArray>
#include <QtConcurrentRun>
#include <algorithm>
#include <cmath>

namespace breakwater::desktop::eog {
namespace {
struct LoadResult { std::shared_ptr<const Dataset> data; QString error; };
const Sample* sampleAt(const Series& series, qint64 cursor) {
    const auto end = std::upper_bound(series.samples.cbegin(), series.samples.cend(), cursor,
        [](qint64 time, const Sample& value) { return time < value.timestamp; });
    return end == series.samples.cbegin() ? nullptr : &*(end - 1);
}
QString primaryMetric(const QString& kind) {
    if (kind == "well") return "water_bpd";
    if (kind == "tank") return "level_pct";
    if (kind == "pump") return "status";
    if (kind == "pipeline") return "receive_flow_bpd";
    return "inflow_bpd";
}
}
EogDatasetController::EogDatasetController(QObject* parent) : QObject(parent) {}
EogDatasetController::~EogDatasetController() { if (cancellation_) cancellation_->store(true); }
void EogDatasetController::loadFolder(const QUrl& folder) {
    if (!folder.isLocalFile()) { error_ = "Choose a local dataset folder."; emit changed(); emit failed(error_); return; }
    if (cancellation_) cancellation_->store(true);
    cancellation_ = std::make_shared<std::atomic_bool>(false);
    const auto generation = ++generation_; const auto token = cancellation_; const auto path = folder.toLocalFile();
    busy_ = true; error_.clear(); status_ = "Reading the three EOG input files..."; emit changed();
    auto* watcher = new QFutureWatcher<LoadResult>(this);
    connect(watcher, &QFutureWatcher<LoadResult>::finished, this, [this, watcher, generation] {
        const auto result = watcher->result(); watcher->deleteLater();
        if (generation != generation_) return;
        busy_ = false;
        if (!result.data) { error_ = result.error; status_ = "Dataset not replaced."; emit changed(); emit failed(error_); return; }
        dataset_ = result.data; error_.clear(); cursor_ = dataset_->now_unix;
        system_id_ = dataset_->systems.first().id;
        facility_id_ = dataset_->systems.first().facility_ids.first();
        asset_id_ = dataset_->facilities.value(facility_id_).tank_id; metric_ = "level_pct";
        status_ = QString("Loaded %1 readings from %2 systems. Read-only data; detection and EOG planning are not connected.")
            .arg(dataset_->row_count).arg(dataset_->systems.size());
        emit changed(); emit completed();
    });
    watcher->setFuture(QtConcurrent::run([path, token] {
        LoadResult result;
        try { result.data = load_dataset(path, token); }
        catch (const std::exception& error) { result.error = QString::fromUtf8(error.what()); }
        return result;
    }));
}
void EogDatasetController::cancel() {
    if (cancellation_) cancellation_->store(true);
    ++generation_; busy_ = false; error_.clear(); status_ = "Dataset load cancelled; previous data retained."; emit changed();
}
QVariantList EogDatasetController::systems() const {
    QVariantList rows;
    if (dataset_) for (const auto& system : dataset_->systems)
        rows.append(QVariantMap{{"id", system.id}, {"label", system.id}, {"facility_count", system.facility_ids.size()}});
    return rows;
}
QVariantList EogDatasetController::facilities() const {
    QVariantList rows;
    if (dataset_) for (const auto& facility : dataset_->facilities) if (facility.system_id == system_id_)
        rows.append(QVariantMap{{"id", facility.id}, {"label", facility.id}, {"system_id", facility.system_id}});
    return rows;
}
QVariantMap EogDatasetController::observation(const QString& asset, const QString& metric) const {
    QVariantMap result{{"available", false}, {"missing", true}, {"asset_id", asset}, {"metric", metric}};
    if (!dataset_) return result;
    const auto found = dataset_->series.constFind(series_key(asset, metric));
    if (found == dataset_->series.cend()) return result;
    result["unit"] = found->unit;
    const auto* sample = sampleAt(*found, cursor_);
    if (!sample) return result;
    const auto age = cursor_ - sample->timestamp;
    result["value"] = sample->value; result["event_unix"] = sample->timestamp; result["age_s"] = age;
    result["available"] = age <= 300; result["missing"] = false; result["stale"] = age > 300;
    result["status_label"] = age > 300 ? "stale observation" : "recorded observation";
    if (metric == "status") {
        const auto kind = dataset_->assets.value(asset).kind;
        result["status_label"] = kind == "pump" ? (sample->value == 0 ? "stopped" : sample->value == 1 ? "running" : "fault feedback")
                                                     : (sample->value == 0 ? "shut in" : "producing");
    }
    return result;
}
QVariantList EogDatasetController::assets() const {
    QVariantList rows;
    if (!dataset_ || !dataset_->facilities.contains(facility_id_)) return rows;
    const auto facility = dataset_->facilities.value(facility_id_);
    const auto add = [&](const QString& id, double x, double z) {
        const auto asset = dataset_->assets.value(id); if (asset.id.isEmpty()) return;
        const auto metric = primaryMetric(asset.kind); const auto value = observation(id, metric);
        QVariantMap row{{"id", id}, {"label", id}, {"kind", asset.kind}, {"x", x}, {"z", z},
            {"selected", id == asset_id_}, {"unknown", !value.value("available").toBool()}, {"unit", value.value("unit")},
            {"observation", value}, {"available", value.value("available")}, {"display_only", true}};
        if (value.contains("value")) row["value"] = value.value("value");
        if (asset.kind == "tank") {
            const auto capacity = asset.definition.value("capacity_bbl").toDouble(); row["capacity"] = capacity;
            if (value.contains("value")) {
                const auto volume = value.value("value").toDouble() / 100 * capacity;
                if (std::isfinite(volume)) row["volume"] = volume;
                else { row["unknown"] = true; row["volume_unavailable_reason"] = "Percentage-to-volume display exceeds finite numeric range."; }
            }
            row["volume_unit"] = "bbl";
        }
        rows.append(row);
    };
    add(facility.id, -300, -130);
    for (qsizetype index = 0; index < facility.well_ids.size(); ++index)
        add(facility.well_ids[index], -260, (double(index) - double(facility.well_ids.size() - 1) / 2) * 90);
    add(facility.tank_id, -90, 0); add(facility.pump_id, 40, 0); add(facility.pipeline_id, 160, 0);
    for (const auto& system : dataset_->systems) if (system.id == system_id_) add(system.disposal_id, 290, 0);
    return rows;
}
QVariantList EogDatasetController::routes() const {
    QVariantList rows; const auto nodes = assets(); QMap<QString, QVariantMap> positions;
    for (const auto& node : nodes) positions.insert(node.toMap().value("id").toString(), node.toMap());
    if (!dataset_ || !dataset_->facilities.contains(facility_id_)) return rows;
    const auto facility = dataset_->facilities.value(facility_id_);
    const auto add = [&](const QString& from, const QString& to) {
        const auto a = positions.value(from), b = positions.value(to);
        if (a.isEmpty() || b.isEmpty()) return;
        rows.append(QVariantMap{{"id", "display:" + series_key(from, to)}, {"source", from}, {"target", to},
            {"sx", a.value("x")}, {"sz", a.value("z")}, {"tx", b.value("x")}, {"tz", b.value("z")},
            {"available", true}, {"flow", 0}, {"display_only", true}});
    };
    for (const auto& well : facility.well_ids) add(well, facility.tank_id);
    add(facility.tank_id, facility.pump_id); add(facility.pump_id, facility.pipeline_id);
    for (const auto& system : dataset_->systems) if (system.id == system_id_) add(facility.pipeline_id, system.disposal_id);
    return rows;
}
QVariantList EogDatasetController::metrics() const {
    QVariantList rows;
    if (dataset_) for (const auto& series : dataset_->series) if (series.asset_id == asset_id_)
        rows.append(QVariantMap{{"id", series.metric}, {"label", series.metric + " (" + series.unit + ")"}, {"unit", series.unit}});
    return rows;
}
QVariantList EogDatasetController::selectedSeries() const {
    QVariantList rows;
    if (!dataset_) return rows;
    const auto found = dataset_->series.constFind(seriesId()); if (found == dataset_->series.cend()) return rows;
    const auto end = std::upper_bound(found->samples.cbegin(), found->samples.cend(), cursor_,
        [](qint64 time, const Sample& value) { return time < value.timestamp; });
    const qsizetype count = end - found->samples.cbegin(); if (count == 0) return rows;
    const auto append = [&](const Sample& sample) {
        rows.append(QVariantMap{{"sensor_id", seriesId()}, {"event_time_s", double(sample.timestamp)}, {"value", sample.value},
            {"quality", "good"}, {"unit", found->unit}});
    };
    // At most8192 points; when needed preserve each bucket's min/max in time order.
    if (count <= 8192) for (auto it = found->samples.cbegin(); it != end; ++it) append(*it);
    else {
        append(found->samples.first()); const auto width = (count + 4094) / 4095;
        for (qsizetype offset = 1; offset < count - 1; offset += width) {
            const auto finish = std::min(offset + width, count - 1);
            auto low = offset, high = offset;
            for (auto index = offset + 1; index < finish; ++index) {
                if (found->samples[index].value < found->samples[low].value) low = index;
                if (found->samples[index].value > found->samples[high].value) high = index;
            }
            if (low > high) std::swap(low, high);
            append(found->samples[low]); if (high != low) append(found->samples[high]);
        }
        append(*(end - 1));
    }
    return rows;
}
QVariantMap EogDatasetController::selectedValues() const { return observation(asset_id_, metric_); }
QString EogDatasetController::unit() const {
    if (!dataset_) return {}; const auto found = dataset_->series.constFind(seriesId());
    return found == dataset_->series.cend() ? QString{} : found->unit;
}
QVariantMap EogDatasetController::opsSummary() const {
    if (!dataset_) return {};
    const auto prices = dataset_->ops.value("prices").toObject(), trucking = dataset_->ops.value("emergency_trucking").toObject();
    const auto system = dataset_->ops.value("systems").toObject().value(system_id_).toObject();
    const auto facility = system.value("facilities").toObject().value(facility_id_).toObject();
    return {{"oil_per_bbl", prices.value("oil_per_bbl").toVariant()}, {"gas_per_mcf", prices.value("gas_per_mcf").toVariant()},
        {"repair_hours", dataset_->ops.value("expected_repair_hours").toObject().value("pump_failure").toVariant()},
        {"truck_capacity_bbl", trucking.value("truck_capacity_bbl").toVariant()}, {"cost_per_trip", trucking.value("cost_per_trip").toVariant()},
        {"fleet_size", system.value("emergency_fleet_size").toVariant()}, {"response_min", facility.value("response_min").toVariant()},
        {"cycle_min", facility.value("cycle_min").toVariant()}, {"planning_status", "unavailable: EOG planner not connected"}};
}
void EogDatasetController::selectSystem(const QString& id) {
    if (!dataset_) return;
    for (const auto& system : dataset_->systems) if (system.id == id) {
        system_id_ = id; facility_id_ = system.facility_ids.first();
        asset_id_ = dataset_->facilities.value(facility_id_).tank_id; metric_ = "level_pct"; emit changed(); return;
    }
}
void EogDatasetController::selectFacility(const QString& id) {
    if (!dataset_ || !dataset_->facilities.contains(id)) return;
    const auto facility = dataset_->facilities.value(id); system_id_ = facility.system_id; facility_id_ = id;
    asset_id_ = facility.tank_id; metric_ = "level_pct"; emit changed();
}
void EogDatasetController::selectAsset(const QString& id) {
    if (!dataset_ || !dataset_->assets.contains(id)) return;
    const auto asset = dataset_->assets.value(id);
    if (asset.system_id != system_id_ || (!asset.facility_id.isEmpty() && asset.facility_id != facility_id_)) return;
    asset_id_ = id; metric_ = primaryMetric(asset.kind);
    if (!dataset_->series.contains(seriesId())) {
        metric_.clear(); for (const auto& series : dataset_->series) if (series.asset_id == id) { metric_ = series.metric; break; }
    }
    emit changed();
}
void EogDatasetController::selectMetric(const QString& metric) {
    if (!dataset_ || !dataset_->series.contains(series_key(asset_id_, metric))) return;
    metric_ = metric; emit changed();
}
void EogDatasetController::setCursorUnix(double timestamp) {
    if (!dataset_ || !std::isfinite(timestamp) || timestamp < dataset_->first_unix || timestamp > dataset_->now_unix) return;
    cursor_ = static_cast<qint64>(timestamp); emit changed();
}
} // namespace breakwater::desktop::eog

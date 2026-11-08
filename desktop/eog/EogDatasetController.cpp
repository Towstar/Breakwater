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
        scene_mode_ = "overview";
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
    ++generation_; busy_ = false; error_.clear(); status_ = "Dataset load cancelled; previous data retained."; emit changed(); emit cancelled();
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
    QVariantMap result{{"available", false}, {"missing", true}, {"has_sample", false},
        {"freshness_label", "missing"}, {"status_label", ""}, {"asset_id", asset}, {"metric", metric}};
    if (!dataset_) return result;
    const auto found = dataset_->series.constFind(series_key(asset, metric));
    if (found == dataset_->series.cend()) return result;
    result["unit"] = found->unit;
    const auto* sample = sampleAt(*found, cursor_);
    if (!sample) return result;
    const auto age = cursor_ - sample->timestamp;
    result["value"] = sample->value; result["event_unix"] = sample->timestamp; result["age_s"] = age;
    result["available"] = age <= 300; result["missing"] = false; result["has_sample"] = true;
    result["stale"] = age > 300; result["freshness_label"] = age > 300 ? "stale" : "fresh";
    result["quality"] = "recorded";
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
            {"system_id", asset.system_id}, {"facility_id", asset.facility_id}, {"definition", asset.definition.toVariantMap()},
            {"position_source", "schematic facility equipment layout"}, {"has_sample", value.value("has_sample")},
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
            const auto limits = dataset_->network.value("defaults").toObject().value("tank_limits_pct").toObject();
            row["warning_pct"] = limits.value("warning").toVariant(); row["shutdown_pct"] = limits.value("shutdown").toVariant();
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
            {"connected", true}, {"flow_available", false}, {"display_only", true}});
    };
    for (const auto& well : facility.well_ids) add(well, facility.tank_id);
    add(facility.tank_id, facility.pump_id); add(facility.pump_id, facility.pipeline_id);
    for (const auto& system : dataset_->systems) if (system.id == system_id_) add(facility.pipeline_id, system.disposal_id);
    return rows;
}
QVariantList EogDatasetController::overviewNodes() const {
    QVariantList rows;
    if (!dataset_) return rows;
    const auto networkSystems = dataset_->network.value("systems").toArray();
    for (const auto& value : networkSystems) {
        const auto system = value.toObject(); const auto systemId = system.value("id").toString();
        const auto add = [&](const QString& id, const QString& kind, double latitude, double longitude,
                             const QString& facilityId = QString{}) {
            QVariantMap row{{"id", id}, {"label", id}, {"kind", kind}, {"system_id", systemId},
                {"facility_id", facilityId}, {"latitude", latitude}, {"longitude", longitude},
                {"selected", kind == "facility" && facilityId == facility_id_}, {"display_only", true},
                {"position_source", "geographic projection"}, {"unknown", true}};
            if (kind == "facility") {
                const auto facility = dataset_->facilities.value(facilityId);
                const auto measured = observation(facility.tank_id, "level_pct");
                row["observation"] = measured; row["unknown"] = !measured.value("available").toBool();
                row["has_sample"] = measured.value("has_sample"); row["unit"] = measured.value("unit");
                if (measured.contains("value")) row["value"] = measured.value("value");
            }
            rows.append(row);
        };
        for (const auto& entry : system.value("facilities").toArray()) {
            const auto facility = entry.toObject(); const auto id = facility.value("id").toString();
            add(id, "facility", facility.value("lat").toDouble(), facility.value("lon").toDouble(), id);
        }
        const auto disposal = system.value("disposal_site").toObject();
        add(disposal.value("id").toString(), "disposal", disposal.value("lat").toDouble(), disposal.value("lon").toDouble());
        const auto yard = system.value("truck_yard").toArray();
        // The yard is a location, not an input equipment ID or measured channel.
        auto yardId = "presentation:truck-yard:" + systemId;
        while (dataset_->assets.contains(yardId)) yardId.prepend("presentation:");
        add(yardId, "truck_yard", yard[0].toDouble(), yard[1].toDouble());
        auto yardRow = rows.last().toMap(); yardRow["label"] = systemId + " truck yard";
        yardRow["synthetic_id"] = true; rows.last() = yardRow;
    }
    if (rows.isEmpty()) return rows;
    double minLatitude = 90, maxLatitude = -90, minLongitude = 180, maxLongitude = -180;
    for (const auto& entry : rows) {
        const auto row = entry.toMap(); const auto lat = row.value("latitude").toDouble(), lon = row.value("longitude").toDouble();
        minLatitude = std::min(minLatitude, lat); maxLatitude = std::max(maxLatitude, lat);
        minLongitude = std::min(minLongitude, lon); maxLongitude = std::max(maxLongitude, lon);
    }
    const auto centerLatitude = (minLatitude + maxLatitude) / 2, centerLongitude = (minLongitude + maxLongitude) / 2;
    // Local equirectangular presentation only; it supplies no travel/physics inputs.
    const auto longitudeScale = std::cos(centerLatitude * std::acos(-1.0) / 180.0);
    const auto spanKm = std::max((maxLatitude - minLatitude) * 111.32,
        (maxLongitude - minLongitude) * longitudeScale * 111.32);
    const auto displayScale = 1000 / std::max(1.0, spanKm);
    QMap<QString, int> coincident;
    for (auto& entry : rows) {
        auto row = entry.toMap();
        auto x = (row.value("longitude").toDouble() - centerLongitude) * longitudeScale * 111.32 * displayScale;
        auto z = (centerLatitude - row.value("latitude").toDouble()) * 111.32 * displayScale;
        const auto coordinate = QString::number(x, 'g', 17) + ',' + QString::number(z, 'g', 17);
        const auto duplicate = coincident[coordinate]++;
        if (duplicate > 0) {
            const auto radius = 45 * std::sqrt(double(duplicate)), angle = 2.399963229728653 * duplicate;
            x += radius * std::cos(angle); z += radius * std::sin(angle);
            row["position_source"] = "geographic projection with schematic separation of coincident locations";
        }
        row["x"] = x; row["z"] = z; entry = row;
    }
    return rows;
}
QVariantList EogDatasetController::overviewRoutes() const {
    QVariantList rows; QMap<QString, QVariantMap> positions;
    for (const auto& entry : overviewNodes()) { const auto row = entry.toMap(); positions.insert(row.value("id").toString(), row); }
    if (!dataset_) return rows;
    const auto add = [&](const QString& from, const QString& to, const QString& kind, const QString& label,
                         const QString& systemId, const QString& facilityId) {
        const auto a = positions.value(from), b = positions.value(to);
        if (a.isEmpty() || b.isEmpty()) return;
        QVariantMap row{{"id", "display:" + series_key(from, to)}, {"source", from}, {"target", to},
            {"sx", a.value("x")}, {"sz", a.value("z")}, {"tx", b.value("x")}, {"tz", b.value("z")},
            {"route_kind", kind}, {"label", label}, {"system_id", systemId}, {"facility_id", facilityId},
            {"display_only", true}, {"flow_available", false}, {"connected", true}};
        if (kind == "truck_access") {
            const auto operation = dataset_->ops.value("systems").toObject().value(systemId).toObject()
                .value("facilities").toObject().value(facilityId).toObject();
            row["response_min"] = operation.value("response_min").toVariant();
            row["cycle_min"] = operation.value("cycle_min").toVariant();
        }
        rows.append(row);
    };
    for (const auto& system : dataset_->systems) {
        QString yardId;
        for (const auto& row : positions) if (row.value("system_id").toString() == system.id && row.value("kind") == "truck_yard") {
            yardId = row.value("id").toString(); break;
        }
        for (const auto& facilityId : system.facility_ids) {
            const auto facility = dataset_->facilities.value(facilityId);
            add(facilityId, system.disposal_id, "pipeline", facility.pipeline_id, system.id, facilityId);
            add(yardId, facilityId, "truck_access", "Truck access (ops.json travel time)", system.id, facilityId);
        }
    }
    return rows;
}
QVariantList EogDatasetController::sceneNodes() const { return scene_mode_ == "overview" ? overviewNodes() : assets(); }
QVariantList EogDatasetController::sceneRoutes() const { return scene_mode_ == "overview" ? overviewRoutes() : routes(); }
QVariantMap EogDatasetController::sceneBounds() const {
    const auto nodes = sceneNodes();
    if (nodes.isEmpty()) return {{"center_x", 0}, {"center_z", 0}, {"width", 800}, {"depth", 600}, {"radius", 500}};
    const auto first = nodes.first().toMap();
    double minX = first.value("x").toDouble(), maxX = minX, minZ = first.value("z").toDouble(), maxZ = minZ;
    for (const auto& entry : nodes) {
        const auto row = entry.toMap(); const auto x = row.value("x").toDouble(), z = row.value("z").toDouble();
        minX = std::min(minX, x); maxX = std::max(maxX, x); minZ = std::min(minZ, z); maxZ = std::max(maxZ, z);
    }
    const auto width = std::max(400.0, maxX - minX + 160), depth = std::max(300.0, maxZ - minZ + 160);
    return {{"center_x", (minX + maxX) / 2}, {"center_z", (minZ + maxZ) / 2},
        {"width", width}, {"depth", depth}, {"radius", std::hypot(width, depth) / 2}};
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
    qsizetype previousIndex = -1;
    const auto append = [&](qsizetype index) {
        const auto& sample = found->samples[index];
        const auto gap = std::upper_bound(found->gap_start_indices.cbegin(), found->gap_start_indices.cend(), previousIndex);
        const auto breakBefore = previousIndex >= 0 && gap != found->gap_start_indices.cend() && *gap <= index;
        rows.append(QVariantMap{{"sensor_id", seriesId()}, {"event_time_s", double(sample.timestamp)}, {"value", sample.value},
            {"quality", "recorded"}, {"unit", found->unit}, {"break_before", breakBefore},
            {"source_index", index}, {"omitted_samples_before", previousIndex < 0 ? 0 : index - previousIndex - 1}});
        previousIndex = index;
    };
    // At most 8192 points; each retained connection is checked against the actual
    // source gaps, never against the larger time difference caused by decimation.
    if (count <= 8192) for (qsizetype index = 0; index < count; ++index) append(index);
    else {
        append(0); const auto width = (count - 2 + 4094) / 4095;
        for (qsizetype offset = 1; offset < count - 1; offset += width) {
            const auto finish = std::min(offset + width, count - 1);
            auto low = offset, high = offset;
            for (auto index = offset + 1; index < finish; ++index) {
                if (found->samples[index].value < found->samples[low].value) low = index;
                if (found->samples[index].value > found->samples[high].value) high = index;
            }
            if (low > high) std::swap(low, high);
            append(low); if (high != low) append(high);
        }
        append(count - 1);
    }
    return rows;
}
QVariantList EogDatasetController::selectedThresholds() const {
    if (!dataset_ || metric_ != "level_pct") return {};
    const auto asset = dataset_->assets.constFind(asset_id_);
    if (asset == dataset_->assets.cend() || asset->kind != "tank") return {};
    const auto limits = dataset_->network.value("defaults").toObject().value("tank_limits_pct").toObject();
    return {QVariantMap{{"kind", "warning"}, {"label", "Warning"}, {"value", limits.value("warning").toDouble()}, {"unit", "%"}},
        QVariantMap{{"kind", "shutdown"}, {"label", "Shutdown"}, {"value", limits.value("shutdown").toDouble()}, {"unit", "%"}},
        QVariantMap{{"kind", "capacity"}, {"label", "Capacity"}, {"value", 100.0}, {"unit", "%"}}};
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
        system_id_ = id; facility_id_ = system.facility_ids.first(); scene_mode_ = "overview";
        asset_id_ = dataset_->facilities.value(facility_id_).tank_id; metric_ = "level_pct"; emit changed(); return;
    }
}
void EogDatasetController::selectFacility(const QString& id) {
    if (!dataset_ || !dataset_->facilities.contains(id)) return;
    const auto facility = dataset_->facilities.value(id); system_id_ = facility.system_id; facility_id_ = id; scene_mode_ = "facility";
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
void EogDatasetController::showOverview() {
    if (scene_mode_ == "overview") return;
    scene_mode_ = "overview"; emit changed();
}
void EogDatasetController::showFacility(const QString& id) { selectFacility(id); }
void EogDatasetController::pickSceneNode(const QString& id) {
    if (!dataset_) return;
    const auto asset = dataset_->assets.constFind(id);
    if (asset == dataset_->assets.cend()) return; // Presentation-only yard has no measurements.
    if (asset->kind == "facility") { showFacility(id); return; }
    if (!asset->facility_id.isEmpty()) {
        const auto facility = dataset_->facilities.value(asset->facility_id);
        system_id_ = facility.system_id; facility_id_ = facility.id; scene_mode_ = "facility";
    } else {
        system_id_ = asset->system_id;
        if (dataset_->facilities.value(facility_id_).system_id != system_id_)
            for (const auto& system : dataset_->systems) if (system.id == system_id_) {
                facility_id_ = system.facility_ids.first(); break;
            }
    }
    selectAsset(id);
}
} // namespace breakwater::desktop::eog

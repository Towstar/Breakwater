#pragma once
#include <QJsonObject>
#include <QMap>
#include <QStringList>
#include <QVector>
#include <atomic>
#include <memory>

namespace breakwater::desktop::eog {
struct Sample { qint64 timestamp = 0; double value = 0; };
struct Series { QString asset_id, metric, unit; QVector<Sample> samples; };
struct Asset { QString id, kind, system_id, facility_id; QJsonObject definition; };
struct Facility {
    QString id, system_id, tank_id, pump_id, pipeline_id;
    QStringList well_ids;
    double latitude = 0, longitude = 0;
};
struct System { QString id, disposal_id; QStringList facility_ids; };
struct Dataset {
    QJsonObject network, ops, source_hashes;
    QMap<QString, Asset> assets;
    QVector<System> systems;
    QMap<QString, Facility> facilities;
    QMap<QString, Series> series;
    QString folder, identity;
    QStringList warnings;
    qint64 first_unix = 0, now_unix = 0;
    qsizetype row_count = 0;
};
inline constexpr qint64 maximum_csv_bytes = 128 * 1024 * 1024;
inline constexpr qsizetype maximum_rows = 2'000'000;
inline constexpr qsizetype maximum_assets = 32768;
inline constexpr qsizetype maximum_series = 65536;
QString series_key(const QString& asset, const QString& metric);
std::shared_ptr<const Dataset> load_dataset(const QString& folder,
                                          const std::shared_ptr<std::atomic_bool>& cancelled = {});
} // namespace breakwater::desktop::eog

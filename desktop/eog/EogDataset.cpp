#include "EogDataset.hpp"
#include "telemetry/TelemetryContracts.hpp"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QRegularExpression>
#include <QSet>
#include <QStringConverter>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace breakwater::desktop::eog {
namespace {
[[noreturn]] void invalid(const QString& message) { throw std::invalid_argument(message.toStdString()); }
void checkCancellation(const std::shared_ptr<std::atomic_bool>& token) {
    if (token && token->load(std::memory_order_relaxed)) throw std::runtime_error("Dataset loading cancelled.");
}
QString id(const QJsonObject& o, const char* key = "id") {
    const auto v = o.value(QLatin1String(key));
    if (!v.isString() || v.toString().isEmpty() || v.toString().size() > 256) invalid("Invalid dataset identifier.");
    for (const auto c : v.toString()) if (c.isNull() || c.category() == QChar::Other_Control) invalid("Control characters in dataset identifier.");
    return v.toString();
}
double number(const QJsonObject& o, const char* key, double minimum = 0.0, bool strictlyPositive = false) {
    const auto v = o.value(QLatin1String(key));
    if (!v.isDouble() || !std::isfinite(v.toDouble()) || v.toDouble() < minimum || (strictlyPositive && v.toDouble() <= 0))
        invalid(QString("Invalid numeric field: %1").arg(QLatin1String(key)));
    return v.toDouble();
}
QJsonObject object(const QJsonObject& o, const char* key) {
    if (!o.value(QLatin1String(key)).isObject()) invalid(QString("Missing object: %1").arg(QLatin1String(key)));
    return o.value(QLatin1String(key)).toObject();
}
QJsonArray array(const QJsonObject& o, const char* key, qsizetype maximum) {
    const auto v = o.value(QLatin1String(key));
    if (!v.isArray() || v.toArray().isEmpty() || v.toArray().size() > maximum) invalid(QString("Invalid array: %1").arg(QLatin1String(key)));
    for (const auto& entry : v.toArray()) if (!entry.isObject()) invalid("Dataset array entries must be objects.");
    return v.toArray();
}
void location(const QJsonObject& o) {
    const auto lat = number(o, "lat", -90), lon = number(o, "lon", -180);
    if (lat > 90 || lon > 180) invalid("Latitude/longitude outside supported range.");
}
void pairLocation(const QJsonObject& o, const char* key) {
    const auto a = o.value(QLatin1String(key)).toArray();
    if (a.size() != 2 || !a[0].isDouble() || !a[1].isDouble() || !std::isfinite(a[0].toDouble()) ||
        !std::isfinite(a[1].toDouble()) || std::abs(a[0].toDouble()) > 90 || std::abs(a[1].toDouble()) > 180)
        invalid("Invalid system map location.");
}
QFileInfo checkedFile(const QString& folder, const QString& name, qint64 maximum) {
    const QFileInfo info(QDir(folder).filePath(name));
    if (!info.isFile() || info.isSymLink() || info.size() <= 0 || info.size() > maximum ||
        QFileInfo(info.canonicalFilePath()).absolutePath() != folder || QFileInfo(info.canonicalFilePath()).fileName() != name)
        invalid(QString("Cannot read bounded local %1.").arg(name));
    return info;
}
QByteArray readJson(const QString& folder, const QString& name, QJsonObject& hashes) {
    const auto info = checkedFile(folder, name, 4 * 1024 * 1024);
    QFile file(info.absoluteFilePath());
    if (!file.open(QIODevice::ReadOnly)) invalid(QString("Cannot open %1.").arg(name));
    const auto bytes = file.read(4 * 1024 * 1024 + 1);
    if (bytes.size() != info.size() || bytes.size() > 4 * 1024 * 1024 || !file.atEnd()) invalid("Dataset JSON changed or exceeded its size limit.");
    const QFileInfo after(info.absoluteFilePath());
    if (after.size() != info.size() || after.lastModified() != info.lastModified()) invalid("Dataset changed during loading.");
    hashes[name] = QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
    return bytes;
}
QStringList csvFields(QString text) {
    if (text.endsWith('\n')) text.chop(1);
    if (text.endsWith('\r')) text.chop(1);
    QStringList result; QString field; bool quoted = false, closed = false;
    for (qsizetype i = 0; i < text.size(); ++i) {
        const auto c = text[i];
        if (quoted) {
            if (c == '"') {
                if (i + 1 < text.size() && text[i + 1] == '"') { field += '"'; ++i; }
                else { quoted = false; closed = true; }
            } else field += c;
        } else if (c == ',') { result.append(field); field.clear(); closed = false; }
        else if (c == '"' && field.isEmpty() && !closed) quoted = true;
        else {
            if (closed || c == '"' || c == '\r' || c == '\n') invalid("Malformed CSV quoting.");
            field += c;
        }
        if (field.size() > 1024 || result.size() > 5) invalid("CSV field limit exceeded.");
    }
    if (quoted) invalid("Unterminated or multiline CSV field.");
    result.append(field);
    if (result.size() != 5) invalid("Telemetry rows require exactly five columns.");
    return result;
}
QString metricUnit(const QString& kind, const QString& metric) {
    if (kind == "facility" && metric == "inflow_bpd") return "bpd";
    if (kind == "well" && metric == "water_bpd") return "bpd";
    if ((kind == "well" || kind == "pump") && metric == "status") return "state";
    if (kind == "tank" && metric == "level_pct") return "%";
    if (kind == "pump" && metric == "flow_bpd") return "bpd";
    if (kind == "pump" && metric == "discharge_psi") return "psi";
    if (kind == "pipeline" && metric == "receive_flow_bpd") return "bpd";
    if (kind == "pipeline" && metric == "pressure_psi") return "psi";
    invalid("Unknown asset/metric combination in telemetry.");
}
QStringList expectedMetrics(const QString& kind) {
    if (kind == "facility") return {"inflow_bpd"};
    if (kind == "well") return {"water_bpd", "status"};
    if (kind == "tank") return {"level_pct"};
    if (kind == "pump") return {"status", "flow_bpd", "discharge_psi"};
    if (kind == "pipeline") return {"receive_flow_bpd", "pressure_psi"};
    return {};
}
}
QString series_key(const QString& asset, const QString& metric) {
    // Length-prefixing avoids collisions when imported identifiers contain punctuation.
    return QString::number(asset.size()) + ":" + asset + ":" + metric;
}
std::shared_ptr<const Dataset> load_dataset(const QString& input, const std::shared_ptr<std::atomic_bool>& cancelled) {
    checkCancellation(cancelled);
    const QFileInfo directory(input);
    if (!directory.isDir()) invalid("Choose an existing local dataset folder.");
    auto data = std::make_shared<Dataset>(); data->folder = directory.canonicalFilePath();
    data->network = telemetry::decode_document(readJson(data->folder, "network.json", data->source_hashes));
    data->ops = telemetry::decode_document(readJson(data->folder, "ops.json", data->source_hashes));
    const auto metadata = object(data->network, "meta");
    const auto units = object(metadata, "units");
    if (metadata.value("timezone") != "UTC" || units.value("volume") != "bbl" || units.value("rate") != "bpd" ||
        units.value("gas_rate") != "mcfd" || units.value("pressure") != "psi") invalid("Unsupported dataset units/timezone.");
    const auto limits = object(object(data->network, "defaults"), "tank_limits_pct");
    const auto warning = number(limits, "warning"), shutdown = number(limits, "shutdown");
    if (warning >= shutdown || shutdown > 100) invalid("Tank warning/shutdown limits must be ordered percentages.");
    QSet<QString> identifiers;
    const auto registerId = [&](const QString& value) {
        if (identifiers.contains(value)) invalid("Duplicate equipment or system ID."); identifiers.insert(value);
    };
    const auto addAsset = [&](const QJsonObject& definition, const QString& kind, const QString& system, const QString& facility) {
        const auto value = id(definition); registerId(value);
        if (data->assets.size() >= maximum_assets) invalid("Dataset equipment limit exceeded.");
        data->assets.insert(value, {value, kind, system, facility, definition}); return value;
    };
    for (const auto& value : array(data->network, "systems", 256)) {
        checkCancellation(cancelled); const auto o = value.toObject(); System system; system.id = id(o); registerId(system.id);
        pairLocation(o, "center"); pairLocation(o, "truck_yard");
        const auto disposal = object(o, "disposal_site"); location(disposal); number(disposal, "pressure_psi");
        system.disposal_id = addAsset(disposal, "disposal", system.id, {});
        for (const auto& entry : array(o, "facilities", 1024)) {
            const auto f = entry.toObject(); location(f); Facility facility;
            facility.id = addAsset(f, "facility", system.id, id(f)); facility.system_id = system.id;
            facility.latitude = f.value("lat").toDouble(); facility.longitude = f.value("lon").toDouble();
            for (const auto& wellValue : array(f, "wells", 1024)) {
                const auto well = wellValue.toObject(); number(well, "baseline_water_bpd"); number(well, "oil_bpd"); number(well, "gas_mcfd");
                facility.well_ids.append(addAsset(well, "well", system.id, facility.id));
            }
            const auto tank = object(f, "tank"), pump = object(f, "pump"), pipe = object(f, "pipeline");
            number(tank, "capacity_bbl", 0, true); number(pump, "max_flow_bpd", 0, true);
            const auto start = number(pump, "start_level_pct"), stop = number(pump, "stop_level_pct");
            if (stop >= start || start > 100) invalid("Pump stop/start setpoints must be ordered percentages.");
            number(pipe, "max_bpd", 0, true); number(pipe, "max_pressure_psi", 0, true);
            facility.tank_id = addAsset(tank, "tank", system.id, facility.id);
            facility.pump_id = addAsset(pump, "pump", system.id, facility.id);
            facility.pipeline_id = addAsset(pipe, "pipeline", system.id, facility.id);
            system.facility_ids.append(facility.id); data->facilities.insert(facility.id, facility);
        }
        data->systems.append(system);
    }
    const auto prices = object(data->ops, "prices"), repairs = object(data->ops, "expected_repair_hours"), trucking = object(data->ops, "emergency_trucking");
    number(prices, "oil_per_bbl"); number(prices, "gas_per_mcf"); number(repairs, "pump_failure", 0, true);
    number(trucking, "truck_capacity_bbl", 0, true); number(trucking, "cost_per_trip");
    const auto operations = object(data->ops, "systems");
    if (operations.size() != data->systems.size()) invalid("Operating systems must match the network.");
    for (const auto& system : data->systems) {
        if (!operations.value(system.id).isObject()) invalid("Missing system operating data.");
        const auto o = operations.value(system.id).toObject(); const auto fleet = number(o, "emergency_fleet_size");
        if (std::floor(fleet) != fleet || fleet > 10000) invalid("Invalid truck fleet size.");
        const auto facilities = object(o, "facilities");
        if (facilities.size() != system.facility_ids.size()) invalid("Operating facilities must match the network.");
        for (const auto& name : system.facility_ids) {
            if (!facilities.value(name).isObject()) invalid("Missing facility operating data.");
            const auto operation = facilities.value(name).toObject(); number(operation, "response_min"); number(operation, "cycle_min", 0, true);
        }
    }
    const auto csvInfo = checkedFile(data->folder, "telemetry.csv", maximum_csv_bytes);
    QFile file(csvInfo.absoluteFilePath()); if (!file.open(QIODevice::ReadOnly)) invalid("Cannot open telemetry.csv.");
    QCryptographicHash hash(QCryptographicHash::Sha256);
    auto readLine = [&]() {
        auto line = file.readLine(16 * 1024 + 1);
        if (line.size() > 16 * 1024 || (!line.endsWith('\n') && !file.atEnd())) invalid("Telemetry line exceeds 16 KiB.");
        hash.addData(line); return line;
    };
    auto header = readLine(); if (header.startsWith("\xef\xbb\xbf")) header.remove(0, 3);
    // Physical records cannot contain embedded newlines; do not carry an
    // incomplete final UTF-8 sequence into an imaginary following record.
    QStringDecoder utf8(QStringDecoder::Utf8, QStringConverter::Flag::Stateless);
    if (csvFields(utf8(header)) != QStringList{"timestamp", "ts_unix", "asset_id", "metric", "value"} || utf8.hasError()) invalid("Unexpected telemetry CSV header.");
    qint64 previous = -1; QSet<QString> atTimestamp; bool cadenceGap = false;
    while (!file.atEnd()) {
        if ((data->row_count & 1023) == 0) checkCancellation(cancelled);
        const auto raw = readLine(); const auto text = utf8(raw);
        if (utf8.hasError()) invalid("Telemetry is not valid UTF-8.");
        if (raw.isEmpty()) break;
        const auto fields = csvFields(text); bool timeOk = false, valueOk = false;
        const auto timestamp = fields[1].toLongLong(&timeOk); const auto numeric = fields[4].toDouble(&valueOk);
        static const QRegularExpression utcShape(QStringLiteral("^[0-9]{4}-[0-9]{2}-[0-9]{2}T(?:[01][0-9]|2[0-3]):[0-5][0-9]:[0-5][0-9]Z\\z"));
        const auto utc = QDateTime::fromString(fields[0], Qt::ISODate);
        if (!timeOk || timestamp < 0 || timestamp > 253402300799LL || !utcShape.match(fields[0]).hasMatch() ||
            !utc.isValid() || utc.toMSecsSinceEpoch() != timestamp * 1000 ||
            !valueOk || !std::isfinite(numeric)) invalid("Invalid telemetry timestamp or finite value.");
        if (timestamp < previous) invalid("Telemetry timestamps are not ordered.");
        if (previous >= 0 && timestamp != previous) {
            if ((timestamp - previous) % 300 != 0) invalid("Telemetry does not follow a five-minute clock.");
            if (timestamp - previous != 300) cadenceGap = true;
            atTimestamp.clear();
        }
        const auto asset = data->assets.constFind(fields[2]);
        if (asset == data->assets.cend()) invalid("Telemetry references unknown equipment.");
        const auto unit = metricUnit(asset->kind, fields[3]); const auto key = series_key(fields[2], fields[3]);
        if (atTimestamp.contains(key)) invalid("Duplicate asset/metric/timestamp reading.");
        atTimestamp.insert(key);
        if (fields[3] == "status" && (std::floor(numeric) != numeric || numeric < 0 || numeric > (asset->kind == "pump" ? 2 : 1)))
            invalid("Invalid typed pump/well status.");
        if (!data->series.contains(key)) {
            if (data->series.size() >= maximum_series) invalid("Dataset series limit exceeded.");
            data->series.insert(key, {fields[2], fields[3], unit, {}, {}});
        }
        auto& series = data->series[key];
        if (!series.samples.isEmpty() && timestamp - series.samples.last().timestamp > 300)
            series.gap_start_indices.append(series.samples.size());
        series.samples.append({timestamp, numeric});
        if (++data->row_count > maximum_rows) invalid("Dataset row limit exceeded.");
        if (data->row_count == 1) data->first_unix = timestamp;
        data->now_unix = timestamp; previous = timestamp;
    }
    if (file.error() != QFileDevice::NoError || data->row_count == 0) invalid("No readable telemetry rows.");
    const QFileInfo after(csvInfo.absoluteFilePath());
    if (after.size() != csvInfo.size() || after.lastModified() != csvInfo.lastModified() || file.pos() != csvInfo.size()) invalid("Telemetry changed during loading.");
    data->source_hashes["telemetry.csv"] = QString::fromLatin1(hash.result().toHex());
    qsizetype incomplete = 0;
    const auto expectedSamples = (data->now_unix - data->first_unix) / 300 + 1;
    for (const auto& asset : data->assets) for (const auto& metric : expectedMetrics(asset.kind)) {
        const auto series = data->series.constFind(series_key(asset.id, metric));
        if (series == data->series.cend() || series->samples.size() != expectedSamples) ++incomplete;
    }
    if (cadenceGap || incomplete) data->warnings.append(QString("Incomplete coverage: %1 series missing samples; gaps are not filled with zeros.").arg(incomplete));
    QByteArray identity("breakwater-eog-inputs-v1\n");
    for (const auto& name : {QStringLiteral("network.json"), QStringLiteral("ops.json"), QStringLiteral("telemetry.csv")})
        identity += name.toUtf8() + ':' + data->source_hashes.value(name).toString().toLatin1() + '\n';
    data->identity = QString::fromLatin1(QCryptographicHash::hash(identity, QCryptographicHash::Sha256).toHex());
    checkCancellation(cancelled); return data;
}
} // namespace breakwater::desktop::eog

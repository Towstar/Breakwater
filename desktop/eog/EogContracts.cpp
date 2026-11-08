#include "EogContracts.hpp"
#include "telemetry/TelemetryContracts.hpp"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMap>
#include <QPair>
#include <QRegularExpression>
#include <QSet>
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <stdexcept>

static void initializeEogContractResources() { Q_INIT_RESOURCE(eog_contract_schemas); }

namespace breakwater::desktop::eog {
namespace {
void require(bool good, const char* message = "Invalid EOG contract.") {
    if (!good) throw std::invalid_argument(message);
}
double n(const QJsonValue& value) { return value.toDouble(); }
QString t(const QJsonValue& value) { return value.toString(); }
QJsonObject o(const QJsonValue& value) { return value.toObject(); }
QJsonArray a(const QJsonValue& value) { return value.toArray(); }
QString digest(const QByteArray& bytes) {
    return QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}
void inspect(const QJsonValue& value, int depth = 0) {
    require(depth <= 64, "EOG nesting exceeds 64 levels.");
    if (value.isDouble()) require(std::isfinite(n(value)), "EOG number is nonfinite.");
    if (value.isString()) require(QString::fromUtf8(t(value).toUtf8()) == t(value), "Invalid EOG Unicode.");
    if (value.isArray()) for (const auto& item : a(value)) inspect(item, depth + 1);
    if (value.isObject()) {
        const auto object = o(value);
        for (auto it = object.begin(); it != object.end(); ++it) {
            inspect(it.key(), depth + 1); inspect(it.value(), depth + 1);
        }
    }
}
const QMap<QString, QJsonObject>& schemas() {
    static const auto documents = [] {
        initializeEogContractResources();
        QMap<QString, QJsonObject> result;
        for (const auto* name : {"common-v1.schema.json", "capture-manifest-v1.schema.json",
                                 "analysis-request-v1.schema.json", "analysis-response-v1.schema.json"}) {
            QFile file(":/breakwater/eog/contracts/" + QString::fromLatin1(name));
            require(file.open(QIODevice::ReadOnly), "Missing embedded EOG schema.");
            result.insert(QString::fromLatin1(name), telemetry::decode_document(file.readAll()));
        }
        return result;
    }();
    return documents;
}

QPair<QString, QJsonObject> schemaReference(const QString& reference, const QString& source) {
    const auto split = reference.split('#');
    require(split.size() <= 2 && !reference.contains("://"), "Only local EOG schema references are permitted.");
    const auto file = split[0].isEmpty() ? source : split[0];
    require(schemas().contains(file), "Unknown EOG schema reference.");
    QJsonValue target = schemas().value(file);
    if (split.size() == 2 && !split[1].isEmpty()) {
        require(split[1].startsWith('/'), "Invalid local EOG JSON pointer.");
        for (const auto& token : split[1].mid(1).split('/')) {
            const auto key = QString(token).replace("~1", "/").replace("~0", "~");
            require(target.isObject() && o(target).contains(key), "Unresolved EOG schema pointer.");
            target = o(target).value(key);
        }
    }
    require(target.isObject(), "EOG schema target is not an object.");
    return {file, o(target)};
}
QJsonValue inlineSchema(const QJsonValue& value, const QString& source, int depth = 0) {
    require(depth <= 128, "EOG schema recursion exceeds its bound.");
    if (value.isArray()) {
        QJsonArray result; for (const auto& item : a(value)) result.append(inlineSchema(item, source, depth + 1));
        return result;
    }
    if (!value.isObject()) return value;
    auto object = o(value);
    if (object.contains("$ref")) {
        const auto reference = schemaReference(t(object.value("$ref")), source);
        const auto expanded = inlineSchema(reference.second, reference.first, depth + 1);
        object.remove("$ref");
        if (object.isEmpty()) return expanded;
        // Reference siblings are conjunctive; merging keys could weaken bounds.
        return QJsonObject{{"allOf", QJsonArray{expanded, inlineSchema(object, source, depth + 1)}}};
    }
    QJsonObject result;
    for (auto it = object.begin(); it != object.end(); ++it)
        if (it.key() != "$schema" && it.key() != "$id" && it.key() != "$defs")
            result[it.key()] = inlineSchema(it.value(), source, depth + 1);
    return result;
}

// Deliberately bounded subset used by these committed schemas. Unknown keywords
// fail closed, so adding a validation keyword requires native support and tests.
bool matches(const QJsonValue& value, const QJsonObject& schema, const QString& source, int depth = 0) {
    require(depth <= 128, "EOG schema recursion exceeds its bound.");
    static const QSet<QString> supported{"$schema", "$id", "$defs", "$ref", "type", "const", "enum",
        "allOf", "anyOf", "if", "then", "else", "properties", "required", "additionalProperties",
        "items", "minItems", "maxItems", "minLength", "maxLength", "pattern",
        "minimum", "maximum", "exclusiveMinimum"};
    for (auto it = schema.begin(); it != schema.end(); ++it)
        require(supported.contains(it.key()), "Unsupported EOG schema keyword.");
    if (schema.contains("$ref")) {
        const auto reference = schemaReference(t(schema.value("$ref")), source);
        if (!matches(value, reference.second, reference.first, depth + 1)) return false;
    }
    if (schema.contains("const") && value != schema.value("const")) return false;
    if (schema.contains("enum") && !a(schema.value("enum")).contains(value)) return false;
    if (schema.contains("type")) {
        const auto type = t(schema.value("type"));
        const bool correct = type == "null" ? value.isNull() :
            type == "boolean" ? value.isBool() : type == "string" ? value.isString() :
            type == "number" ? value.isDouble() : type == "integer" ? value.isDouble() && std::floor(n(value)) == n(value) :
            type == "object" ? value.isObject() : type == "array" ? value.isArray() : false;
        if (!correct) return false;
    }
    for (const auto& part : a(schema.value("allOf")))
        if (!matches(value, o(part), source, depth + 1)) return false;
    if (schema.contains("anyOf")) {
        bool found = false;
        for (const auto& part : a(schema.value("anyOf")))
            if (matches(value, o(part), source, depth + 1)) { found = true; break; }
        if (!found) return false;
    }
    if (schema.contains("if")) {
        const auto branch = matches(value, o(schema.value("if")), source, depth + 1) ? "then" : "else";
        if (schema.contains(branch) && !matches(value, o(schema.value(branch)), source, depth + 1)) return false;
    }
    if (value.isDouble()) {
        if (schema.contains("minimum") && n(value) < n(schema.value("minimum"))) return false;
        if (schema.contains("maximum") && n(value) > n(schema.value("maximum"))) return false;
        if (schema.contains("exclusiveMinimum") && n(value) <= n(schema.value("exclusiveMinimum"))) return false;
    }
    if (value.isString()) {
        const auto count = t(value).toUcs4().size();
        if (schema.contains("minLength") && count < n(schema.value("minLength"))) return false;
        if (schema.contains("maxLength") && count > n(schema.value("maxLength"))) return false;
        if (schema.contains("pattern") && !QRegularExpression(t(schema.value("pattern"))).match(t(value)).hasMatch()) return false;
    }
    if (value.isObject()) {
        const auto object = o(value), properties = o(schema.value("properties"));
        for (const auto& field : a(schema.value("required"))) if (!object.contains(t(field))) return false;
        for (auto it = object.begin(); it != object.end(); ++it) {
            if (properties.contains(it.key())) {
                if (!matches(it.value(), o(properties.value(it.key())), source, depth + 1)) return false;
            } else if (schema.contains("additionalProperties") && !schema.value("additionalProperties").toBool()) return false;
        }
    }
    if (value.isArray()) {
        const auto values = a(value);
        if (schema.contains("minItems") && values.size() < n(schema.value("minItems"))) return false;
        if (schema.contains("maxItems") && values.size() > n(schema.value("maxItems"))) return false;
        if (schema.contains("items"))
            for (const auto& item : values) if (!matches(item, o(schema.value("items")), source, depth + 1)) return false;
    }
    return true;
}
void shape(const QJsonObject& value, const QString& kind) {
    static const QMap<QString, QString> files{{"capture_manifest", "capture-manifest-v1.schema.json"},
        {"analysis_request", "analysis-request-v1.schema.json"}, {"analysis_response", "analysis-response-v1.schema.json"}};
    require(files.contains(kind), "Unsupported EOG document kind.");
    const auto file = files.value(kind);
    require(matches(value, schemas().value(file), file), "EOG schema shape mismatch.");
}
QJsonValue projection(const QJsonValue& value) {
    if (value.isDouble()) {
        const double number = n(value) == 0 ? 0.0 : n(value);
        const auto bits = std::bit_cast<std::uint64_t>(number);
        return QJsonObject{{"$f64", QString::fromLatin1(QByteArray::number(bits, 16).rightJustified(16, '0'))}};
    }
    if (value.isObject()) {
        QJsonObject result; const auto input = o(value);
        for (auto it = input.begin(); it != input.end(); ++it) result[it.key()] = projection(it.value());
        return result;
    }
    if (value.isArray()) { QJsonArray result; for (const auto& item : a(value)) result.append(projection(item)); return result; }
    return value;
}
void unique(const QJsonArray& values, const char* key) {
    QSet<QString> seen;
    for (const auto& value : values) {
        const auto id = t(o(value).value(QLatin1String(key)));
        require(!seen.contains(id), "Duplicate EOG identity."); seen.insert(id);
    }
}
void ids(const QJsonValue& value) {
    if (value.isObject()) {
        const auto object = o(value);
        for (auto it = object.begin(); it != object.end(); ++it) {
            if ((it.key().endsWith("_id") || it.key() == "evaluator_method" ||
                 it.key() == "validator_method" || it.key() == "threshold_version") && it.value().isString())
                require(!t(it.value()).isEmpty() && t(it.value()).size() <= 256, "EOG ID exceeds UTF-16 bound.");
            if (it.key() == "system_ids" || it.key() == "evidence_ids" || it.key() == "inspection_assets") {
                QSet<QString> seen;
                for (const auto& item : a(it.value())) {
                    const auto id = t(item);
                    require(id.size() <= 256 && !seen.contains(id), "Invalid or repeated EOG reference."); seen.insert(id);
                }
            }
            ids(it.value());
        }
    }
    if (value.isArray()) for (const auto& item : a(value)) ids(item);
}
void manifest(const QJsonObject& value) {
    require(t(value.value("dataset_identity")) == dataset_identity(o(value.value("source_hashes"))), "Dataset hash mismatch.");
    const auto first = n(value.value("source_first_unix")), cutoff = n(value.value("cutoff_unix")), now = n(value.value("source_now_unix"));
    require(first <= cutoff && cutoff <= now, "EOG cutoff is outside source window.");
    if (value.value("context") == "snapshot_now") require(cutoff == now, "Snapshot-now cutoff differs from source now.");
    const auto files = o(value.value("captured_files")), hashes = o(value.value("source_hashes"));
    for (const auto* name : {"network.json", "ops.json", "telemetry.csv"}) {
        const auto info = o(files.value(QLatin1String(name)));
        const bool csv = QString::fromLatin1(name) == "telemetry.csv";
        require(n(info.value("size_bytes")) <= static_cast<double>(csv ? breakwater::eog::maximum_csv_bytes : breakwater::eog::maximum_document_bytes),
            "Captured EOG file exceeds its bound.");
        if (!csv || cutoff == now)
            require(info.value("sha256") == hashes.value(QLatin1String(name)), "Unchanged captured input hash differs.");
    }
    require(value.value("capture_id") == capture_identity(value), "Capture hash mismatch.");
}
void cost(const QJsonObject& value) {
    const auto total = n(value.value("total")), sum = n(value.value("truck_cost")) + n(value.value("lost_revenue"));
    require(std::isfinite(sum) && std::abs(total - sum) <= std::max(1e-9, 1e-12 * std::max(std::abs(total), std::abs(sum))), "EOG cost components disagree.");
}
void plan(const QJsonObject& value, double cutoff, const QJsonObject& captured = {}) {
    require(value.value("plan_hash") == plan_identity(value), "Plan content hash differs.");
    require(n(value.value("start_unix")) == cutoff, "Plan starts at a stale cutoff.");
    if (!captured.isEmpty()) require(value.value("context") == captured.value("context"), "Plan context differs.");
    unique(a(value.value("repair_deadlines")), "facility_id");
    unique(a(value.value("truck_jobs")), "job_id");
    unique(a(value.value("well_shut_ins")), "well_id");
    for (const auto& item : a(value.value("well_shut_ins"))) {
        const auto shut = o(item);
        require(0 <= n(shut.value("start_elapsed_s")) && n(shut.value("start_elapsed_s")) <= n(shut.value("end_elapsed_s")), "Invalid well shut-in interval.");
    }
    for (const auto& item : a(value.value("truck_jobs"))) {
        const auto job = o(item); const auto trips = a(job.value("trips"));
        const auto first = n(o(trips.first()).value("arrival_elapsed_s")), last = n(o(trips.last()).value("arrival_elapsed_s"));
        require(0 <= n(job.value("called_elapsed_s")) && n(job.value("called_elapsed_s")) <= first && first <= last &&
            last <= n(job.value("available_again_elapsed_s")), "Invalid truck job interval.");
        for (qsizetype i = 1; i < trips.size(); ++i)
            require(n(o(trips[i-1]).value("arrival_elapsed_s")) < n(o(trips[i]).value("arrival_elapsed_s")), "Reversed truck trips.");
    }
}
QSet<QString> evidence(const QJsonArray& values, double cutoff, const QJsonObject& context) {
    static const QMap<QString, QString> units{{"inflow_bpd", "bpd"}, {"water_bpd", "bpd"}, {"status", "state"},
        {"level_pct", "%"}, {"flow_bpd", "bpd"}, {"discharge_psi", "psi"}, {"receive_flow_bpd", "bpd"}, {"pressure_psi", "psi"}};
    static const QMap<QString, QSet<QString>> kinds{{"facility", {"inflow_bpd"}}, {"well", {"water_bpd", "status"}},
        {"tank", {"level_pct"}}, {"pump", {"status", "flow_bpd", "discharge_psi"}}, {"pipeline", {"receive_flow_bpd", "pressure_psi"}}, {"disposal", {}}};
    unique(values, "evidence_id"); QSet<QString> result;
    const auto assets = o(context.value("assets"));
    if (context.contains("assets")) {
        require(context.value("assets").isObject(), "EOG asset context must be an object.");
        for (const auto& kind : assets) require(kind.isString() && kinds.contains(t(kind)), "Unknown EOG asset kind context.");
    }
    for (const auto& item : values) {
        const auto sample = o(item); const auto metric = t(sample.value("metric")), quality = t(sample.value("quality"));
        require(n(sample.value("timestamp_unix")) <= cutoff, "Future EOG sample.");
        require(sample.value("unit") == units.value(metric), "EOG sample unit differs from metric.");
        require((quality == "missing") == sample.value("value").isNull(), "Missing sample value mismatch.");
        if (quality == "good") require(cutoff - n(sample.value("timestamp_unix")) <= 300, "Good EOG evidence is stale.");
        if (context.contains("assets")) {
            const auto id = t(sample.value("asset_id"));
            require(assets.contains(id), "Unknown EOG evidence asset.");
            const auto kind = t(assets.value(id)); require(kinds.value(kind).contains(metric), "Unsupported EOG metric attachment.");
            if (metric == "status" && !sample.value("value").isNull())
                require(n(sample.value("value")) == 0 || n(sample.value("value")) == 1 ||
                    (kind == "pump" && n(sample.value("value")) == 2), "Unsupported EOG equipment state.");
        }
        result.insert(t(sample.value("evidence_id")));
    }
    return result;
}
void references(const QJsonArray& values, const QSet<QString>& known) {
    for (const auto& id : values) require(known.contains(t(id)), "Unknown EOG evidence reference.");
}
void events(const QJsonArray& values, const QSet<QString>& known, double cutoff) {
    unique(values, "event_id");
    for (const auto& item : values) {
        const auto event = o(item); references(a(event.value("evidence_ids")), known);
        const auto updated = n(event.value("updated_unix")); require(updated <= cutoff, "Future EOG event.");
        const auto onset = event.value("onset_unix"), end = event.value("end_unix");
        require(onset.isNull() || n(onset) <= updated, "Event onset exceeds its update.");
        if (!event.value("onset_bounds").isNull()) {
            const auto bounds = o(event.value("onset_bounds"));
            const auto earliest = n(bounds.value("earliest_unix")), latest = n(bounds.value("latest_unix"));
            require(earliest <= latest && latest <= updated && (onset.isNull() || (earliest <= n(onset) && n(onset) <= latest)),
                "Invalid EOG onset bounds.");
        }
        if (event.value("state") == "resolved") {
            require(!end.isNull() && n(end) <= updated && (onset.isNull() || n(onset) <= n(end)), "Resolved event lacks causal end.");
            require(event.value("countdown_hours").isNull(), "Resolved event has a countdown.");
        } else require(end.isNull(), "Ongoing event claims a known end.");
        const auto category = t(event.value("category"));
        if (category == "pipeline_leak" || category == "pump_degradation") {
            require(event.value("countdown_hours").isNull(), "Inspection event has a countdown.");
            require(event.value("recommended_action") == "field_inspection" || event.value("recommended_action") == "unavailable",
                "Inspection event has an incompatible action.");
        }
        if (event.value("coverage") != "available") require(event.value("countdown_hours").isNull(), "Unavailable event claims a countdown.");
    }
}
QMap<QString, double> horizons(const QJsonArray& values) {
    unique(values, "facility_id"); QMap<QString, double> result;
    for (const auto& value : values) {
        const auto item = o(value); result.insert(t(item.value("facility_id")), n(item.value("repair_elapsed_s")));
    }
    return result;
}
void assessment(const QJsonObject& value) {
    if (!value.value("cost").isNull()) cost(o(value.value("cost")));
    const bool admissible = value.value("feasible").toBool();
    if (admissible) {
        require(value.value("coverage") == "available" && !value.value("cost").isNull() &&
            value.value("outcome") != "unavailable", "Admissible actions lack complete coverage/cost/outcome.");
        require(a(value.value("violations")).isEmpty(), "Admissible actions list violations.");
    } else require(!a(value.value("violations")).isEmpty(), "Assessed invalid actions lack violations.");
    if (value.value("coverage") != "available")
        require(value.value("outcome") == "unavailable" && value.value("cost").isNull(),
            "Incomplete assessment cannot claim a complete outcome or invented cost.");
    unique(a(value.value("shutdowns")), "facility_id");
    if (value.value("outcome") == "avoids_shutdown")
        require(a(value.value("shutdowns")).isEmpty(), "Avoidance lists an automatic shutdown.");
    if (value.value("outcome") == "automatic_shutdown")
        require(!a(value.value("shutdowns")).isEmpty(), "Automatic shutdown lacks its crossing.");
}
void shutdowns(const QJsonObject& value, const QMap<QString, double>& modeled) {
    for (const auto& item : a(value.value("shutdowns"))) {
        const auto shutdown = o(item); const auto id = t(shutdown.value("facility_id"));
        require(modeled.contains(id) && n(shutdown.value("elapsed_s")) <= modeled.value(id),
            "Shutdown lies outside modeled facility/repair horizon.");
    }
}
void evaluation(const QJsonObject& value, double cutoff, const QJsonObject& captured) {
    plan(o(value.value("plan")), cutoff, captured); assessment(value);
    const auto required = horizons(a(o(value.value("plan")).value("repair_deadlines")));
    const auto trajectories = a(value.value("trajectories")), occupancy = a(value.value("fleet_occupancy"));
    unique(trajectories, "tank_id"); unique(trajectories, "facility_id"); unique(occupancy, "job_id");
    if (value.value("coverage") == "available")
        require(!required.isEmpty() && trajectories.size() == required.size(), "Incomplete modeled facility coverage.");
    for (const auto& item : trajectories) {
        const auto trajectory = o(item); const auto times = a(trajectory.value("elapsed_s"));
        const auto facility = t(trajectory.value("facility_id"));
        require(required.contains(facility), "Trajectory references an undeclared repair facility.");
        require(times.size() == a(trajectory.value("volumes_bbl")).size() && n(times.first()) == 0, "Invalid EOG trajectory shape.");
        const auto horizon = required.value(facility);
        require(n(times.last()) <= horizon && (value.value("coverage") != "available" || n(times.last()) == horizon),
            "Forecast omits its exact repair endpoint.");
        for (qsizetype i = 1; i < times.size(); ++i) {
            require(n(times[i-1]) <= n(times[i]), "Reversed EOG trajectory time.");
            if (times[i-1] == times[i]) require(i == 1 || n(times[i-2]) < n(times[i]), "More than two samples at a step.");
        }
        require(n(trajectory.value("shutdown_volume_bbl")) <= n(trajectory.value("capacity_bbl")), "Invalid shutdown inventory.");
    }
    shutdowns(value, required);
    if (value.value("feasible").toBool()) {
        QMap<QString, QMap<double, qsizetype>> points;
        for (const auto& item : trajectories) {
            const auto trajectory = o(item); auto& counts = points[t(trajectory.value("facility_id"))];
            for (const auto& time : a(trajectory.value("elapsed_s"))) ++counts[n(time)];
        }
        const auto proposed = o(value.value("plan"));
        for (const auto& item : a(proposed.value("truck_jobs"))) {
            const auto job = o(item); const auto facility = t(job.value("facility_id"));
            require(points.contains(facility), "Truck job lacks modeled facility.");
            for (const auto& trip : a(job.value("trips")))
                require(points.value(facility).value(n(o(trip).value("arrival_elapsed_s"))) == 2,
                    "Truck-arrival time lacks its exact combined before/after pair.");
        }
        for (const auto& item : a(proposed.value("well_shut_ins"))) {
            const auto action = o(item); const auto facility = t(action.value("facility_id"));
            require(points.contains(facility) && points.value(facility).contains(n(action.value("start_elapsed_s"))) &&
                points.value(facility).contains(n(action.value("end_elapsed_s"))), "Well-action breakpoint is absent.");
        }
        for (const auto& item : a(value.value("shutdowns"))) {
            const auto shutdown = o(item);
            require(points.value(t(shutdown.value("facility_id"))).contains(n(shutdown.value("elapsed_s"))),
                "Automatic shutdown crossing is absent from trajectory.");
        }
    }
    QMap<QString, QJsonObject> jobs;
    for (const auto& item : a(o(value.value("plan")).value("truck_jobs"))) jobs.insert(t(o(item).value("job_id")), o(item));
    require(occupancy.size() == jobs.size(), "Missing EOG fleet occupancy.");
    for (const auto& item : occupancy) {
        const auto occupied = o(item); const auto id = t(occupied.value("job_id"));
        require(jobs.contains(id), "Unknown EOG occupancy job."); const auto job = jobs.value(id);
        require(occupied.value("truck_id") == job.value("truck_id") && occupied.value("start_elapsed_s") == job.value("called_elapsed_s") &&
            occupied.value("end_elapsed_s") == job.value("available_again_elapsed_s"), "Fleet occupancy differs from half-open job interval.");
    }
}
} // namespace

QJsonObject decode_document(const QByteArray& bytes) {
    const auto result = telemetry::decode_document(bytes, static_cast<qsizetype>(breakwater::eog::maximum_document_bytes));
    if (result.value("kind") == "analysis_request")
        require(bytes.size() <= static_cast<qsizetype>(breakwater::eog::maximum_request_bytes), "EOG request exceeds 1 MiB.");
    return result;
}
QByteArray semantic_bytes(const QJsonValue& value) {
    inspect(value); const auto projected = projection(value);
    if (projected.isObject()) return QJsonDocument(o(projected)).toJson(QJsonDocument::Compact);
    if (projected.isArray()) return QJsonDocument(a(projected)).toJson(QJsonDocument::Compact);
    auto wrapped = QJsonDocument(QJsonArray{projected}).toJson(QJsonDocument::Compact);
    return wrapped.mid(1, wrapped.size() - 2);
}
QString dataset_identity(const QJsonObject& hashes) {
    require(hashes.size() == 3, "EOG dataset needs exactly three file hashes.");
    QByteArray bytes("breakwater-eog-inputs-v1\n");
    static const QRegularExpression pattern("^[0-9a-f]{64}$");
    for (const auto* name : {"network.json", "ops.json", "telemetry.csv"}) {
        const auto hash = t(hashes.value(QLatin1String(name)));
        require(hash.size() == 64 && pattern.match(hash).hasMatch(), "Invalid source SHA-256.");
        bytes += QByteArray(name) + ':' + hash.toLatin1() + '\n';
    }
    return digest(bytes);
}
QString capture_identity(const QJsonObject& value) {
    auto projected = value; projected.remove("capture_id");
    return digest(QByteArray("breakwater-eog-capture-v1\n") + semantic_bytes(projected));
}
QString plan_identity(const QJsonObject& value) {
    auto projected = value; projected.remove("plan_hash");
    return digest(QByteArray("breakwater-eog-plan-v1\n") + semantic_bytes(projected));
}
QJsonObject agent_plan_schema() {
    const auto plan = schemas().value("common-v1.schema.json").value("$defs").toObject().value("plan").toObject();
    auto result = inlineSchema(plan, "common-v1.schema.json").toObject();
    QJsonArray required;
    const QSet<QString> envelope{"plan_id", "plan_hash", "context", "start_unix"};
    for (const auto& field : result.value("required").toArray())
        if (!envelope.contains(field.toString())) required.append(field);
    result["required"] = required;
    auto properties = result.value("properties").toObject();
    for (const auto& field : envelope) properties.remove(field);
    result["properties"] = properties;
    return result;
}
void validate_document(const QJsonObject& document, const QString& requestedKind, const QJsonObject& context) {
    inspect(document); inspect(context);
    const auto kind = requestedKind.isEmpty() ? t(document.value("kind")) : requestedKind;
    const auto budget = document.value("kind") == "analysis_request" ? breakwater::eog::maximum_request_bytes : breakwater::eog::maximum_document_bytes;
    require(QJsonDocument(document).toJson(QJsonDocument::Compact).size() <= static_cast<qsizetype>(budget), "EOG document exceeds byte budget.");
    shape(document, kind); ids(document);
    if (kind == "capture_manifest") { manifest(document); return; }
    const auto cutoff = n(document.value("cutoff_unix"));
    const auto operation = t(document.value("operation"));
    if (kind == "analysis_request") {
        const auto captured = o(document.value("capture_manifest")); validate_document(captured, "capture_manifest");
        for (const auto* key : {"dataset_identity", "capture_id", "cutoff_unix"})
            require(document.value(QLatin1String(key)) == captured.value(QLatin1String(key)), "EOG request binding mismatch.");
        const auto path = t(document.value("capture_directory"));
        static const QRegularExpression driveRoot(QStringLiteral("^[A-Za-z]:[/\\\\]"));
        bool absolute = driveRoot.match(path).hasMatch();
#ifndef Q_OS_WIN
        absolute = absolute || QDir::isAbsolutePath(path);
#endif
        require(absolute && !path.startsWith("\\\\") && !path.startsWith("//") && !path.contains(QChar::Null),
            "EOG capture path must be absolute and local.");
        if (operation == "evaluate_plan" || operation == "validate_plan")
            plan(o(o(document.value("arguments")).value("plan")), cutoff, captured);
        return;
    }
    QJsonObject request, captured;
    if (context.contains("request")) {
        require(context.value("request").isObject(), "EOG request context must be an object.");
        request = o(context.value("request")); validate_document(request, "analysis_request");
        for (const auto* key : {"request_id", "operation", "method_version", "dataset_identity", "capture_id", "cutoff_unix"})
            require(document.value(QLatin1String(key)) == request.value(QLatin1String(key)), "EOG response binding mismatch.");
        captured = o(request.value("capture_manifest"));
    }
    if (document.value("status") != "completed") return;
    const auto result = o(document.value("result"));
    if (operation == "detect") {
        const auto known = evidence(a(result.value("evidence")), cutoff, context);
        events(a(result.value("events")), known, cutoff);
        if (result.value("coverage") == "unavailable") require(!a(result.value("limitations")).isEmpty(), "Missing coverage limitation.");
        if (result.value("coverage") == "available") require(!known.isEmpty(), "Available detection lacks evidence.");
    } else if (operation == "evaluate_plan") {
        const auto evaluated = o(result.value("evaluation")); evaluation(evaluated, cutoff, captured);
        if (!request.isEmpty()) require(evaluated.value("plan") == o(request.value("arguments")).value("plan"), "Evaluated plan was substituted.");
    } else if (operation == "search_plans") {
        const auto values = a(result.value("evaluations")); QJsonArray plans;
        for (const auto& item : values) { const auto evaluated = o(item); evaluation(evaluated, cutoff, captured); plans.append(evaluated.value("plan")); }
        unique(plans, "plan_id");
        const auto search = o(result.value("search"));
        require(n(search.value("evaluated_candidates")) <= n(search.value("candidate_limit")) &&
            values.size() <= n(search.value("evaluated_candidates")), "Search exceeds declared bound.");
        if (search.value("optimality") == "proven_within_space")
            require(search.value("complete").toBool() && search.value("scope") == "enumerated_space", "Unsupported optimality claim.");
        if (!result.value("selected_plan_id").isNull()) {
            bool selected = false; double chosen = 0, cheapest = std::numeric_limits<double>::infinity();
            for (const auto& item : values) {
                const auto evaluated = o(item); if (!evaluated.value("feasible").toBool()) continue;
                const auto total = n(o(evaluated.value("cost")).value("total")); cheapest = std::min(cheapest, total);
                if (o(evaluated.value("plan")).value("plan_id") == result.value("selected_plan_id")) { selected = true; chosen = total; }
            }
            require(selected && chosen == cheapest, "Selected plan is absent, infeasible or not cheapest retained.");
        }
        if (!request.isEmpty()) {
            const auto args = o(request.value("arguments"));
            require(search.value("candidate_limit") == args.value("candidate_limit") &&
                search.value("time_limit_s") == args.value("time_limit_s"), "Search bounds differ from request.");
        }
    } else if (operation == "validate_plan") {
        const auto validation = o(result.value("validation"));
        references(a(validation.value("evidence_ids")), evidence(a(validation.value("evidence")), cutoff, context));
        assessment(validation);
        const auto verified = horizons(a(validation.value("validated_horizons")));
        require(validation.value("coverage") != "available" || !verified.isEmpty(), "Available validation has no modeled horizons.");
        shutdowns(validation, verified);
        require(validation.value("validator_method") != validation.value("evaluator_method"), "Self-validation is not independent.");
        if (!request.isEmpty()) {
            const auto args = o(request.value("arguments")), proposed = o(args.value("plan"));
            const auto required = horizons(a(proposed.value("repair_deadlines")));
            for (auto it = verified.begin(); it != verified.end(); ++it)
                require(required.contains(it.key()) && required.value(it.key()) == it.value(), "Validation changed a repair horizon.");
            require(validation.value("coverage") != "available" || verified == required, "Validation omits a requested repair horizon.");
            require(validation.value("plan_id") == proposed.value("plan_id") && validation.value("plan_hash") == proposed.value("plan_hash") &&
                validation.value("evaluator_method") == args.value("evaluator_method"), "Validation references another plan.");
        }
    }
}
} // namespace breakwater::desktop::eog

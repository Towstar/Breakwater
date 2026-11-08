#include "TelemetryContracts.hpp"
#include "ProjectDocument.hpp"

#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSet>
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace breakwater::desktop::telemetry {
namespace {
namespace dto = breakwater::telemetry;
void require(bool value, const char* message = "Invalid telemetry contract.") {
    if (!value) throw std::invalid_argument(message);
}
void keys(const QJsonObject& object, const QSet<QString>& required, const QSet<QString>& optional = {}) {
    for (const auto& name : required) require(object.contains(name), "Missing telemetry field.");
    for (auto it = object.begin(); it != object.end(); ++it)
        require(required.contains(it.key()) || optional.contains(it.key()), "Unknown telemetry field.");
}
QJsonObject object(const QJsonValue& value) { require(value.isObject()); return value.toObject(); }
QJsonArray array(const QJsonValue& value, qsizetype low = 0, qsizetype high = 256) {
    require(value.isArray()); const auto result = value.toArray();
    require(result.size() >= low && result.size() <= high, "Telemetry count limit exceeded."); return result;
}
double number(const QJsonValue& value, double minimum = -std::numeric_limits<double>::max()) {
    require(value.isDouble() && std::isfinite(value.toDouble()) && value.toDouble() >= minimum,
        "Telemetry values must be finite and within the declared domain.");
    return value.toDouble();
}
double positive(const QJsonValue& value) { const auto result = number(value); require(result > 0.0); return result; }
std::uint64_t integer(const QJsonValue& value, std::uint64_t minimum = 0, std::uint64_t maximum = dto::maximum_sequence) {
    const auto result = number(value);
    require(std::floor(result) == result && result >= static_cast<double>(minimum) && result <= static_cast<double>(maximum));
    return static_cast<std::uint64_t>(result);
}
bool boolean(const QJsonValue& value) { require(value.isBool()); return value.toBool(); }
QString text(const QJsonValue& value, qsizetype maximum = 256, bool empty = false) {
    require(value.isString()); const auto result = value.toString();
    require((empty || !result.isEmpty()) && result.size() <= 2 * maximum && result.toUcs4().size() <= maximum &&
        QString::fromUtf8(result.toUtf8()) == result, "Invalid telemetry text.");
    return result;
}
QString identifier(const QJsonValue& value) {
    static const QRegularExpression pattern(QStringLiteral("^[A-Za-z0-9][A-Za-z0-9_.:-]{0,127}$"));
    const auto result = text(value, 128); const auto match = pattern.match(result);
    require(match.hasMatch() && match.capturedLength() == result.size(), "Invalid telemetry identifier."); return result;
}
QString reference(const QJsonValue& value) {
    const auto result = text(value); require(result.size() <= 256, "Legacy references exceed 256 UTF-16 units."); return result;
}
QString hash(const QJsonValue& value) {
    static const QRegularExpression pattern(QStringLiteral("^[0-9a-f]{64}$"));
    const auto result = text(value, 64); require(pattern.match(result).hasMatch(), "Invalid telemetry hash."); return result;
}
QString enumeration(const QJsonValue& value, const QSet<QString>& allowed) {
    const auto result = text(value, 128); require(allowed.contains(result), "Unsupported telemetry enum value."); return result;
}
QString utc(const QJsonValue& value) {
    const auto result = text(value, 64);
    static const QRegularExpression pattern(QStringLiteral("^\\d{4}-\\d{2}-\\d{2}T\\d{2}:\\d{2}:\\d{2}(?:\\.\\d{1,6})?Z$"));
    const auto match = pattern.match(result);
    require(match.hasMatch() && match.capturedLength() == result.size() && result.mid(11, 2).toInt() <= 23 &&
        result.mid(14, 2).toInt() <= 59 && result.mid(17, 2).toInt() <= 59 && QDateTime::fromString(result, Qt::ISODateWithMs).isValid(),
        "Receive time must be an explicit UTC timestamp.");
    return result;
}
std::string s(const QString& value) { return value.toStdString(); }
void common(const QJsonObject& value, const char* kind) {
    require(integer(value.value("telemetry_version"), 1, 1) == 1 && value.value("kind") == QString::fromLatin1(kind), "Unsupported telemetry kind/version.");
}
QSet<QString> qualities() { return {"good", "suspect", "bad", "unknown"}; }
QSet<QString> operations() { return {"pause", "resume", "set_rate", "restart", "stop"}; }
QSet<QString> pumpStates() { return {"commanded_off", "running", "delivery_loss", "unavailable", "unknown"}; }
dto::Value value(const QJsonValue& item, bool pump) {
    if (pump) return s(enumeration(item, pumpStates()));
    return number(item);
}
std::vector<std::string> identifiers(const QJsonValue& values, bool legacy = false) {
    QSet<QString> seen; std::vector<std::string> result;
    for (const auto& item : array(values)) {
        const auto id = legacy ? reference(item) : identifier(item); require(!seen.contains(id), "Duplicate telemetry identity.");
        seen.insert(id); result.push_back(s(id));
    }
    return result;
}
void inspect(const QJsonValue& value, int depth = 0) {
    require(depth <= 64, "Telemetry nesting limit exceeded.");
    if (value.isDouble()) (void)number(value);
    if (value.isString()) {
        const auto t = value.toString(); require(QString::fromUtf8(t.toUtf8()) == t, "Invalid Unicode scalar in telemetry.");
    }
    if (value.isObject()) {
        const auto input = value.toObject();
        for (auto it = input.begin(); it != input.end(); ++it) { inspect(it.key(), depth + 1); inspect(it.value(), depth + 1); }
    }
    if (value.isArray()) for (const auto& child : value.toArray()) inspect(child, depth + 1);
}

// QJsonDocument normalizes duplicate keys. Inspect the original, already syntax-
// checked UTF-8 so escaped spellings of the same key cannot evade rejection.
class DuplicateKeyCheck {
public:
    explicit DuplicateKeyCheck(const QByteArray& bytes) : bytes_(bytes) {}
    void run() { parseValue(0); whitespace(); require(at_ == bytes_.size()); }
private:
    const QByteArray& bytes_; qsizetype at_ = 0;
    void whitespace() { while (at_ < bytes_.size() && (bytes_[at_] == ' ' || bytes_[at_] == '\t' || bytes_[at_] == '\r' || bytes_[at_] == '\n')) ++at_; }
    QByteArray stringToken() {
        require(at_ < bytes_.size() && bytes_[at_] == '"'); const auto start = at_++;
        while (at_ < bytes_.size()) {
            const auto ch = bytes_[at_++];
            if (ch == '\\') { require(at_ < bytes_.size()); ++at_; }
            else if (ch == '"') return bytes_.mid(start, at_ - start);
        }
        throw std::invalid_argument("Unterminated telemetry string.");
    }
    void parseValue(int depth) {
        require(depth <= 64, "Telemetry nesting limit exceeded."); whitespace(); require(at_ < bytes_.size());
        if (bytes_[at_] == '{') {
            ++at_; whitespace(); QSet<QString> keysSeen;
            if (bytes_[at_] == '}') { ++at_; return; }
            for (;;) {
                whitespace(); const auto token = stringToken();
                // The whole document has already passed syntax and UTF-8 checks.
                // Ordinary protocol keys need no second JSON parse. Escaped
                // spellings still use Qt's decoder so aliases remain identical.
                const auto content = token.mid(1, token.size() - 2);
                const auto key = content.contains('\\')
                    ? QJsonDocument::fromJson(QByteArray("[") + token + ']').array().at(0).toString()
                    : QString::fromUtf8(content);
                require(!keysSeen.contains(key), "Duplicate telemetry object key."); keysSeen.insert(key);
                whitespace(); require(bytes_[at_++] == ':'); parseValue(depth + 1); whitespace();
                const auto delimiter = bytes_[at_++]; if (delimiter == '}') return; require(delimiter == ',');
            }
        }
        if (bytes_[at_] == '[') {
            ++at_; whitespace(); if (bytes_[at_] == ']') { ++at_; return; }
            for (;;) { parseValue(depth + 1); whitespace(); const auto delimiter = bytes_[at_++]; if (delimiter == ']') return; require(delimiter == ','); }
        }
        if (bytes_[at_] == '"') { (void)stringToken(); return; }
        while (at_ < bytes_.size() && bytes_[at_] != ',' && bytes_[at_] != '}' && bytes_[at_] != ']' && bytes_[at_] != ' ' && bytes_[at_] != '\r' && bytes_[at_] != '\n' && bytes_[at_] != '\t') ++at_;
    }
};

void checkContext(const QJsonObject& context) {
    inspect(context);
    for (const auto* key : {"network", "project", "configuration", "pipe_parameters", "pipe_parameter_hashes", "command", "previous_frame", "clock"})
        if (context.contains(QLatin1String(key))) require(context.value(QLatin1String(key)).isObject(), "Telemetry context entry must be an object.");
    if (context.contains("physical_hash")) (void)hash(context.value("physical_hash"));
    if (context.contains("network")) (void)parse_project({{"version", 1}, {"network", context.value("network")}, {"layout", QJsonObject{}}});
    if (context.contains("project")) (void)parse_project(context.value("project").toObject());
    if (context.contains("network") && context.contains("project"))
        require(context.value("network") == context.value("project").toObject().value("network"), "Telemetry context contains contradictory networks.");
    if (context.contains("pipe_parameters") && context.contains("project")) {
        const auto transport = context.value("project").toObject().value("pipe_transport").toObject();
        if (transport.contains("routes")) require(context.value("pipe_parameters") == transport.value("routes"), "Telemetry context contains contradictory pipe parameters.");
    }
    if (context.contains("clock")) {
        const auto clock = context.value("clock").toObject();
        keys(clock, {"stream_id", "epoch_id", "now_event_time_s", "watermark_event_time_s", "mode"});
        (void)identifier(clock.value("stream_id")); (void)identifier(clock.value("epoch_id"));
        (void)number(clock.value("now_event_time_s"), 0); (void)number(clock.value("watermark_event_time_s"), 0);
        (void)enumeration(clock.value("mode"), {"synthetic_live", "replay", "historical"});
    }
}
QJsonObject contextConfiguration(const QJsonObject& context) { return context.value("configuration").toObject(); }
QJsonObject contextNetwork(const QJsonObject& context) {
    if (context.contains("network")) return context.value("network").toObject();
    return context.value("project").toObject().value("network").toObject();
}
QJsonObject findSensor(const QJsonObject& configuration, const QString& sensorId) {
    for (const auto& item : configuration.value("sensors").toArray())
        if (item.toObject().value("sensor_id") == sensorId) return item.toObject();
    throw std::invalid_argument("Unknown telemetry sensor reference.");
}
bool pumpSensor(const QJsonObject& sensor) { return sensor.value("quantity") == "pump_state"; }
double unitFactor(const QString& quantity, const QString& unit, const QString& canonical) {
    if (unit == canonical) return 1.0;
    if (quantity == "inventory" && unit == "L") return 0.001;
    if (quantity == "level" && unit == "cm") return 0.01;
    if ((quantity == "source_flow" || quantity == "route_flow") && unit == "L/s") return 3.6;
    if ((quantity == "source_flow" || quantity == "route_flow") && unit == "L/h") return 0.001;
    throw std::invalid_argument("Unsupported telemetry unit conversion.");
}
bool mappingSourceAvailable(const QJsonObject& sensor, const QJsonObject& sample) {
    const auto quantity = sensor.value("quantity").toString();
    if (!sensor.value("enabled").toBool() || quantity == "pressure" || quantity == "temperature" || quantity == "custom_numeric" ||
        sensor.value("physical_support").toObject().value("kind") == "point_unavailable" ||
        sample.value("receiver_status") != "good" || sample.value("reported_quality") != "good") return false;
    if (quantity == "level") {
        const auto knots = sensor.value("calibration").toObject().value("knots").toArray();
        const auto level = sample.value("canonical_value").toDouble();
        return !knots.isEmpty() && level >= knots.first().toObject().value("level_m").toDouble() && level <= knots.last().toObject().value("level_m").toDouble();
    }
    return true;
}
void bind(const QJsonObject& document, const QJsonObject& context) {
    const auto configuration = contextConfiguration(context);
    if (context.contains("configuration")) {
        (void)parse_sensor_configuration(configuration, context);
        require(document.value("physical_hash") == configuration.value("physical_hash"), "Telemetry physical identity differs from configuration.");
        require(document.value("configuration_hash") == sensor_semantic_hash(configuration), "Telemetry configuration identity differs.");
    }
    if (context.contains("physical_hash")) require(document.value("physical_hash") == context.value("physical_hash"));
}
} // namespace

QJsonObject decode_document(const QByteArray& bytes, qsizetype maximum) {
    require(!bytes.isEmpty() && bytes.size() <= maximum && !bytes.startsWith("\xef\xbb\xbf"), "Telemetry document byte limit or encoding is invalid.");
    require(QString::fromUtf8(bytes).toUtf8() == bytes, "Telemetry must be valid UTF-8.");
    QJsonParseError error; const auto document = QJsonDocument::fromJson(bytes, &error);
    require(error.error == QJsonParseError::NoError && document.isObject(), "Telemetry must be a JSON object.");
    const auto kind = document.object().value("kind").toString();
    if (document.object().contains("telemetry_version") && (kind == "stream_frame" || kind == "stream_command"))
        require(bytes.size() <= static_cast<qsizetype>(dto::maximum_frame_bytes), "Stream frame exceeds its original UTF-8 byte limit.");
    DuplicateKeyCheck(bytes).run(); inspect(document.object()); return document.object();
}

breakwater::telemetry::SensorConfiguration parse_sensor_configuration(const QJsonObject& document, const QJsonObject& context) {
    checkContext(context);
    keys(document, {"telemetry_version", "kind", "configuration_id", "revision", "physical_hash", "policy", "sensors"});
    common(document, "sensor_configuration"); inspect(document);
    dto::SensorConfiguration result;
    result.configuration_id = s(identifier(document.value("configuration_id")));
    result.revision = integer(document.value("revision"), 1);
    result.physical_hash = s(hash(document.value("physical_hash")));
    if (context.contains("physical_hash")) require(document.value("physical_hash") == context.value("physical_hash"));
    const auto policy = object(document.value("policy"));
    keys(policy, {"max_skew_s", "reorder_window_s", "future_allowance_s", "heartbeat_period_s", "source_timeout_s"});
    result.policy = {number(policy.value("max_skew_s"), 0), number(policy.value("reorder_window_s"), 0),
        number(policy.value("future_allowance_s"), 0), positive(policy.value("heartbeat_period_s")), positive(policy.value("source_timeout_s"))};
    require(result.policy.source_timeout_s >= result.policy.heartbeat_period_s, "Source timeout precedes heartbeat period.");
    const auto networkObject = contextNetwork(context);
    std::optional<Network> network;
    if (!networkObject.isEmpty()) network = parse_project({{"version", 1}, {"network", networkObject}, {"layout", QJsonObject{}}}).network;
    auto pipeParameters = context.value("pipe_parameters").toObject();
    if (!context.contains("pipe_parameters") && context.value("project").isObject())
        pipeParameters = context.value("project").toObject().value("pipe_transport").toObject().value("routes").toObject();
    const QMap<QString, QString> canonicalUnits{{"inventory", "m3"}, {"level", "m"}, {"source_flow", "m3/h"},
        {"route_flow", "m3/h"}, {"pump_state", "state"}, {"concentration", "mg/L"}, {"pressure", "kPa"}, {"temperature", "degC"}};
    QSet<QString> sensorIds, channels;
    QMap<QString, QJsonObject> groups;
    QMap<QString, int> primaries;
    for (const auto& entry : array(document.value("sensors"))) {
        const auto item = object(entry);
        keys(item, {"sensor_id", "label", "quantity", "unit", "source_id", "channel_id", "attachment", "enabled",
            "expected_period_s", "max_age_s", "uncertainty_bound", "group_id", "role", "physical_support", "calibration"},
            {"instrument_min", "instrument_max", "pipe_parameter_hash"});
        dto::Sensor sensor;
        const auto sensorId = identifier(item.value("sensor_id")); require(!sensorIds.contains(sensorId), "Duplicate sensor ID."); sensorIds.insert(sensorId);
        sensor.sensor_id = s(sensorId); sensor.label = s(text(item.value("label"), 256, true));
        const auto quantity = enumeration(item.value("quantity"), {"inventory", "level", "source_flow", "route_flow", "pump_state", "concentration", "pressure", "temperature", "custom_numeric"});
        sensor.quantity = s(quantity); const auto unit = text(item.value("unit"), 32); sensor.unit = s(unit);
        if (quantity != "custom_numeric") require(unit == canonicalUnits.value(quantity), "Sensor has an incompatible canonical unit.");
        const auto source = identifier(item.value("source_id")), channel = identifier(item.value("channel_id"));
        const auto channelKey = source + '\n' + channel; require(!channels.contains(channelKey), "Duplicate source/channel mapping."); channels.insert(channelKey);
        sensor.source_id = s(source); sensor.channel_id = s(channel);
        const auto attachment = object(item.value("attachment")); keys(attachment, {"kind", "id"});
        const auto attachmentKind = enumeration(attachment.value("kind"), {"asset", "route"}), attachmentId = reference(attachment.value("id"));
        sensor.attachment = {s(attachmentKind), s(attachmentId)};
        const bool needsAsset = quantity == "inventory" || quantity == "level" || quantity == "source_flow";
        const bool needsRoute = quantity == "route_flow" || quantity == "pump_state" || quantity == "concentration";
        require((!needsAsset || attachmentKind == "asset") && (!needsRoute || attachmentKind == "route"), "Sensor attachment kind is incompatible.");
        if (network) {
            if (attachmentKind == "asset") {
                const auto found = std::find_if(network->assets.begin(), network->assets.end(), [&](const Asset& asset) { return asset.id == sensor.attachment.id; });
                require(found != network->assets.end(), "Sensor references an unknown asset.");
                if (quantity == "inventory" || quantity == "level") require(found->kind == AssetKind::tank, "Inventory/level requires a tank.");
                if (quantity == "source_flow") require(found->kind == AssetKind::source, "Source flow requires a production source.");
            } else {
                const auto found = std::find_if(network->routes.begin(), network->routes.end(), [&](const Route& route) { return route.id == sensor.attachment.id; });
                require(found != network->routes.end(), "Sensor references an unknown route.");
                if (quantity == "pump_state") require(found->has_pump, "Pump feedback requires a pump route.");
            }
        }
        sensor.enabled = boolean(item.value("enabled")); sensor.expected_period_s = positive(item.value("expected_period_s"));
        sensor.max_age_s = positive(item.value("max_age_s")); require(sensor.max_age_s >= sensor.expected_period_s, "Maximum age precedes expected sampling period.");
        sensor.uncertainty_bound = number(item.value("uncertainty_bound"), 0);
        const auto groupId = identifier(item.value("group_id")); sensor.group_id = s(groupId);
        sensor.role = s(enumeration(item.value("role"), {"primary", "redundant"}));
        if (sensor.role == "primary") ++primaries[groupId];
        const QJsonObject groupShape{{"quantity", quantity}, {"unit", unit}, {"attachment", attachment}};
        require(!groups.contains(groupId) || groups.value(groupId) == groupShape, "Redundant sensor group mixes quantities or attachments."); groups[groupId] = groupShape;
        const auto support = object(item.value("physical_support"));
        const auto supportKind = enumeration(support.value("kind"), {"none", "cell_average", "interval_average", "point_unavailable"});
        QSet<QString> supportFields{"kind", "acquisition_window_s", "delay_s"};
        if (supportKind == "cell_average") supportFields.insert("cell_index");
        if (supportKind == "interval_average" || supportKind == "point_unavailable") { supportFields.insert("start_m"); supportFields.insert("end_m"); }
        keys(support, supportFields);
        sensor.physical_support.kind = s(supportKind);
        sensor.physical_support.acquisition_window_s = number(support.value("acquisition_window_s"), 0);
        sensor.physical_support.delay_s = number(support.value("delay_s"), 0);
        require((quantity == "concentration") == (supportKind != "none"), "Concentration needs explicit spatial support; other channels use none.");
        if (quantity == "concentration") sensor.pipe_parameter_hash = s(hash(item.value("pipe_parameter_hash")));
        else require(!item.contains("pipe_parameter_hash"), "Only concentration sensors bind pipe parameters.");
        if (supportKind == "cell_average") sensor.physical_support.cell_index = integer(support.value("cell_index"), 0, 4095);
        if (supportKind == "interval_average" || supportKind == "point_unavailable") {
            sensor.physical_support.start_m = number(support.value("start_m"), 0);
            sensor.physical_support.end_m = number(support.value("end_m"), 0);
            require(supportKind == "point_unavailable" ? *sensor.physical_support.start_m == *sensor.physical_support.end_m : *sensor.physical_support.start_m < *sensor.physical_support.end_m,
                "Invalid concentration spatial support.");
        }
        if (quantity == "concentration" && network) {
            require(pipeParameters.value(attachmentId).isObject(), "Concentration requires declared pipe geometry.");
            require(item.value("pipe_parameter_hash") == context.value("pipe_parameter_hashes").toObject().value(attachmentId), "Concentration pipe parameter identity differs from registration.");
            const auto pipe = pipeParameters.value(attachmentId).toObject(); const auto length = positive(pipe.value("length_m"));
            const auto cells = integer(pipe.value("cells"), 2, 4096);
            if (sensor.physical_support.cell_index) require(*sensor.physical_support.cell_index < cells, "Concentration cell lies outside the pipe.");
            if (sensor.physical_support.end_m) require(*sensor.physical_support.end_m <= length, "Concentration support lies outside the pipe.");
        }
        const auto calibration = object(item.value("calibration"));
        const auto calibrationKind = enumeration(calibration.value("kind"), {"identity", "level_volume_piecewise_linear"});
        keys(calibration, {"calibration_id", "version", "kind", "error_bound"}, calibrationKind == "level_volume_piecewise_linear" ? QSet<QString>{"knots"} : QSet<QString>{});
        sensor.calibration.calibration_id = s(identifier(calibration.value("calibration_id")));
        sensor.calibration.version = integer(calibration.value("version"), 1); sensor.calibration.kind = s(calibrationKind);
        sensor.calibration.error_bound = number(calibration.value("error_bound"), 0);
        require((quantity == "level") == (calibrationKind == "level_volume_piecewise_linear"), "Level requires explicit level-volume calibration.");
        if (calibrationKind == "level_volume_piecewise_linear") {
            double previousLevel = -std::numeric_limits<double>::infinity(), previousVolume = previousLevel;
            for (const auto& knotValue : array(calibration.value("knots"), 2)) {
                const auto knot = object(knotValue); keys(knot, {"level_m", "volume_m3"});
                const auto level = number(knot.value("level_m")), volume = number(knot.value("volume_m3"));
                require(level > previousLevel && volume > previousVolume, "Level calibration must strictly increase.");
                sensor.calibration.knots.push_back({level, volume}); previousLevel = level; previousVolume = volume;
            }
        }
        if (item.contains("instrument_min")) sensor.instrument_min = number(item.value("instrument_min"));
        if (item.contains("instrument_max")) sensor.instrument_max = number(item.value("instrument_max"));
        require(!sensor.instrument_min || !sensor.instrument_max || *sensor.instrument_min < *sensor.instrument_max, "Instrument range is reversed or empty.");
        result.sensors.push_back(std::move(sensor));
    }
    for (auto it = groups.begin(); it != groups.end(); ++it) require(primaries.value(it.key()) == 1, "Each sensor group needs exactly one declared primary.");
    return result;
}

namespace {
QJsonValue hashProjection(const QJsonValue& value) {
    if (value.isBool() || value.isNull() || value.isString()) return value;
    if (value.isDouble()) {
        const auto n = number(value); const auto bits = std::bit_cast<std::uint64_t>(n == 0.0 ? 0.0 : n);
        const auto hexadecimal = QByteArray::number(static_cast<qulonglong>(bits), 16).rightJustified(16, '0');
        return QJsonObject{{"$f64", QString::fromLatin1(hexadecimal)}};
    }
    if (value.isArray()) { QJsonArray result; for (const auto& child : value.toArray()) result.append(hashProjection(child)); return result; }
    QJsonObject result; const auto input = value.toObject();
    for (auto it = input.begin(); it != input.end(); ++it) result.insert(it.key(), hashProjection(it.value()));
    return result;
}
}

QByteArray sensor_semantic_bytes(const QJsonObject& configuration) {
    (void)parse_sensor_configuration(configuration);
    auto sensors = configuration.value("sensors").toArray(); std::vector<QJsonObject> sorted;
    for (const auto& value : sensors) { auto sensor = value.toObject(); sensor.remove("label"); sorted.push_back(sensor); }
    std::sort(sorted.begin(), sorted.end(), [](const QJsonObject& a, const QJsonObject& b) { return a.value("sensor_id").toString() < b.value("sensor_id").toString(); });
    sensors = {}; for (const auto& sensor : sorted) sensors.append(sensor);
    const QJsonObject projection{{"telemetry_version", 1}, {"physical_hash", configuration.value("physical_hash")},
        {"policy", configuration.value("policy")}, {"sensors", sensors}};
    return QJsonDocument(hashProjection(projection).toObject()).toJson(QJsonDocument::Compact);
}
QString sensor_semantic_hash(const QJsonObject& configuration) {
    return QString::fromLatin1(QCryptographicHash::hash(sensor_semantic_bytes(configuration), QCryptographicHash::Sha256).toHex());
}

breakwater::telemetry::StreamCommand parse_stream_command(const QJsonObject& document) {
    keys(document, {"telemetry_version", "kind", "request_id", "stream_id", "epoch_id", "operation", "payload"});
    common(document, "stream_command"); inspect(document);
    dto::StreamCommand result;
    result.request_id = s(identifier(document.value("request_id"))); result.stream_id = s(identifier(document.value("stream_id")));
    result.epoch_id = s(identifier(document.value("epoch_id"))); result.operation = s(enumeration(document.value("operation"), operations()));
    const auto payload = object(document.value("payload"));
    if (result.operation == "set_rate") { keys(payload, {"clock_rate"}); result.clock_rate = positive(payload.value("clock_rate")); }
    else keys(payload, {});
    require(QJsonDocument(document).toJson(QJsonDocument::Compact).size() <= static_cast<qsizetype>(dto::maximum_frame_bytes));
    return result;
}

breakwater::telemetry::StreamFrame parse_stream_frame(const QJsonObject& document, const QJsonObject& context) {
    checkContext(context);
    keys(document, {"telemetry_version", "kind", "type", "stream_id", "epoch_id", "sequence", "configuration_hash", "physical_hash", "event_time_s", "payload"});
    common(document, "stream_frame"); inspect(document); bind(document, context);
    require(QJsonDocument(document).toJson(QJsonDocument::Compact).size() <= static_cast<qsizetype>(dto::maximum_frame_bytes));
    dto::StreamFrame result;
    result.type = s(enumeration(document.value("type"), {"start", "readings", "heartbeat", "ack", "error", "end"}));
    result.stream_id = s(identifier(document.value("stream_id"))); result.epoch_id = s(identifier(document.value("epoch_id")));
    result.sequence = integer(document.value("sequence")); result.configuration_hash = s(hash(document.value("configuration_hash")));
    result.physical_hash = s(hash(document.value("physical_hash"))); result.event_time_s = number(document.value("event_time_s"), 0);
    require(result.type == "start" ? result.sequence == 0 : result.sequence > 0, "Invalid telemetry start/subsequent sequence.");
    const auto payload = object(document.value("payload")); const auto configuration = contextConfiguration(context);
    if (context.contains("configuration")) (void)parse_sensor_configuration(configuration, context);
    if (result.type == "start" || result.type == "heartbeat") {
        keys(payload, result.type == "start" ? QSet<QString>{"mode", "clock_rate", "paused"} : QSet<QString>{"clock_rate", "paused"});
        if (result.type == "start") result.mode = s(enumeration(payload.value("mode"), {"synthetic_live", "replay"}));
        result.clock = {boolean(payload.value("paused")), positive(payload.value("clock_rate"))};
    } else if (result.type == "readings") {
        keys(payload, {"readings"}); QSet<QString> seen;
        for (const auto& item : array(payload.value("readings"), 1)) {
            const auto reading = object(item); keys(reading, {"sensor_id", "event_time_s", "acquisition_start_s", "acquisition_end_s", "value", "unit", "quality"});
            dto::Reading decoded; const auto id = identifier(reading.value("sensor_id"));
            require(!seen.contains(id), "A reading batch repeats a sensor."); seen.insert(id); decoded.sensor_id = s(id);
            decoded.event_time_s = number(reading.value("event_time_s"), 0); decoded.acquisition_start_s = number(reading.value("acquisition_start_s"), 0);
            decoded.acquisition_end_s = number(reading.value("acquisition_end_s"), 0);
            require(decoded.acquisition_start_s <= decoded.acquisition_end_s && decoded.acquisition_end_s <= decoded.event_time_s && decoded.event_time_s <= result.event_time_s,
                "Acquisition/event/frame times are inconsistent.");
            const auto unit = text(reading.value("unit"), 32); decoded.unit = s(unit); decoded.quality = s(enumeration(reading.value("quality"), qualities()));
            if (context.contains("configuration")) {
                const auto sensor = findSensor(configuration, id); require(sensor.value("enabled").toBool(), "Reading references a disabled sensor.");
                decoded.value = value(reading.value("value"), pumpSensor(sensor));
                const auto factor = unitFactor(sensor.value("quantity").toString(), unit, sensor.value("unit").toString());
                if (!pumpSensor(sensor)) require(std::isfinite(std::get<double>(decoded.value) * factor), "Unit conversion overflowed.");
            } else decoded.value = value(reading.value("value"), reading.value("value").isString());
            result.readings.push_back(std::move(decoded));
        }
    } else if (result.type == "ack") {
        keys(payload, {"request_id", "operation", "effective_event_time_s", "paused", "clock_rate"});
        dto::Acknowledgement ack;
        ack.request_id = s(identifier(payload.value("request_id"))); ack.operation = s(enumeration(payload.value("operation"), operations()));
        ack.effective_event_time_s = number(payload.value("effective_event_time_s"), 0); require(ack.effective_event_time_s <= result.event_time_s);
        ack.clock = {boolean(payload.value("paused")), positive(payload.value("clock_rate"))};
        if (context.value("command").isObject()) {
            const auto command = parse_stream_command(context.value("command").toObject());
            require(command.request_id == ack.request_id && command.operation == ack.operation && command.stream_id == result.stream_id && command.epoch_id == result.epoch_id,
                "Acknowledgement does not match its command.");
        }
        result.acknowledgement = ack;
    } else if (result.type == "error") {
        keys(payload, {"code", "message"}); result.error = dto::StreamError{s(identifier(payload.value("code"))), s(text(payload.value("message"), 2000))};
    } else { keys(payload, {"reason"}); result.end_reason = s(text(payload.value("reason"), 2000)); }
    return result;
}

QString frame_disposition(const QJsonObject& document, const QJsonObject& context) {
    const auto frame = parse_stream_frame(document, context); const auto clock = context.value("clock").toObject();
    if (clock.isEmpty()) return "accepted";
    if (QString::fromStdString(frame.stream_id) != clock.value("stream_id") || QString::fromStdString(frame.epoch_id) != clock.value("epoch_id")) return "old_epoch";
    const auto previous = context.value("previous_frame").toObject();
    if (!previous.isEmpty() && document.value("stream_id") == previous.value("stream_id") &&
        document.value("epoch_id") == previous.value("epoch_id") && document.value("sequence") == previous.value("sequence"))
        return document == previous ? "duplicate" : "conflict";
    const auto configuration = contextConfiguration(context);
    const auto policy = context.contains("configuration") ? parse_sensor_configuration(configuration).policy : dto::Policy{};
    const auto now = number(clock.value("now_event_time_s"), 0), watermark = number(clock.value("watermark_event_time_s"), 0);
    if (frame.event_time_s > now + policy.future_allowance_s) return "future";
    if (frame.event_time_s < watermark - policy.reorder_window_s) return "history_only";
    if (frame.event_time_s < watermark) return "buffered";
    if (clock.contains("mode") && clock.value("mode") != "synthetic_live") return "history_only";
    return "accepted";
}

breakwater::telemetry::ObservedSnapshot parse_observed_snapshot(const QJsonObject& document, const QJsonObject& context) {
    checkContext(context);
    require(context.contains("configuration") && context.contains("network") && context.contains("physical_hash"), "Observed snapshots require a registered configuration/network/hash context.");
    keys(document, {"telemetry_version", "kind", "snapshot_id", "physical_hash", "numerical_revision", "configuration_hash", "stream_id", "epoch_id", "mode",
        "captured_event_time_s", "expires_event_time_s", "samples", "commands", "derived_state", "coverage", "assumptions", "availability", "actionable"});
    common(document, "observed_snapshot"); inspect(document); bind(document, context);
    dto::ObservedSnapshot result;
    result.snapshot_id = s(identifier(document.value("snapshot_id"))); result.physical_hash = s(hash(document.value("physical_hash")));
    result.numerical_revision = integer(document.value("numerical_revision")); result.configuration_hash = s(hash(document.value("configuration_hash")));
    result.stream_id = s(identifier(document.value("stream_id"))); result.epoch_id = s(identifier(document.value("epoch_id")));
    result.mode = s(enumeration(document.value("mode"), {"synthetic_live", "replay", "historical"}));
    result.captured_event_time_s = number(document.value("captured_event_time_s"), 0);
    result.expires_event_time_s = number(document.value("expires_event_time_s"), 0);
    require(result.captured_event_time_s <= result.expires_event_time_s, "Snapshot expiry precedes capture.");
    require(!boolean(document.value("actionable")), "Interchange snapshots cannot grant control authority.");
    result.availability = s(enumeration(document.value("availability"), {"available", "degraded", "unavailable"}));
    const auto configuration = contextConfiguration(context);
    if (context.contains("configuration")) (void)parse_sensor_configuration(configuration, context);
    QMap<QString, QJsonObject> samples; QSet<QString> commandIds;
    for (const auto& item : array(document.value("samples"))) {
        const auto sample = object(item); keys(sample, {"sensor_id", "frame_sequence", "event_time_s", "acquisition_start_s", "acquisition_end_s", "received_utc", "received_monotonic_s", "original_value", "original_unit",
            "canonical_value", "canonical_unit", "reported_quality", "receiver_status"});
        dto::Sample decoded; const auto id = identifier(sample.value("sensor_id"));
        require(!samples.contains(id), "Snapshot repeats a sensor sample."); samples[id] = sample; decoded.sensor_id = s(id);
        decoded.frame_sequence = integer(sample.value("frame_sequence")); decoded.event_time_s = number(sample.value("event_time_s"), 0);
        decoded.acquisition_start_s = number(sample.value("acquisition_start_s"), 0); decoded.acquisition_end_s = number(sample.value("acquisition_end_s"), 0);
        require(decoded.acquisition_start_s <= decoded.acquisition_end_s && decoded.acquisition_end_s <= decoded.event_time_s && decoded.event_time_s <= result.captured_event_time_s,
            "Snapshot acquisition/event/capture times are inconsistent.");
        decoded.received_utc = s(utc(sample.value("received_utc"))); decoded.received_monotonic_s = number(sample.value("received_monotonic_s"), 0);
        decoded.original_unit = s(text(sample.value("original_unit"), 32)); decoded.canonical_unit = s(text(sample.value("canonical_unit"), 32));
        decoded.reported_quality = s(enumeration(sample.value("reported_quality"), qualities()));
        decoded.receiver_status = s(enumeration(sample.value("receiver_status"), {"good", "stale", "missing", "rejected", "inconsistent"}));
        if (context.contains("configuration")) {
            const auto sensor = findSensor(configuration, id); const bool pump = pumpSensor(sensor);
            decoded.original_value = value(sample.value("original_value"), pump); decoded.canonical_value = value(sample.value("canonical_value"), pump);
            require(sample.value("canonical_unit") == sensor.value("unit"), "Sample canonical unit differs from configuration.");
            const auto factor = unitFactor(sensor.value("quantity").toString(), sample.value("original_unit").toString(), sensor.value("unit").toString());
            if (pump) require(decoded.original_value == decoded.canonical_value, "Pump feedback changed during conversion.");
            else {
                const auto canonical = std::get<double>(decoded.original_value) * factor, actual = std::get<double>(decoded.canonical_value);
                require(std::isfinite(canonical) && std::abs(actual - canonical) <= std::max(1e-12, 1e-12 * std::max(std::abs(actual), std::abs(canonical))),
                    "Original and canonical sample values disagree.");
            }
            if (decoded.receiver_status == "good") require(result.captured_event_time_s - decoded.event_time_s <= sensor.value("max_age_s").toDouble(), "Stale sample is labeled good.");
        } else {
            decoded.original_value = value(sample.value("original_value"), sample.value("original_value").isString());
            decoded.canonical_value = value(sample.value("canonical_value"), sample.value("canonical_value").isString());
        }
        result.samples.push_back(std::move(decoded));
    }
    for (const auto& item : array(document.value("commands"), 0, 512)) {
        const auto command = object(item); keys(command, {"control_id", "value", "unit"});
        const auto id = text(command.value("control_id"), 512); require(!commandIds.contains(id)); commandIds.insert(id);
        require(command.value("unit") == "m3/h"); result.commands.push_back({s(id), number(command.value("value"))});
    }
    const auto coverage = object(document.value("coverage")); keys(coverage, {"required_sensor_ids", "missing_sensor_ids", "conflicting_sensor_ids"});
    result.coverage = {identifiers(coverage.value("required_sensor_ids")), identifiers(coverage.value("missing_sensor_ids")), identifiers(coverage.value("conflicting_sensor_ids"))};
    const auto asSet = [](const std::vector<std::string>& list) { QSet<QString> values; for (const auto& id : list) values.insert(QString::fromStdString(id)); return values; };
    const auto required = asSet(result.coverage.required_sensor_ids), missing = asSet(result.coverage.missing_sensor_ids), conflicts = asSet(result.coverage.conflicting_sensor_ids);
    QSet<QString> sampleIds; for (auto it = samples.begin(); it != samples.end(); ++it) sampleIds.insert(it.key());
    require((missing - required).isEmpty() && (missing & sampleIds).isEmpty() && (required - sampleIds - missing).isEmpty(), "Snapshot coverage is inconsistent.");
    if (context.contains("configuration")) for (const auto& id : required | missing | conflicts | sampleIds) (void)findSensor(configuration, id);
    if (result.availability == "available") {
        require(missing.isEmpty() && conflicts.isEmpty(), "Available snapshots have missing or conflicting coverage.");
        double earliest = std::numeric_limits<double>::infinity(), latest = -earliest;
        for (const auto& id : required) {
            const auto sample = samples.value(id);
            require(sample.value("receiver_status") == "good" && sample.value("reported_quality") == "good", "Available snapshot requires good required samples.");
            if (context.contains("configuration")) require(mappingSourceAvailable(findSensor(configuration, id), sample), "Unavailable sensor mapping cannot provide available required coverage.");
            const auto time = sample.value("event_time_s").toDouble(); earliest = std::min(earliest, time); latest = std::max(latest, time);
        }
        if (context.contains("configuration") && !required.isEmpty()) require(latest - earliest <= configuration.value("policy").toObject().value("max_skew_s").toDouble(), "Snapshot observations are incoherent.");
    }
    for (const auto& item : array(document.value("assumptions"), 0, 64)) result.assumptions.push_back(s(text(item, 2000)));
    QSet<QString> derivedBindings; const auto network = contextNetwork(context);
    const QMap<QString, QString> units{{"inventory", "m3"}, {"level", "m"}, {"source_flow", "m3/h"}, {"route_flow", "m3/h"}, {"pump_state", "state"}, {"concentration", "mg/L"}};
    for (const auto& item : array(document.value("derived_state"), 0, 512)) {
        const auto state = object(item); keys(state, {"quantity", "attachment_id", "value", "unit", "origin", "source_sensor_ids"});
        dto::DerivedState decoded;
        const auto quantity = enumeration(state.value("quantity"), {"inventory", "level", "source_flow", "route_flow", "pump_state", "concentration", "pressure", "temperature", "custom_numeric"});
        require(units.contains(quantity), "This quantity has no G1 forecast mapping."); decoded.quantity = s(quantity);
        const auto attachment = reference(state.value("attachment_id")), key = quantity + '\n' + attachment;
        require(!derivedBindings.contains(key), "Competing derived-state assignments."); derivedBindings.insert(key); decoded.attachment_id = s(attachment);
        decoded.unit = s(text(state.value("unit"), 32)); require(QString::fromStdString(decoded.unit) == units.value(quantity));
        decoded.value = value(state.value("value"), quantity == "pump_state");
        decoded.origin = s(enumeration(state.value("origin"), {"measured", "converted", "inferred", "assumed"}));
        decoded.source_sensor_ids = identifiers(state.value("source_sensor_ids"));
        for (const auto& id : decoded.source_sensor_ids)
            require(!conflicts.contains(QString::fromStdString(id)), "Conflicting observations cannot supply a derived-state assignment.");
        if (context.contains("configuration")) for (const auto& id : decoded.source_sensor_ids) {
            const auto sensorId = QString::fromStdString(id);
            require(samples.contains(sensorId), "Derived state references an absent observation.");
            const auto sensor = findSensor(configuration, sensorId);
            require(mappingSourceAvailable(sensor, samples.value(sensorId)), "Derived state uses an unavailable observation mapping.");
        }
        if (decoded.origin == "measured" || decoded.origin == "converted") {
            require(decoded.source_sensor_ids.size() == 1, "Direct measured/converted state requires exactly one observation.");
            const auto sourceId = QString::fromStdString(decoded.source_sensor_ids.front());
            require(samples.contains(sourceId), "Direct state references an absent observation.");
            const auto sourceSensor = findSensor(configuration, sourceId), sourceSample = samples.value(sourceId);
            require(sourceSensor.value("quantity") == quantity && sourceSensor.value("attachment").toObject().value("id") == attachment &&
                sourceSample.value("canonical_unit") == QString::fromStdString(decoded.unit), "Direct measurement quantity, attachment or unit differs from its source.");
            if (std::holds_alternative<std::string>(decoded.value))
                require(sourceSample.value("canonical_value").isString() && sourceSample.value("canonical_value").toString() == QString::fromStdString(std::get<std::string>(decoded.value)),
                    "Direct feedback state differs from its source.");
            else {
                const auto expected = number(sourceSample.value("canonical_value")), actual = std::get<double>(decoded.value);
                require(std::abs(actual - expected) <= std::max(1e-12, 1e-12 * std::max(std::abs(actual), std::abs(expected))), "Direct state differs from its canonical observation.");
            }
        }
        if (decoded.origin == "assumed") require(!result.assumptions.empty(), "Assumed state needs explicit assumptions.");
        if (decoded.origin == "inferred") require(!decoded.source_sensor_ids.empty() && !result.assumptions.empty(), "Inferred state needs source observations and named assumptions.");
        if (quantity == "inventory" && result.availability == "available" && !network.isEmpty()) {
            bool found = false;
            for (const auto& assetValue : network.value("assets").toArray()) {
                const auto asset = assetValue.toObject();
                if (asset.value("id") == attachment) {
                    found = asset.value("kind") == "tank" && std::get<double>(decoded.value) >= 0 && std::get<double>(decoded.value) <= number(asset.value("capacity_m3"), 0); break;
                }
            }
            require(found, "Out-of-model inventory cannot initialize an available native state.");
        }
        result.derived_state.push_back(std::move(decoded));
    }
    return result;
}

QString snapshot_disposition(const QJsonObject& document, const QJsonObject& context) {
    checkContext(context); common(document, "observed_snapshot");
    const auto mode = enumeration(document.value("mode"), {"synthetic_live", "replay", "historical"});
    if (!context.contains("configuration") || !context.contains("network") || !context.contains("physical_hash"))
        return mode == "synthetic_live" ? "unavailable" : "historical";
    const auto snapshot = parse_observed_snapshot(document, context);
    if (snapshot.mode != "synthetic_live") return "historical";
    const auto clock = context.value("clock").toObject();
    if (clock.isEmpty()) return "unavailable";
    if (!clock.isEmpty()) {
        if (clock.contains("mode") && clock.value("mode") != "synthetic_live") return "historical";
        if (QString::fromStdString(snapshot.stream_id) != clock.value("stream_id") || QString::fromStdString(snapshot.epoch_id) != clock.value("epoch_id")) return "historical";
        const auto now = number(clock.value("now_event_time_s"), 0);
        const auto configuration = contextConfiguration(context);
        const auto allowance = context.contains("configuration") ? parse_sensor_configuration(configuration).policy.future_allowance_s : dto::Policy{}.future_allowance_s;
        if (snapshot.captured_event_time_s > now + allowance) return "unavailable";
        if (now > snapshot.expires_event_time_s) return "expired";
    }
    return QString::fromStdString(snapshot.availability);
}

breakwater::telemetry::DetectionEvent parse_detection_event(const QJsonObject& document, const QJsonObject& context) {
    checkContext(context);
    keys(document, {"telemetry_version", "kind", "event_id", "revision", "physical_hash", "configuration_hash", "stream_id", "epoch_id", "snapshot_id", "detector_id", "detector_version", "threshold_version",
        "status", "severity", "onset_event_time_s", "updated_event_time_s", "cleared_event_time_s", "acknowledgement", "sensor_ids", "asset_ids", "evidence", "diagnosis", "explanation", "actionable"});
    common(document, "detection_event"); inspect(document); bind(document, context);
    dto::DetectionEvent result;
    result.event_id = s(identifier(document.value("event_id"))); result.revision = integer(document.value("revision"), 1);
    result.physical_hash = s(hash(document.value("physical_hash"))); result.configuration_hash = s(hash(document.value("configuration_hash")));
    result.stream_id = s(identifier(document.value("stream_id"))); result.epoch_id = s(identifier(document.value("epoch_id")));
    result.snapshot_id = s(identifier(document.value("snapshot_id"))); result.detector_id = s(identifier(document.value("detector_id")));
    result.detector_version = s(identifier(document.value("detector_version"))); result.threshold_version = s(identifier(document.value("threshold_version")));
    result.status = s(enumeration(document.value("status"), {"active", "cleared"})); result.severity = s(enumeration(document.value("severity"), {"info", "warning", "critical"}));
    result.onset_event_time_s = number(document.value("onset_event_time_s"), 0); result.updated_event_time_s = number(document.value("updated_event_time_s"), 0);
    require(result.onset_event_time_s <= result.updated_event_time_s, "Event update precedes onset.");
    if (!document.value("cleared_event_time_s").isNull()) result.cleared_event_time_s = number(document.value("cleared_event_time_s"), 0);
    require(result.status == "active" ? !result.cleared_event_time_s.has_value() : result.cleared_event_time_s &&
        *result.cleared_event_time_s >= result.onset_event_time_s && *result.cleared_event_time_s <= result.updated_event_time_s,
        "Event clearing state and timestamps disagree.");
    if (!document.value("acknowledgement").isNull()) {
        const auto ack = object(document.value("acknowledgement")); keys(ack, {"actor", "acknowledged_utc"});
        result.acknowledgement = dto::EventAcknowledgement{s(identifier(ack.value("actor"))), s(utc(ack.value("acknowledged_utc")))};
    }
    result.sensor_ids = identifiers(document.value("sensor_ids")); result.asset_ids = identifiers(document.value("asset_ids"), true);
    const auto configuration = contextConfiguration(context);
    if (context.contains("configuration")) for (const auto& id : result.sensor_ids) (void)findSensor(configuration, QString::fromStdString(id));
    for (const auto& item : array(document.value("evidence"), 0, 64)) {
        const auto evidence = object(item); keys(evidence, {"metric", "value", "unit", "threshold"});
        result.evidence.push_back({s(identifier(evidence.value("metric"))), s(text(evidence.value("unit"), 32)), number(evidence.value("value")), number(evidence.value("threshold"))});
    }
    result.diagnosis = s(enumeration(document.value("diagnosis"), {"unresolved", "hypothesis"})); result.explanation = s(text(document.value("explanation"), 2000));
    require(!boolean(document.value("actionable")), "Detection events cannot grant control authority."); return result;
}

void validate_document(const QJsonObject& document, const QString& kind, const QJsonObject& context) {
    require(QJsonDocument(document).toJson(QJsonDocument::Compact).size() <= maximumDocumentBytes); inspect(document); checkContext(context);
    if (kind == "sensor_configuration") (void)parse_sensor_configuration(document, context);
    else if (kind == "stream_frame") (void)parse_stream_frame(document, context);
    else if (kind == "stream_command") (void)parse_stream_command(document);
    else if (kind == "observed_snapshot") (void)parse_observed_snapshot(document, context);
    else if (kind == "detection_event") (void)parse_detection_event(document, context);
    else if (kind == "project_v2") {
        keys(document, {"version", "project", "sensor_configuration", "sensor_layout"}); require(integer(document.value("version"), 2, 2) == 2);
        const auto project = object(document.value("project")); require(project.value("version") == 1 && project.value("network").isObject() && project.value("layout").isObject());
        (void)parse_project(project);
        require(context.value("network").isObject() && context.contains("physical_hash"), "Project v2 requires an explicit registered network/hash context.");
        (void)hash(context.value("physical_hash"));
        require(project.value("network") == context.value("network"), "Project v2 differs from its registered network.");
        auto configContext = context; configContext["project"] = project;
        const auto configuration = object(document.value("sensor_configuration")); (void)parse_sensor_configuration(configuration, configContext);
        const auto embeddedPipes = project.value("pipe_transport").toObject().value("routes").toObject();
        const auto registeredPipes = context.value("pipe_parameters").toObject();
        for (const auto& value : configuration.value("sensors").toArray()) {
            const auto sensor = value.toObject();
            if (sensor.value("quantity") == "concentration") {
                const auto route = sensor.value("attachment").toObject().value("id").toString();
                require(embeddedPipes.contains(route) && registeredPipes.contains(route) && embeddedPipes.value(route) == registeredPipes.value(route),
                    "Project v2 pipe parameters differ from registered measurement geometry.");
            }
        }
        const auto layout = object(document.value("sensor_layout")); require(layout.size() <= 256);
        for (auto it = layout.begin(); it != layout.end(); ++it) {
            (void)identifier(it.key()); (void)findSensor(configuration, it.key()); const auto position = object(it.value()); keys(position, {"x", "y", "z"});
            for (const auto& coordinate : position) (void)number(coordinate);
        }
    } else throw std::invalid_argument("Unsupported telemetry document kind.");
}

} // namespace breakwater::desktop::telemetry

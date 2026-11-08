#include "WorkerProtocol.hpp"
#include "ProjectDocument.hpp"

#include <QJsonArray>
#include <QRegularExpression>
#include <QSet>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace breakwater::desktop::science::protocol {
namespace {
void require(bool condition) { if (!condition) throw std::invalid_argument("Invalid scientific protocol data."); }
void keys(const QJsonObject& object, const QSet<QString>& fields) {
    require(object.size() == fields.size());
    for (const auto& field : fields) require(object.contains(field));
}
double number(const QJsonValue& value) {
    require(value.isDouble() && std::isfinite(value.toDouble())); return value.toDouble();
}
qint64 integer(const QJsonValue& value, qint64 low, qint64 high) {
    const auto n = number(value);
    require(n >= static_cast<double>(low) && n <= static_cast<double>(high) && std::floor(n) == n);
    return static_cast<qint64>(n);
}
void text(const QJsonValue& value, qsizetype maximum, bool empty = false) {
    require(value.isString()); const auto string = value.toString();
    // JSON Schema measures Unicode scalar characters, rather than UTF-16 units.
    require((empty || !string.isEmpty()) && string.size() <= 2 * maximum &&
        QString::fromUtf8(string.toUtf8()) == string && string.toUcs4().size() <= maximum);
}
void identity(const QJsonObject& object) {
    keys(object, {"snapshot_id", "volume_run_id", "numerical_revision", "physical_hash"});
    text(object.value("snapshot_id"), 256); text(object.value("volume_run_id"), 256);
    (void)integer(object.value("numerical_revision"), 0, 9007199254740991LL);
    static const QRegularExpression hash(QStringLiteral("^[0-9a-f]{64}$"));
    require(object.value("physical_hash").isString() && hash.match(object.value("physical_hash").toString()).hasMatch());
}
QJsonArray array(const QJsonValue& value, qsizetype minimum, qsizetype maximum) {
    require(value.isArray()); const auto values = value.toArray();
    require(values.size() >= minimum && values.size() <= maximum); return values;
}
void numbers(const QJsonValue& value, qsizetype count) {
    for (const auto& item : array(value, count, count)) (void)number(item);
}
std::vector<double> requestedTimes(double horizon, double step) {
    // Independently enforce the adopted wire policy. Rounded division is not a
    // work count: it can omit a terminal ULP, invent a duplicate, or underflow.
    constexpr std::size_t maximumUpdates = 10000;
    require(std::isfinite(horizon) && std::isfinite(step) && horizon > 0.0 && horizon <= 24.0 && step > 0.0);
    require(horizon <= static_cast<double>(maximumUpdates) * step);
    std::vector<double> times{0.0};
    for (std::size_t index = 1; index <= maximumUpdates; ++index) {
        const auto time = static_cast<double>(index) * step;
        if (time >= horizon) { times.push_back(horizon); return times; }
        require(time > times.back());
        times.push_back(time);
    }
    throw std::invalid_argument("Invalid scientific protocol data.");
}
} // namespace

bool storagePayload(const QJsonObject& payload) {
    try {
        keys(payload, {"method_version", "identity", "network", "controls", "horizon_h", "step_h", "derivatives", "preview"});
        require(integer(payload.value("method_version"), 1, 1) == 1 && payload.value("identity").isObject());
        identity(payload.value("identity").toObject());
        const auto horizon = number(payload.value("horizon_h")), step = number(payload.value("step_h"));
        (void)requestedTimes(horizon, step);
        require(payload.value("derivatives").isBool());
        const bool derivatives = payload.value("derivatives").toBool();
        require(payload.value("preview").isObject()); const auto preview = payload.value("preview").toObject();
        keys(preview, {"max_times"}); (void)integer(preview.value("max_times"), 2, 256);
        require(payload.value("network").isObject());
        const auto network = parse_project({{"version", 1}, {"network", payload.value("network")}, {"layout", QJsonObject{}}}).network;
        require(network.assets.size() <= 128 && network.routes.size() <= 256);
        std::vector<ControlValue> controls; QSet<QString> identifiers;
        for (const auto& value : array(payload.value("controls"), derivatives ? 1 : 0, 384)) {
            require(value.isObject()); const auto control = value.toObject(); keys(control, {"control_id", "value"});
            text(control.value("control_id"), 512); const auto id = control.value("control_id").toString();
            require(!identifiers.contains(id)); identifiers.insert(id);
            controls.push_back({id.toStdString(), number(control.value("value"))});
        }
        // Validate the captured draft independently before applying any override.
        (void)compile(network); (void)compile(network, controls);
        if (derivatives) {
            const auto catalog = control_catalog(network);
            for (const auto& control : controls) {
                const auto descriptor = std::find_if(catalog.begin(), catalog.end(), [&](const ControlDescriptor& value) { return value.id == control.id; });
                require(descriptor != catalog.end() && descriptor->minimum < descriptor->maximum &&
                    control.value > descriptor->minimum && control.value < descriptor->maximum);
                const auto source = std::find_if(network.assets.begin(), network.assets.end(), [&](const Asset& value) {
                    return value.id == descriptor->asset_id && value.kind == AssetKind::source;
                });
                if (source != network.assets.end()) {
                    const auto feed = std::find_if(network.routes.begin(), network.routes.end(), [&](const Route& value) { return value.source_id == source->id; });
                    require(feed != network.routes.end() && source->inflow_m3h - control.value < feed->capacity_m3h);
                }
            }
        }
        return true;
    } catch (const std::exception&) { return false; }
}

QJsonObject storageCapability(const QJsonObject& result) {
    for (const auto& value : result.value("methods").toArray())
        if (value.toObject().value("name").toString() == "storage_rollout") return value.toObject();
    return {};
}

bool capabilities(const QJsonObject& result) {
    try {
        keys(result, {"capability_version", "methods", "max_line_bytes", "max_preview_times", "max_preview_cells", "artifacts_supported"});
        (void)integer(result.value("capability_version"), 1, 1);
        (void)integer(result.value("max_line_bytes"), 1, 1024 * 1024);
        (void)integer(result.value("max_preview_times"), 2, 256);
        (void)integer(result.value("max_preview_cells"), 2, 128);
        require(result.value("artifacts_supported").isBool() && !result.value("artifacts_supported").toBool());
        QSet<QString> methods;
        const QSet<QString> supported{"health", "capabilities", "storage_rollout", "pipe_transport"};
        for (const auto& value : array(result.value("methods"), 4, 4)) {
            require(value.isObject()); const auto method = value.toObject();
            keys(method, {"name", "method_version", "implemented", "execution_verified"});
            const auto name = method.value("name").toString();
            require(supported.contains(name) && !methods.contains(name)); methods.insert(name);
            (void)integer(method.value("method_version"), 1, 1);
            require(method.value("implemented").isBool() && method.value("execution_verified").isBool());
            const bool implemented = method.value("implemented").toBool(), verified = method.value("execution_verified").toBool();
            require(!verified || implemented);
            if (name == "health" || name == "capabilities") require(implemented && verified);
            if (name == "pipe_transport") require(!implemented && !verified);
        }
        return true;
    } catch (const std::exception&) { return false; }
}

bool storage(const QJsonObject& result, const QJsonObject& payload) {
    try {
        require(payload.value("derivatives").isBool());
        const bool derivatives = payload.value("derivatives").toBool();
        QSet<QString> fields{"method", "method_version", "identity", "provenance", "time_h", "time_indices", "tank_ids", "volumes_m3", "cost", "warnings"};
        if (derivatives) fields.insert("derivatives");
        keys(result, fields);
        require(result.value("method").toString() == "storage_rollout"); (void)integer(result.value("method_version"), 1, 1);
        require(result.value("identity").isObject() && result.value("identity") == payload.value("identity"));
        require(result.value("provenance").isObject()); const auto provenance = result.value("provenance").toObject();
        keys(provenance, {"implementation", "implementation_version", "dtype", "device", "execution_status"});
        require(provenance.value("implementation").toString() == "torch_storage_float64" && provenance.value("dtype").toString() == "float64" &&
            provenance.value("device").toString() == "cpu" && provenance.value("execution_status").toString() == "completed");
        text(provenance.value("implementation_version"), 128);
        const auto maximum = payload.value("preview").toObject().value("max_times").toInt();
        const auto times = array(result.value("time_h"), 2, maximum), indices = array(result.value("time_indices"), times.size(), times.size());
        const auto horizon = payload.value("horizon_h").toDouble(), step = payload.value("step_h").toDouble();
        const auto expectedTimes = requestedTimes(horizon, step);
        const auto lastIndex = static_cast<qint64>(expectedTimes.size() - 1);
        qint64 previousIndex = -1; double previousTime = -1.0;
        for (qsizetype i = 0; i < times.size(); ++i) {
            const auto index = integer(indices[i], 0, lastIndex);
            const auto time = number(times[i]);
            require(index > previousIndex && time > previousTime && time == expectedTimes[static_cast<std::size_t>(index)]);
            previousIndex = index; previousTime = time;
        }
        require(indices.first().toInteger() == 0 && indices.last().toInteger() == lastIndex &&
            times.first().toDouble() == 0.0 && times.last().toDouble() == horizon);
        QJsonArray tankIds;
        for (const auto& value : payload.value("network").toObject().value("assets").toArray())
            if (value.toObject().value("kind").toString() == "tank") tankIds.append(value.toObject().value("id"));
        require(!tankIds.isEmpty() && result.value("tank_ids").isArray() && result.value("tank_ids").toArray() == tankIds);
        for (const auto& row : array(result.value("volumes_m3"), tankIds.size(), tankIds.size())) numbers(row, times.size());
        require(number(result.value("cost")) >= 0.0);
        for (const auto& warning : array(result.value("warnings"), 0, 32)) text(warning, 1000, true);
        if (derivatives) {
            QJsonArray controlIds;
            for (const auto& value : array(payload.value("controls"), 1, 384)) controlIds.append(value.toObject().value("control_id"));
            require(result.value("derivatives").isObject());
            const auto derivative = result.value("derivatives").toObject();
            keys(derivative, {"control_ids", "terminal_volume_jacobian_h", "cost_gradient"});
            require(derivative.value("control_ids").isArray() && derivative.value("control_ids").toArray() == controlIds);
            for (const auto& row : array(derivative.value("terminal_volume_jacobian_h"), tankIds.size(), tankIds.size())) numbers(row, controlIds.size());
            numbers(derivative.value("cost_gradient"), controlIds.size());
        }
        return true;
    } catch (const std::exception&) { return false; }
}

bool publicErrorCode(const QString& code) {
    static const QSet<QString> codes{"not_implemented", "invalid_request", "unsupported_version", "unsupported_method", "invalid_network",
        "invalid_controls", "invalid_parameters", "identity_mismatch", "runtime_unavailable", "output_too_large", "artifact_unavailable",
        "work_limit", "numerical_failure", "cancelled"};
    return codes.contains(code);
}
QString publicErrorMessage(const QString& code) {
    if (code == "runtime_unavailable") return QStringLiteral("The configured scientific runtime or storage modules are unavailable.");
    if (code == "not_implemented" || code == "unsupported_method" || code == "unsupported_version") return QStringLiteral("The worker does not support the requested scientific operation.");
    if (code == "work_limit" || code == "output_too_large") return QStringLiteral("The scientific calculation exceeded its work or output budget.");
    if (code == "cancelled") return QStringLiteral("The scientific calculation was cancelled.");
    return QStringLiteral("The scientific worker rejected or could not complete this captured calculation.");
}

} // namespace breakwater::desktop::science::protocol

#include "StorageScienceAnalysis.hpp"
#include <QDateTime>
#include <QJsonArray>
#include <QUuid>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace breakwater::desktop::science {
namespace {
QJsonObject identity(const CapturedForecast& run) {
    return {{"snapshot_id", run.id}, {"volume_run_id", run.id}, {"physical_hash", run.physical_hash},
        {"numerical_revision", static_cast<qint64>(run.revision)}};
}
QJsonObject payload(const CapturedForecast& run, bool sensitivities, const QJsonArray& controls) {
    return {{"method_version", 1}, {"identity", identity(run)}, {"network", network_json(run.network)},
        {"controls", controls}, {"horizon_h", run.config.horizon_h}, {"step_h", run.config.step_h},
        {"derivatives", sensitivities}, {"preview", QJsonObject{{"max_times", 256}}}};
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
QString ineligibleReason(const Network& network, const ControlDescriptor& control) {
    if (!std::isfinite(control.value) || !std::isfinite(control.minimum) || !std::isfinite(control.maximum))
        return QStringLiteral("Captured control bounds/value are not finite.");
    if (control.maximum <= control.minimum) return QStringLiteral("Fixed or unavailable control.");
    if (control.value <= control.minimum || control.value >= control.maximum)
        return QStringLiteral("Captured value lies on a control bound.");
    const auto asset = std::find_if(network.assets.begin(), network.assets.end(),
        [&](const Asset& value) { return value.id == control.asset_id; });
    if (asset != network.assets.end() && asset->kind == AssetKind::source) {
        const auto feed = std::find_if(network.routes.begin(), network.routes.end(),
            [&](const Route& value) { return value.source_id == asset->id; });
        if (feed == network.routes.end()) return QStringLiteral("Captured source feed is unavailable.");
        if (asset->inflow_m3h - control.value >= feed->capacity_m3h)
            return QStringLiteral("Captured source flow is on the feed-capacity boundary.");
    }
    return {};
}
QJsonArray selectedMetadata(const CapturedForecast& run, const QJsonArray& controls) {
    require(!controls.isEmpty() && controls.size() <= 384, "Select at least one captured interior control for sensitivities.");
    const auto catalog = control_catalog(run.network);
    QJsonArray selected;
    std::ptrdiff_t previous = -1;
    for (const auto& value : controls) {
        require(value.isObject(), "Scientific sensitivity selection is invalid.");
        const auto item = value.toObject();
        require(item.size() == 2 && item.value("control_id").isString() && item.contains("value"),
            "Scientific sensitivity selection is invalid.");
        const auto id = item.value("control_id").toString();
        const auto control = std::find_if(catalog.begin(), catalog.end(),
            [&](const ControlDescriptor& descriptor) { return QString::fromStdString(descriptor.id) == id; });
        require(control != catalog.end(), "Scientific sensitivity selection names an unknown captured control.");
        const auto index = std::distance(catalog.begin(), control);
        require(index > previous && finiteNumber(item.value("value")) == control->value && ineligibleReason(run.network, *control).isEmpty(),
            "Scientific sensitivity selection must retain unchanged interior captured values in catalog order.");
        previous = index;
        selected.append(QJsonObject{{"control_id", id}, {"captured_value", control->value}, {"unit", QString::fromStdString(control->unit)}});
    }
    return selected;
}
QJsonObject checkedDerivatives(const QJsonObject& worker, const QJsonArray& selected, qsizetype tankCount) {
    require(worker.value("derivatives").isObject(), "Scientific sensitivity output is missing its derivatives.");
    const auto derivative = worker.value("derivatives").toObject();
    require(derivative.size() == 3 && derivative.contains("control_ids") && derivative.contains("terminal_volume_jacobian_h") &&
        derivative.contains("cost_gradient"), "Scientific sensitivity output contains unsupported derivative fields.");
    QJsonArray expectedIds;
    for (const auto& value : selected) expectedIds.append(value.toObject().value("control_id"));
    require(derivative.value("control_ids").isArray() && derivative.value("control_ids").toArray() == expectedIds,
        "Scientific derivative columns do not match the selected captured controls.");
    require(derivative.value("terminal_volume_jacobian_h").isArray() && derivative.value("cost_gradient").isArray(),
        "Scientific derivative dimensions are invalid.");
    const auto matrix = derivative.value("terminal_volume_jacobian_h").toArray(), gradient = derivative.value("cost_gradient").toArray();
    require(matrix.size() == tankCount && gradient.size() == selected.size(), "Scientific derivative dimensions are invalid.");
    for (const auto& value : matrix) {
        require(value.isArray(), "Scientific derivative dimensions are invalid.");
        const auto row = value.toArray();
        require(row.size() == selected.size(), "Scientific derivative dimensions are invalid.");
        for (const auto& coefficient : row) (void)finiteNumber(coefficient);
    }
    for (const auto& coefficient : gradient) (void)finiteNumber(coefficient);
    return derivative;
}
QJsonObject compare(const std::shared_ptr<const CapturedForecast>& run, const QJsonObject& response,
                    bool sensitivities, const QJsonArray& controls) {
    const auto worker = response.value("result").toObject();
    require(response.value("status").toString() == "completed" && worker.value("identity").toObject() == identity(*run),
        "Scientific response does not bind to this captured native forecast.");
    require(worker.value("method").toString() == "storage_rollout" && worker.value("method_version").toDouble() == 1.0 &&
        !worker.contains("artifact"), "Scientific response contains unsupported analysis output.");
    const auto selected = sensitivities ? selectedMetadata(*run, controls) : QJsonArray{};
    require(sensitivities || (controls.isEmpty() && !worker.contains("derivatives")),
        "Forward scientific output contains unrequested sensitivity data.");
    const auto times = worker.value("time_h").toArray(), indices = worker.value("time_indices").toArray();
    const auto ids = worker.value("tank_ids").toArray(), volumes = worker.value("volumes_m3").toArray();
    const auto& native = run->result;
    require(times.size() >= 2 && times.size() <= 256 && indices.size() == times.size() &&
        ids.size() == static_cast<qsizetype>(native.tank_ids.size()) && volumes.size() == ids.size(),
        "Scientific preview dimensions disagree with the native reference.");
    require(indices.first().toInteger(-1) == 0 && indices.last().toInteger(-1) == static_cast<qint64>(native.time_h.size() - 1),
        "Scientific preview does not retain the captured native endpoints.");
    const auto derivative = sensitivities ? checkedDerivatives(worker, selected, ids.size()) : QJsonObject{};
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
    QJsonObject evidence{{"result_version", 1}, {"result_id", QUuid::createUuid().toString(QUuid::WithoutBraces)},
        {"created_at", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}, {"method", "storage_rollout"},
        {"run_id", run->id}, {"snapshot_id", run->id}, {"physical_hash", run->physical_hash},
        {"numerical_revision", static_cast<qint64>(run->revision)}, {"horizon_h", run->config.horizon_h}, {"step_h", run->config.step_h},
        {"analysis_mode", sensitivities ? "sensitivities" : "forward"}, {"selected_controls", selected},
        {"native_solver_version", QString::fromLatin1(breakwater::solver_version)},
        {"native_cost", native.cost}, {"worker_cost", workerCost}, {"cost_difference", workerCost - native.cost},
        {"cost_absolute_error", absoluteCost}, {"cost_scaled_error", costScaled}, {"within_tolerance", agrees},
        {"comparison_scope", "Returned worker preview samples versus their original native indices; not a feasibility verdict"},
        {"absolute_tolerance", 1e-9}, {"relative_tolerance", 1e-10}, {"native_volume_feasible", run->validation.feasible},
        {"tanks", descriptors}, {"tank_errors", errors}, {"provenance", worker.value("provenance")}, {"warnings", worker.value("warnings")},
        {"preview", QJsonObject{{"time_h", times}, {"time_indices", indices}, {"native_volumes_m3", nativePreview},
            {"worker_volumes_m3", volumes}, {"source_frames", static_cast<qint64>(native.time_h.size())}}}};
    if (sensitivities) evidence["derivatives"] = derivative;
    return evidence;
}
}

QString sensitivity_ineligible_reason(const Network& network, const ControlDescriptor& control) {
    return ineligibleReason(network, control);
}
QJsonObject storage_science_payload(const CapturedForecast& capture, bool sensitivities, const QJsonArray& controls) {
    return payload(capture, sensitivities, controls);
}
QJsonObject compare_storage_science(const std::shared_ptr<const CapturedForecast>& capture,
    const QJsonObject& workerResponse, bool sensitivities, const QJsonArray& controls) {
    return compare(capture, workerResponse, sensitivities, controls);
}
} // namespace breakwater::desktop::science
